#include "discovery.h"

#include "protocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QVariantMap>
#include <algorithm>

Discovery::Discovery(QObject *parent) : QObject(parent)
{
    m_advertiseTimer.setInterval(2000);
    connect(&m_advertiseTimer, &QTimer::timeout, this, &Discovery::announce);

    m_pruneTimer.setInterval(2000);
    connect(&m_pruneTimer, &QTimer::timeout, this, &Discovery::prune);
}

void Discovery::startAdvertising(const QString &name, quint16 port, int rules, const QString &status)
{
    if (!m_sender) {
        m_sender = new QUdpSocket(this);
        m_sender->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    }
    m_name = name;
    m_port = port;
    m_rules = rules;
    m_status = status;
    announce();
    m_advertiseTimer.start();
}

void Discovery::updateStatus(const QString &status)
{
    m_status = status;
    if (m_advertiseTimer.isActive())
        announce();
}

void Discovery::stopAdvertising()
{
    m_advertiseTimer.stop();
    m_port = 0;
}

void Discovery::startBrowsing()
{
    if (m_listener) {
        if (m_listener->state() != QAbstractSocket::BoundState) {
            if (!m_listener->bind(QHostAddress::AnyIPv4, Protocol::DiscoveryPort,
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
                delete m_listener;
                m_listener = nullptr;
            }
        }
    }
    if (!m_listener) {
        m_listener = new QUdpSocket(this);
        if (!m_listener->bind(QHostAddress::AnyIPv4, Protocol::DiscoveryPort,
                              QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            delete m_listener;
            m_listener = nullptr;
            return;
        }
        connect(m_listener, &QUdpSocket::readyRead, this, &Discovery::readPending);
    }
    m_pruneTimer.start();
}

void Discovery::stopBrowsing()
{
    m_pruneTimer.stop();
    if (m_listener) {
        m_listener->close();
        delete m_listener;
        m_listener = nullptr;
    }
    if (!m_entries.isEmpty()) {
        m_entries.clear();
        emit gamesChanged();
    }
}

bool Discovery::browsing() const
{
    return m_listener != nullptr;
}

void Discovery::announce()
{
    if (!m_sender || m_port == 0)
        return;

    QByteArray payload = QJsonDocument(QJsonObject{
        {"rack", 1},
        {"name", m_name},
        {"port", int(m_port)},
        {"rules", m_rules},
        {"status", m_status}
    }).toJson(QJsonDocument::Compact);

    QList<QHostAddress> targets{QHostAddress::Broadcast};
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const auto &entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                targets.append(entry.broadcast());
        }
    }

    QList<QHostAddress> sent;
    for (const auto &address : targets) {
        if (sent.contains(address))
            continue;
        sent.append(address);
        m_sender->writeDatagram(payload, address, Protocol::DiscoveryPort);
    }
}

void Discovery::readPending()
{
    if (!m_listener)
        return;

    bool changed = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (m_listener->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_listener->receiveDatagram();
        if (!datagram.isValid())
            continue;

        const auto document = QJsonDocument::fromJson(datagram.data());
        if (!document.isObject())
            continue;
        const QJsonObject object = document.object();
        if (object.value("rack").toInt() != 1)
            continue;

        const int port = object.value("port").toInt();
        if (port <= 0 || port > 65535)
            continue;

        Entry entry;
        entry.name = object.value("name").toString().left(40);
        if (entry.name.isEmpty())
            entry.name = tr("Rack table");
        entry.address = datagram.senderAddress().toString() + ":" + QString::number(port);
        entry.status = object.value("status").toString();
        entry.rules = object.value("rules").toInt();
        entry.seen = now;

        const auto existing = m_entries.constFind(entry.address);
        if (existing == m_entries.constEnd() || existing->name != entry.name
            || existing->status != entry.status || existing->rules != entry.rules) {
            changed = true;
        }
        m_entries.insert(entry.address, entry);
    }
    if (changed)
        emit gamesChanged();
}

void Discovery::prune()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (now - it->seen > 7000) {
            it = m_entries.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed)
        emit gamesChanged();
}

QVariantList Discovery::games() const
{
    QList<Entry> entries = m_entries.values();
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    QVariantList list;
    for (const auto &entry : entries) {
        QVariantMap map;
        map.insert("name", entry.name);
        map.insert("address", entry.address);
        map.insert("status", entry.status);
        map.insert("rules", entry.rules);
        list.append(map);
    }
    return list;
}
