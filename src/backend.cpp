#include "backend.h"

#include "discovery.h"
#include "game.h"
#include "gameclient.h"
#include "hostserver.h"
#include "protocol.h"
#include "sound.h"

#include <QCoreApplication>
#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QVariantMap>

namespace {
QVariantMap playerEntry(const QString &name, bool host, bool you, bool ready, const QString &omarchy)
{
    QVariantMap map;
    map.insert("name", name);
    map.insert("host", host);
    map.insert("you", you);
    map.insert("ready", ready);
    map.insert("omarchy", omarchy);
    return map;
}

QJsonObject playerJson(const QString &name, bool host, bool you, bool ready, const QString &omarchy)
{
    return {{"name", name}, {"host", host}, {"you", you}, {"ready", ready}, {"omarchy", omarchy}};
}
}

Backend::Backend(QObject *parent)
    : QObject(parent),
      m_settings(QSettings::IniFormat, QSettings::UserScope, "rack", "rack")
{
    m_attestation = Omarchy::detect();
    m_playerName = m_settings.value("name").toString().trimmed();
    if (m_playerName.isEmpty())
        m_playerName = qEnvironmentVariable("USER", tr("Player")).left(24);
    m_rules = qBound(0, m_settings.value("rules", 0).toInt(), 1);

    m_game = new Game(this);
    m_sound = new Sound(this);
    m_host = new HostServer(this);
    m_client = new GameClient(this);
    m_discovery = new Discovery(this);

    m_sound->setMuted(m_settings.value("muted", false).toBool());
    m_sound->setVolume(m_settings.value("volume", 0.6).toDouble());
    connect(m_sound, &Sound::mutedChanged, this, [this] {
        m_settings.setValue("muted", m_sound->muted());
    });
    connect(m_sound, &Sound::volumeChanged, this, [this] {
        m_settings.setValue("volume", m_sound->volume());
    });

    connect(m_game, &Game::soundCue, m_sound, &Sound::play);
    connect(m_game, &Game::changed, this, [this] {
        updateRemoteAimVisibility();
        emit stateChanged();
        if (m_role == Host && m_matchRunning)
            broadcastState();
    });
    connect(m_game, &Game::matchEnded, this, [this](int winner, const QString &reason) {
        Q_UNUSED(winner)
        appendSystem(reason);
    });

    connect(m_discovery, &Discovery::gamesChanged, this, &Backend::discoveredGamesChanged);

    // Global shortcuts must stand down while a text field has focus. The
    // focused object is the control's input item, so match on its type rather
    // than reaching into the QML scene from C++.
    connect(qApp, &QGuiApplication::focusObjectChanged, this, [this](QObject *object) {
        const QString type = object ? QString::fromLatin1(object->metaObject()->className()) : QString();
        const bool typing = type.contains(QLatin1String("TextInput"))
            || type.contains(QLatin1String("TextField"))
            || type.contains(QLatin1String("TextEdit"))
            || type.contains(QLatin1String("TextArea"));
        if (typing == m_typing)
            return;
        m_typing = typing;
        emit typingChanged();
    });

    m_remoteAimTimer.setSingleShot(true);
    m_remoteAimTimer.setInterval(2600);
    connect(&m_remoteAimTimer, &QTimer::timeout, this, &Backend::updateRemoteAimVisibility);

    connectHostSignals();
    connectClientSignals();

    if (!m_attestation.omarchy) {
        m_screen = QStringLiteral("blocked");
        m_status = m_attestation.reason;
    } else {
        m_screen = QStringLiteral("home");
    }
}

Backend::~Backend()
{
    resetSession();
    m_settings.sync();
}

QObject *Backend::gameObject() { return m_game; }
QObject *Backend::soundObject() { return m_sound; }
Game *Backend::game() const { return m_game; }
Sound *Backend::sound() const { return m_sound; }

QString Backend::omarchyDetail() const
{
    if (!m_attestation.omarchy)
        return m_attestation.reason;
    return tr("Omarchy %1 · Hyprland %2").arg(m_attestation.version, m_attestation.hyprland);
}

bool Backend::myTurn() const
{
    if (m_screen == QLatin1String("blocked") || !m_game)
        return false;
    if (m_game->winner() >= 0)
        return false;
    const int me = m_game->myPlayer();
    if (me >= 0 && m_game->currentPlayer() != me)
        return false;
    return m_game->phase() == Game::Aiming || m_game->phase() == Game::BallInHand;
}

QString Backend::rulesName() const
{
    if (m_game && !m_game->player0().isEmpty() && m_screen == QLatin1String("game"))
        return m_game->rulesName();
    return m_rules == Game::EightBall ? tr("8-Ball") : tr("9-Ball");
}

QVariantList Backend::discoveredGames() const
{
    return m_discovery ? m_discovery->games() : QVariantList();
}

void Backend::setPlayerName(const QString &name)
{
    const QString trimmed = name.trimmed().left(24);
    if (trimmed.isEmpty() || trimmed == m_playerName)
        return;
    m_playerName = trimmed;
    m_settings.setValue("name", m_playerName);
    emit playerNameChanged();
    emit stateChanged();
}

void Backend::setRules(int rules)
{
    rules = qBound(0, rules, 1);
    if (m_rules == rules)
        return;
    m_rules = rules;
    m_settings.setValue("rules", m_rules);
    emit stateChanged();
}

void Backend::setJoinAddress(const QString &address)
{
    if (m_joinAddress == address)
        return;
    m_joinAddress = address;
    emit stateChanged();
}

void Backend::setScreen(const QString &screen)
{
    if (m_screen == screen)
        return;
    m_screen = screen;
    emit stateChanged();
}

void Backend::setStatus(const QString &status, const QString &error)
{
    const bool statusChangedNow = (m_status != status);
    const bool errorChangedNow = (m_error != error);
    m_status = status;
    m_error = error;
    if (statusChangedNow || errorChangedNow)
        emit statusChanged();
}

void Backend::clearError()
{
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit statusChanged();
}

void Backend::copyAddress()
{
    if (m_address.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_address);
    setStatus(tr("Address copied: %1").arg(m_address));
}

void Backend::appendChat(const QString &name, const QString &text, bool mine)
{
    if (text.trimmed().isEmpty())
        return;
    QVariantMap entry;
    entry.insert("name", name);
    entry.insert("text", text.trimmed());
    entry.insert("mine", mine);
    entry.insert("system", false);
    m_chatLog.append(entry);
    while (m_chatLog.size() > 60)
        m_chatLog.removeFirst();
    emit chatChanged();
}

void Backend::appendSystem(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    QVariantMap entry;
    entry.insert("name", QString());
    entry.insert("text", text);
    entry.insert("mine", false);
    entry.insert("system", true);
    m_chatLog.append(entry);
    while (m_chatLog.size() > 60)
        m_chatLog.removeFirst();
    emit chatChanged();
}

void Backend::resetSession()
{
    const bool hadClient = m_role == Client;
    m_role = None;
    m_matchRunning = false;
    m_practice = false;
    m_connecting = false;
    if (hadClient)
        m_client->disconnectFromHost();
    m_host->stop();
    m_discovery->stopAdvertising();
    m_discovery->stopBrowsing();
    if (m_game)
        m_game->clear();
    m_guestPresent = false;
    m_guestReady = false;
    m_guestPeer = -1;
    m_guestName.clear();
    m_guestAttestation = {};
    m_hostAttestation = {};
    m_hostName.clear();
    m_roomName.clear();
    m_address.clear();
    m_lobbyPlayers.clear();
    m_chatLog.clear();
    m_remoteAimVisible = false;
    m_remoteAim = 0;
    m_rematchWanted = false;
    emit lobbyChanged();
    emit chatChanged();
    emit remoteAimChanged();
    emit stateChanged();
}

void Backend::startLocal(int rules)
{
    if (!m_attestation.omarchy)
        return;
    resetSession();
    m_role = Local;
    m_rules = qBound(0, rules, 1);
    m_screen = QStringLiteral("game");
    m_status.clear();
    m_error.clear();
    m_game->start(static_cast<Game::Rules>(m_rules), m_playerName, tr("Player 2"),
                  QRandomGenerator::global()->generate());
    m_game->setMyPlayer(-1);
    appendSystem(tr("Local table: %1 vs Player 2.").arg(m_playerName));
    emit stateChanged();
    emit statusChanged();
}

void Backend::startPractice(int rules)
{
    if (!m_attestation.omarchy)
        return;
    resetSession();
    m_role = Local;
    m_practice = true;
    m_rules = qBound(0, rules, 1);
    m_screen = QStringLiteral("game");
    m_status.clear();
    m_error.clear();
    m_game->startPractice(static_cast<Game::Rules>(m_rules), m_playerName);
    appendSystem(tr("Practice table. Break when you are ready."));
    emit stateChanged();
    emit statusChanged();
}

void Backend::hostGame(int rules)
{
    if (!m_attestation.omarchy)
        return;
    resetSession();
    m_rules = qBound(0, rules, 1);
    m_role = Host;
    m_hostName = m_playerName;
    m_roomName = tr("%1's table").arg(m_playerName);
    m_hostAttestation = m_attestation.toJson();

    QString error;
    if (!m_host->listen(Protocol::TcpPort, &error)) {
        m_role = None;
        m_screen = QStringLiteral("home");
        setStatus(QString(), tr("Could not host on port %1: %2")
                                .arg(Protocol::TcpPort).arg(error));
        emit stateChanged();
        return;
    }

    m_address = Protocol::localAddress() + ":" + QString::number(Protocol::TcpPort);
    m_discovery->startAdvertising(m_roomName, Protocol::TcpPort, m_rules,
                                  QStringLiteral("lobby"));
    buildHostLobby();
    m_screen = QStringLiteral("lobby");
    setStatus(tr("Waiting for a player on your Wi-Fi."));
    appendSystem(tr("Table open at %1.").arg(m_address));
    emit stateChanged();
}

void Backend::connectHostSignals()
{
    connect(m_host, &HostServer::hello, this, &Backend::onHostHello);
    connect(m_host, &HostServer::ready, this, &Backend::onHostReady);
    connect(m_host, &HostServer::chat, this, &Backend::onHostChat);
    connect(m_host, &HostServer::disconnected, this, &Backend::onHostDisconnected);

    connect(m_host, &HostServer::shot, this, [this](int peer, double angle, double power) {
        if (m_role != Host || !m_matchRunning || peer != m_guestPeer)
            return;
        if (m_game->phase() != Game::Aiming || m_game->currentPlayer() != 1)
            return;
        m_remoteAimTimer.stop();
        if (m_remoteAimVisible) {
            m_remoteAimVisible = false;
            emit remoteAimChanged();
        }
        m_game->shoot(angle, power);
        m_host->broadcast({{"t", "shot"}, {"by", 1}, {"angle", angle}, {"power", power}});
    });

    connect(m_host, &HostServer::place, this, [this](int peer, double x, double y) {
        if (m_role != Host || !m_matchRunning || peer != m_guestPeer)
            return;
        if (m_game->currentPlayer() != 1 || m_game->phase() != Game::BallInHand)
            return;
        m_game->placeCueBall(x, y);
    });

    connect(m_host, &HostServer::aim, this, [this](int peer, double angle) {
        if (m_role != Host || !m_matchRunning || peer != m_guestPeer)
            return;
        if (m_game->currentPlayer() != 1)
            return;
        m_remoteAim = angle;
        m_remoteAimClock.start();
        m_remoteAimTimer.start();
        updateRemoteAimVisibility();
    });

    connect(m_host, &HostServer::rematch, this, [this](int peer) {
        if (m_role != Host || peer != m_guestPeer)
            return;
        m_rematchWanted = true;
        setStatus(tr("%1 wants a rematch.").arg(m_guestName));
        appendSystem(tr("%1 wants a rematch.").arg(m_guestName));
        emit stateChanged();
    });
}

void Backend::connectClientSignals()
{
    connect(m_client, &GameClient::welcomed, this,
            [this](int player, int rules, const QString &host, const QJsonObject &hostAttest) {
        if (m_role != Client)
            return;
        m_connecting = false;
        m_hostName = host;
        m_hostAttestation = hostAttest;
        m_rules = qBound(0, rules, 1);
        m_game->setMyPlayer(player);
        m_roomName = tr("%1's table").arg(host);
        m_discovery->stopBrowsing();
        m_screen = QStringLiteral("lobby");
        setStatus(tr("Connected to %1.").arg(host));
        appendSystem(tr("Connected to %1.").arg(host));
        m_client->setReady(true);
        emit stateChanged();
    });

    connect(m_client, &GameClient::lobbyReceived, this,
            [this](const QJsonArray &players, int rules) {
        if (m_role != Client)
            return;
        QVariantList list;
        for (const auto &value : players)
            list.append(value.toObject().toVariantMap());
        m_lobbyPlayers = list;
        m_rules = qBound(0, rules, 1);
        emit lobbyChanged();
        emit stateChanged();
    });

    connect(m_client, &GameClient::started, this, [this](const QJsonObject &snapshot) {
        if (m_role != Client)
            return;
        m_game->applySnapshot(snapshot);
        m_game->setMyPlayer(1);
        m_matchRunning = true;
        m_practice = false;
        m_remoteAimVisible = false;
        m_screen = QStringLiteral("game");
        setStatus(QString());
        appendSystem(tr("Match started."));
        emit stateChanged();
    });

    connect(m_client, &GameClient::stateReceived, this, [this](const QJsonObject &snapshot) {
        if (m_role != Client)
            return;
        m_game->applySnapshot(snapshot);
        updateRemoteAimVisibility();
        emit stateChanged();
    });

    connect(m_client, &GameClient::shotReceived, this, [this](int, double, double) {
        m_remoteAimTimer.stop();
        m_remoteAimVisible = false;
        emit remoteAimChanged();
    });

    connect(m_client, &GameClient::aimReceived, this, [this](int from, double angle) {
        if (m_role != Client || from == 1)
            return;
        m_remoteAim = angle;
        m_remoteAimClock.start();
        m_remoteAimTimer.start();
        updateRemoteAimVisibility();
    });

    connect(m_client, &GameClient::chatReceived, this,
            [this](int from, const QString &name, const QString &text) {
        if (m_role != Client)
            return;
        appendChat(name, text, from == 1);
    });

    connect(m_client, &GameClient::rematchRequested, this, [this](int) {
        if (m_role != Client)
            return;
        setStatus(tr("The host wants a rematch."));
        appendSystem(tr("The host wants a rematch."));
        emit stateChanged();
    });

    connect(m_client, &GameClient::rejected, this, [this](const QString &reason) {
        clientLeaveBecause(reason, true);
    });

    connect(m_client, &GameClient::failed, this, [this](const QString &reason) {
        clientLeaveBecause(reason, true);
    });

    connect(m_client, &GameClient::disconnected, this, [this](const QString &reason) {
        if (m_role == Client)
            clientLeaveBecause(reason, false);
    });
}

void Backend::onHostHello(int peer, const QString &name, const QJsonObject &attestation)
{
    if (m_role != Host) {
        m_host->kick(peer, tr("This table is not accepting players."));
        return;
    }

    QString reason;
    if (!Omarchy::verifyRemote(attestation, &reason)) {
        m_host->kick(peer, reason);
        return;
    }
    if (m_guestPresent) {
        m_host->kick(peer, tr("This table already has two players."));
        return;
    }

    m_guestPresent = true;
    m_guestPeer = peer;
    m_guestName = name;
    m_guestAttestation = attestation;
    m_guestReady = false;
    m_hostAttestation = m_attestation.toJson();

    m_host->sendWelcome(peer, 1, m_rules, m_playerName, m_hostAttestation);
    sendLobbyToGuest();
    buildHostLobby();
    setStatus(tr("%1 joined. Start when you are both ready.").arg(name));
    appendSystem(tr("%1 joined the table.").arg(name));
    emit stateChanged();
}

void Backend::onHostReady(int peer, bool ready)
{
    if (m_role != Host || peer != m_guestPeer)
        return;
    m_guestReady = ready;
    sendLobbyToGuest();
    buildHostLobby();
    emit stateChanged();
}

void Backend::onHostChat(int peer, const QString &text)
{
    if (m_role != Host || peer != m_guestPeer)
        return;
    appendChat(m_guestName, text, false);
    m_host->broadcast({{"t", "chat"}, {"from", 1}, {"name", m_guestName}, {"text", text}});
}

void Backend::onHostDisconnected(int peer)
{
    if (m_role != Host || peer != m_guestPeer)
        return;

    const QString name = m_guestName;
    m_guestPresent = false;
    m_guestReady = false;
    m_guestPeer = -1;
    m_guestName.clear();
    m_guestAttestation = {};

    if (m_matchRunning) {
        m_matchRunning = false;
        m_game->clear();
        m_screen = QStringLiteral("lobby");
        setStatus(tr("%1 left the match. Waiting for a new player.").arg(name));
        appendSystem(tr("%1 left the match.").arg(name));
    } else {
        setStatus(tr("%1 left. Waiting for a new player.").arg(name));
        appendSystem(tr("%1 left the table.").arg(name));
    }

    buildHostLobby();
    m_discovery->updateStatus(QStringLiteral("lobby"));
    emit stateChanged();
}

void Backend::sendLobbyToGuest()
{
    if (!m_guestPresent || m_guestPeer < 0)
        return;
    QJsonArray players;
    players.append(playerJson(m_playerName, true, false, true, m_attestation.version));
    players.append(playerJson(m_guestName, false, true, m_guestReady,
                              m_guestAttestation.value("version").toString()));
    m_host->sendLobby(m_guestPeer, players, m_rules);
}

void Backend::buildHostLobby()
{
    QVariantList list;
    list.append(playerEntry(m_playerName, true, true, true, m_attestation.version));
    if (m_guestPresent)
        list.append(playerEntry(m_guestName, false, false, m_guestReady,
                                m_guestAttestation.value("version").toString()));
    m_lobbyPlayers = list;
    emit lobbyChanged();
}

void Backend::startMatch()
{
    if (m_role != Host || !m_guestPresent || m_guestPeer < 0)
        return;

    m_chatLog.clear();
    emit chatChanged();
    m_game->start(static_cast<Game::Rules>(m_rules), m_playerName, m_guestName,
                  QRandomGenerator::global()->generate());
    m_game->setMyPlayer(0);
    m_matchRunning = true;
    m_guestReady = false;
    m_rematchWanted = false;
    m_remoteAimVisible = false;
    m_screen = QStringLiteral("game");
    setStatus(QString());
    m_discovery->updateStatus(QStringLiteral("playing"));
    m_host->sendStart(m_guestPeer, m_game->snapshot());
    appendSystem(tr("Match started: %1 vs %2.").arg(m_playerName, m_guestName));
    sendLobbyToGuest();
    buildHostLobby();
    emit stateChanged();
}

void Backend::openJoin()
{
    if (!m_attestation.omarchy)
        return;
    resetSession();
    m_screen = QStringLiteral("join");
    setStatus(tr("Looking for tables on your Wi-Fi."));
    m_discovery->startBrowsing();
    emit stateChanged();
}

void Backend::closeJoin()
{
    if (m_role == Client)
        leave();
    else {
        m_discovery->stopBrowsing();
        m_screen = QStringLiteral("home");
        setStatus(QString());
        emit stateChanged();
    }
}

void Backend::refreshGames()
{
    m_discovery->startBrowsing();
    emit discoveredGamesChanged();
}

void Backend::joinGame(const QString &address)
{
    if (!m_attestation.omarchy)
        return;

    QString host;
    quint16 port = Protocol::TcpPort;
    if (!Protocol::parseAddress(address, &host, &port)) {
        setStatus(QString(), tr("Enter an address like 192.168.1.20:46321."));
        return;
    }

    resetSession();
    m_role = Client;
    m_joinAddress = address.trimmed();
    m_screen = QStringLiteral("join");
    m_connecting = true;
    setStatus(tr("Connecting to %1...").arg(m_joinAddress));
    m_client->connectToHost(host, port, m_playerName, m_attestation.toJson());
    emit stateChanged();
}

void Backend::leave()
{
    if (m_role == Host && m_guestPresent && m_guestPeer >= 0)
        m_host->kick(m_guestPeer, tr("The host closed the table."));
    resetSession();
    m_screen = m_attestation.omarchy ? QStringLiteral("home") : QStringLiteral("blocked");
    setStatus(m_attestation.omarchy ? QString() : m_attestation.reason);
    m_error.clear();
    emit stateChanged();
    emit statusChanged();
}

void Backend::rematch()
{
    if (m_role == Host) {
        if (m_guestPresent)
            startMatch();
    } else if (m_role == Client) {
        m_client->sendRematch();
        setStatus(tr("Rematch requested."));
        emit stateChanged();
    } else if (m_role == Local) {
        m_game->rackAgain();
        appendSystem(tr("Fresh rack."));
        emit stateChanged();
    }
}

void Backend::sendChat(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    const QString clean = text.trimmed().left(400);
    if (m_role == Host && m_matchRunning) {
        appendChat(m_playerName, clean, true);
        m_host->broadcast({{"t", "chat"}, {"from", 0}, {"name", m_playerName}, {"text", clean}});
    } else if (m_role == Client) {
        m_client->sendChat(clean);
    }
}

void Backend::setAim(double angle)
{
    m_game->setAim(angle);
    if (m_role == Host && m_matchRunning && m_game->currentPlayer() == 0)
        sendAimIfNeeded(angle);
    else if (m_role == Client && m_game->currentPlayer() == 1)
        sendAimIfNeeded(angle);
}

void Backend::setPower(double power)
{
    m_game->setAimPower(power);
}

void Backend::shoot(double angle, double power)
{
    if (m_role == Local) {
        m_game->shoot(angle, power);
    } else if (m_role == Host) {
        if (!m_matchRunning || m_game->phase() != Game::Aiming || m_game->currentPlayer() != 0)
            return;
        m_game->shoot(angle, power);
        m_host->broadcast({{"t", "shot"}, {"by", 0}, {"angle", angle}, {"power", power}});
    } else if (m_role == Client) {
        if (!myTurn())
            return;
        m_game->setAim(angle);
        m_game->setAimPower(power);
        m_client->sendShoot(angle, power);
    }
}

void Backend::shootCurrent()
{
    shoot(m_game->aimAngle(), m_game->aimPower());
}

void Backend::adjustAim(double delta)
{
    setAim(m_game->aimAngle() + delta);
}

void Backend::adjustPower(double delta)
{
    setPower(m_game->aimPower() + delta);
}

void Backend::placeCueBall(double x, double y)
{
    if (m_role == Client) {
        if (m_game->ballInHand())
            m_client->sendPlace(x, y);
        return;
    }
    if (m_role == Host && m_game->currentPlayer() != 0)
        return;
    if (m_game->canPlaceCueBall(x, y))
        m_game->placeCueBall(x, y);
}

bool Backend::canPlaceCueBall(double x, double y) const
{
    return m_game->canPlaceCueBall(x, y);
}

void Backend::previewCueBall(double x, double y)
{
    m_game->previewCueBall(x, y);
}

void Backend::goHome()
{
    if (m_role != None || m_screen == QLatin1String("join"))
        leave();
    else {
        m_screen = QStringLiteral("home");
        emit stateChanged();
    }
}

void Backend::setReady(bool ready)
{
    if (m_role == Client)
        m_client->setReady(ready);
}

void Backend::sendAimIfNeeded(double angle)
{
    if (!m_aimClock.isValid() || m_aimClock.elapsed() > 66) {
        if (m_role == Host && m_guestPresent && m_guestPeer >= 0)
            m_host->send(m_guestPeer, {{"t", "aim"}, {"from", 0}, {"angle", angle}});
        else if (m_role == Client)
            m_client->sendAim(angle);
        m_aimClock.start();
    }
}

void Backend::broadcastState()
{
    if (!m_guestPresent || m_guestPeer < 0)
        return;
    m_host->broadcast({{"t", "state"}, {"snapshot", m_game->snapshot()}});
}

void Backend::updateRemoteAimVisibility()
{
    bool visible = false;
    if ((m_role == Host || m_role == Client) && m_matchRunning
        && m_remoteAimClock.isValid() && m_remoteAimClock.elapsed() < 2500) {
        const int me = m_game->myPlayer();
        const bool theirTurn = me >= 0 && m_game->currentPlayer() != me;
        const bool aiming = m_game->phase() == Game::Aiming || m_game->phase() == Game::BallInHand;
        visible = theirTurn && aiming;
    }
    if (visible == m_remoteAimVisible)
        return;
    m_remoteAimVisible = visible;
    emit remoteAimChanged();
}

void Backend::clientLeaveBecause(const QString &reason, bool toJoinScreen)
{
    if (m_role != Client)
        return;
    m_role = None;
    m_connecting = false;
    m_matchRunning = false;
    m_client->disconnectFromHost();
    m_game->clear();
    m_lobbyPlayers.clear();
    m_guestPresent = false;
    m_guestPeer = -1;
    m_matchRunning = false;
    m_remoteAimVisible = false;
    m_error = reason;
    m_status = toJoinScreen ? tr("Connection closed.") : QString();
    m_screen = toJoinScreen ? QStringLiteral("join") : QStringLiteral("home");
    if (toJoinScreen)
        m_discovery->startBrowsing();
    emit lobbyChanged();
    emit remoteAimChanged();
    emit stateChanged();
    emit statusChanged();
}
