#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

#include "omarchy.h"

class Discovery;
class Game;
class GameClient;
class HostServer;
class Sound;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *game READ gameObject CONSTANT)
    Q_PROPERTY(QObject *sound READ soundObject CONSTANT)
    Q_PROPERTY(QString screen READ screen NOTIFY stateChanged)
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged)
    Q_PROPERTY(QString roomName READ roomName NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(QString address READ address NOTIFY stateChanged)
    Q_PROPERTY(QString joinAddress READ joinAddress WRITE setJoinAddress NOTIFY stateChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString omarchyVersion READ omarchyVersion NOTIFY stateChanged)
    Q_PROPERTY(QString omarchyDetail READ omarchyDetail NOTIFY stateChanged)
    Q_PROPERTY(bool omarchyOk READ omarchyOk NOTIFY stateChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY stateChanged)
    Q_PROPERTY(bool isClient READ isClient NOTIFY stateChanged)
    Q_PROPERTY(bool isLocal READ isLocal NOTIFY stateChanged)
    Q_PROPERTY(bool isPractice READ isPractice NOTIFY stateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY lobbyChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY stateChanged)
    Q_PROPERTY(bool myTurn READ myTurn NOTIFY stateChanged)
    Q_PROPERTY(int rules READ rules WRITE setRules NOTIFY stateChanged)
    Q_PROPERTY(QString rulesName READ rulesName NOTIFY stateChanged)
    Q_PROPERTY(QVariantList discoveredGames READ discoveredGames NOTIFY discoveredGamesChanged)
    Q_PROPERTY(QVariantList lobbyPlayers READ lobbyPlayers NOTIFY lobbyChanged)
    Q_PROPERTY(QVariantList chatLog READ chatLog NOTIFY chatChanged)
    Q_PROPERTY(double remoteAim READ remoteAim NOTIFY remoteAimChanged)
    Q_PROPERTY(bool remoteAimVisible READ remoteAimVisible NOTIFY remoteAimChanged)
    Q_PROPERTY(bool typing READ typing NOTIFY typingChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QObject *gameObject();
    QObject *soundObject();

    QString screen() const { return m_screen; }
    QString playerName() const { return m_playerName; }
    void setPlayerName(const QString &name);
    QString roomName() const { return m_roomName; }
    QString statusText() const { return m_status; }
    QString errorText() const { return m_error; }
    QString address() const { return m_address; }
    QString joinAddress() const { return m_joinAddress; }
    void setJoinAddress(const QString &address);
    QString version() const { return QStringLiteral("0.1.0"); }
    QString omarchyVersion() const { return m_attestation.version; }
    QString omarchyDetail() const;
    bool omarchyOk() const { return m_attestation.omarchy; }
    bool isHost() const { return m_role == Host; }
    bool isClient() const { return m_role == Client; }
    bool isLocal() const { return m_role == Local; }
    bool isPractice() const { return m_practice; }
    bool canStart() const { return m_role == Host && m_guestPresent; }
    bool connecting() const { return m_connecting; }
    bool myTurn() const;
    int rules() const { return m_rules; }
    void setRules(int rules);
    QString rulesName() const;
    QVariantList discoveredGames() const;
    QVariantList lobbyPlayers() const { return m_lobbyPlayers; }
    QVariantList chatLog() const { return m_chatLog; }
    double remoteAim() const { return m_remoteAim; }
    bool remoteAimVisible() const { return m_remoteAimVisible; }
    bool typing() const { return m_typing; }

    Q_INVOKABLE void startLocal(int rules);
    Q_INVOKABLE void startPractice(int rules);
    Q_INVOKABLE void hostGame(int rules);
    Q_INVOKABLE void startMatch();
    Q_INVOKABLE void openJoin();
    Q_INVOKABLE void closeJoin();
    Q_INVOKABLE void refreshGames();
    Q_INVOKABLE void joinGame(const QString &address);
    Q_INVOKABLE void leave();
    Q_INVOKABLE void rematch();
    Q_INVOKABLE void sendChat(const QString &text);
    Q_INVOKABLE void setAim(double angle);
    Q_INVOKABLE void setPower(double power);
    Q_INVOKABLE void shoot(double angle, double power);
    Q_INVOKABLE void shootCurrent();
    Q_INVOKABLE void adjustAim(double delta);
    Q_INVOKABLE void adjustPower(double delta);
    Q_INVOKABLE void placeCueBall(double x, double y);
    Q_INVOKABLE void previewCueBall(double x, double y);
    Q_INVOKABLE bool canPlaceCueBall(double x, double y) const;
    Q_INVOKABLE void goHome();
    Q_INVOKABLE void setReady(bool ready);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void copyAddress();

signals:
    void stateChanged();
    void statusChanged();
    void lobbyChanged();
    void discoveredGamesChanged();
    void chatChanged();
    void remoteAimChanged();
    void playerNameChanged();
    void typingChanged();

private:
    enum Role { None, Host, Client, Local };

    Game *game() const;
    Sound *sound() const;

    void setScreen(const QString &screen);
    void setStatus(const QString &status, const QString &error = {});
    void resetSession();
    void connectHostSignals();
    void connectClientSignals();
    void onHostHello(int peer, const QString &name, const QJsonObject &attestation);
    void onHostReady(int peer, bool ready);
    void onHostChat(int peer, const QString &text);
    void onHostDisconnected(int peer);
    void sendLobbyToGuest();
    void buildHostLobby();
    void appendChat(const QString &name, const QString &text, bool mine);
    void appendSystem(const QString &text);
    void broadcastState();
    void updateRemoteAimVisibility();
    void sendAimIfNeeded(double angle);
    void hostShot(double angle, double power);
    void clientLeaveBecause(const QString &reason, bool toJoinScreen);

    QSettings m_settings;
    Attestation m_attestation;
    Game *m_game = nullptr;
    Sound *m_sound = nullptr;
    HostServer *m_host = nullptr;
    GameClient *m_client = nullptr;
    Discovery *m_discovery = nullptr;

    Role m_role = None;
    QString m_screen = QStringLiteral("home");
    QString m_playerName;
    QString m_roomName;
    QString m_status;
    QString m_error;
    QString m_address;
    QString m_joinAddress;
    QString m_hostName;
    QString m_guestName;
    QJsonObject m_hostAttestation;
    QJsonObject m_guestAttestation;
    int m_guestPeer = -1;
    bool m_guestPresent = false;
    bool m_guestReady = false;
    bool m_connecting = false;
    bool m_matchRunning = false;
    bool m_practice = false;
    bool m_rematchWanted = false;
    int m_rules = 0;
    QVariantList m_lobbyPlayers;
    QVariantList m_chatLog;
    double m_remoteAim = 0;
    bool m_remoteAimVisible = false;
    QElapsedTimer m_aimClock;
    QElapsedTimer m_remoteAimClock;
    QTimer m_remoteAimTimer;
    bool m_typing = false;
};
