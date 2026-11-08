#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class QUdpSocket;

// Finds tables on the same Wi-Fi network with a small UDP broadcast. Some
// access points isolate clients or block broadcast; direct-address play is the
// fallback and is documented in the README.
class Discovery : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)

public:
    explicit Discovery(QObject *parent = nullptr);

    void startAdvertising(const QString &name, quint16 port, int rules, const QString &status);
    void updateStatus(const QString &status);
    void stopAdvertising();

    void startBrowsing();
    void stopBrowsing();
    bool browsing() const;

    QVariantList games() const;

signals:
    void gamesChanged();

private:
    struct Entry {
        QString name;
        QString address;
        QString status;
        int rules = 0;
        qint64 seen = 0;
    };

    void announce();
    void readPending();
    void prune();

    QUdpSocket *m_sender = nullptr;
    QUdpSocket *m_listener = nullptr;
    QTimer m_advertiseTimer;
    QTimer m_pruneTimer;

    QString m_name;
    QString m_status = QStringLiteral("lobby");
    quint16 m_port = 0;
    int m_rules = 0;
    QHash<QString, Entry> m_entries;
};
