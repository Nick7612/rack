#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "physics.h"

// One match of 8-ball or 9-ball. The same object drives local play, the host's
// authoritative simulation, and the guest's view of the host's snapshots.
class Game : public QObject {
    Q_OBJECT
    friend class RackTests;
    Q_PROPERTY(QVariantList balls READ balls NOTIFY changed)
    Q_PROPERTY(int phase READ phase NOTIFY changed)
    Q_PROPERTY(int currentPlayer READ currentPlayer NOTIFY changed)
    Q_PROPERTY(int rules READ rules NOTIFY changed)
    Q_PROPERTY(QString rulesName READ rulesName NOTIFY changed)
    Q_PROPERTY(int group0 READ group0 NOTIFY changed)
    Q_PROPERTY(int group1 READ group1 NOTIFY changed)
    Q_PROPERTY(int remaining0 READ remaining0 NOTIFY changed)
    Q_PROPERTY(int remaining1 READ remaining1 NOTIFY changed)
    Q_PROPERTY(int targetBall READ targetBall NOTIFY changed)
    Q_PROPERTY(QString player0 READ player0 NOTIFY changed)
    Q_PROPERTY(QString player1 READ player1 NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(int winner READ winner NOTIFY changed)
    Q_PROPERTY(bool rolling READ rolling NOTIFY changed)
    Q_PROPERTY(bool canShoot READ canShoot NOTIFY changed)
    Q_PROPERTY(bool ballInHand READ ballInHand NOTIFY changed)
    Q_PROPERTY(double aimAngle READ aimAngle NOTIFY changed)
    Q_PROPERTY(double aimPower READ aimPower NOTIFY changed)
    Q_PROPERTY(double cueX READ cueX NOTIFY changed)
    Q_PROPERTY(double cueY READ cueY NOTIFY changed)
    Q_PROPERTY(bool cueVisible READ cueVisible NOTIFY changed)
    Q_PROPERTY(int myPlayer READ myPlayer NOTIFY changed)

public:
    enum Rules { EightBall = 0, NineBall = 1 };
    enum Phase { Aiming = 0, Rolling = 1, BallInHand = 2, Finished = 3 };
    Q_ENUM(Rules)
    Q_ENUM(Phase)

    explicit Game(QObject *parent = nullptr);

    QVariantList balls() const;
    int phase() const { return m_phase; }
    int currentPlayer() const { return m_current; }
    int rules() const { return m_rules; }
    QString rulesName() const;
    int group0() const { return m_group[0]; }
    int group1() const { return m_group[1]; }
    int remaining0() const;
    int remaining1() const;
    int targetBall() const;
    QString player0() const { return m_names[0]; }
    QString player1() const { return m_names[1]; }
    QString message() const { return m_message; }
    int winner() const { return m_winner; }
    bool rolling() const { return m_phase == Rolling; }
    bool canShoot() const;
    bool ballInHand() const;
    double aimAngle() const { return m_aimAngle; }
    double aimPower() const { return m_aimPower; }
    double cueX() const { return m_cueGhost.x(); }
    double cueY() const { return m_cueGhost.y(); }
    bool cueVisible() const;
    int myPlayer() const { return m_myPlayer; }

    void start(Rules rules, const QString &player0, const QString &player1, quint32 seed = 0);
    void startPractice(Rules rules, const QString &playerName);
    void clear();
    void setMyPlayer(int player);
    void setAim(double angle);
    void setAimPower(double power);
    void shoot(double angle, double power);
    bool canPlaceCueBall(double x, double y) const;
    void placeCueBall(double x, double y);
    void previewCueBall(double x, double y);
    void rackAgain();

    // Visible for tests and for the host's frame timer.
    void advanceFrame();

    QJsonObject snapshot() const;
    void applySnapshot(const QJsonObject &object);

    const QList<BallState> &rawBalls() const { return m_physics.balls; }

signals:
    void changed();
    void soundCue(const QString &kind);
    void shotStarted(double angle, double power);
    void shotFinished();
    void messageAdded(const QString &text);
    void matchEnded(int winner, const QString &reason);

private:
    void resetState();
    void buildRack(quint32 seed);
    void addBall(int id, double x, double y);
    void startRolling();
    void finishShot();
    void evaluateEightBall();
    void evaluateNineBall();
    void finishTurn(bool foul, const QString &reason, const QList<int> &objectPockets);
    void giveBallInHand(int player, const QString &reason);
    void endMatch(int winner, const QString &reason);
    void rerack();
    void spotBall(int id);
    void switchTurn();
    void noteEvents(const QList<PhysicsEvent> &events);
    void setMessage(const QString &message);
    void playSound(const QString &kind);
    int otherPlayer() const { return m_current == 0 ? 1 : 0; }
    int groupOfBall(int id) const;
    bool onEight(int player) const;
    bool groupsAssigned() const;
    int remainingFor(int player) const;
    int activeObjectBalls() const;
    int lowestBall() const;
    QPointF headSpot() const;
    QPointF footSpot() const;

    Physics m_physics;
    QTimer m_timer;
    QElapsedTimer m_clock;

    Rules m_rules = EightBall;
    int m_phase = Aiming;
    int m_current = 0;
    int m_winner = -1;
    int m_group[2] = {0, 0};
    QString m_names[2];
    QString m_message;
    quint32 m_seed = 0;
    int m_myPlayer = -1;
    bool m_practice = false;
    bool m_breakShot = true;
    bool m_isBreak = true;

    double m_aimAngle = 0;
    double m_aimPower = 0.55;
    QPointF m_cueGhost{Physics::Width * 0.25, Physics::Height / 2};

    QList<int> m_pocketedThisShot;
    int m_firstContact = -1;
    bool m_railAfterContact = false;
    int m_targetBeforeShot = -1;
    int m_remainingBeforeShot = 0;
};
