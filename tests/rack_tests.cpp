#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

#include "game.h"
#include "gameclient.h"
#include "hostserver.h"
#include "omarchy.h"
#include "physics.h"
#include "protocol.h"
#include "theme.h"
#include "translator.h"

class RackTests : public QObject {
    Q_OBJECT

private slots:
    void physicsComesToRest();
    void physicsPocketsACornerBall();
    void physicsBouncesOffCushion();
    void cuePlacementIsValidated();

    void eightBallRackIsLegal();
    void nineBallRackIsLegal();
    void breakSettlesAndSpreads();

    void earlyEightBallLoses();
    void legalEightBallWins();
    void nineBallPocketsTheNine();
    void nineBallWrongFirstContactIsAFoul();
    void legalMissPassesTheTurn();
    void ballInHandPlacementRestoresTheCue();

    void snapshotRoundTrips();
    void protocolRoundTrips();
    void addressParsing();
    void omarchyRemoteVerification();
    void omarchyDetectionRejectsOtherSystems();

    void themeFallsBackAndFollowsPalette();
    void translatorFallsBackToEnglish();
    void translatorFollowsTheLanguageFile();

    void hostAndClientHandshake();

private:
    static bool advanceUntilDone(Game &game, int maximumFrames = 4000)
    {
        for (int i = 0; i < maximumFrames; ++i) {
            if (game.phase() != Game::Rolling)
                return true;
            game.advanceFrame();
        }
        return game.phase() != Game::Rolling;
    }

    static void clearTable(Game &game, Game::Rules rules)
    {
        game.start(rules, QStringLiteral("Host"), QStringLiteral("Guest"), 12345);
        game.m_physics.balls.clear();
        game.m_rules = rules;
        game.m_phase = Game::Aiming;
        game.m_winner = -1;
        game.m_current = 0;
        game.m_myPlayer = -1;
        game.m_practice = false;
        game.m_isBreak = false;
        game.m_group[0] = 0;
        game.m_group[1] = 0;
        game.m_pocketedThisShot.clear();
        game.m_firstContact = -1;
        game.m_railAfterContact = false;
    }

    static void add(Game &game, int id, double x, double y)
    {
        game.m_physics.balls.append(BallState(id, x, y));
    }

    static QJsonObject goodAttestation()
    {
        return {
            {"id", "omarchy"},
            {"version", "4.5.0"},
            {"build", "4.5.0"},
            {"hyprland", true},
            {"hash", QString(64, QLatin1Char('a'))}
        };
    }
};

void RackTests::physicsComesToRest()
{
    Physics physics;
    physics.balls.append(BallState(0, 1.0, 0.6));
    physics.strike(0, 0.4, 0.6);
    for (int i = 0; i < 20000 && physics.rolling(); ++i)
        physics.step();
    QVERIFY(!physics.rolling());
    const auto &ball = physics.balls.first();
    QVERIFY(ball.x >= Physics::BallRadius && ball.x <= Physics::Width - Physics::BallRadius);
    QVERIFY(ball.y >= Physics::BallRadius && ball.y <= Physics::Height - Physics::BallRadius);
}

void RackTests::physicsPocketsACornerBall()
{
    Physics physics;
    physics.balls.append(BallState(3, 0.02, 0.02));
    QList<PhysicsEvent> events;
    physics.step(&events);
    QVERIFY(!physics.balls.first().active);
    bool pocketed = false;
    for (const auto &event : events)
        if (event.type == PhysicsEvent::Pocketed && event.a == 3)
            pocketed = true;
    QVERIFY(pocketed);
}

void RackTests::physicsBouncesOffCushion()
{
    Physics physics;
    physics.balls.append(BallState(0, 0.06, 0.6));
    physics.balls.first().vx = -2.0;
    for (int i = 0; i < 40; ++i)
        physics.step();
    QVERIFY(physics.balls.first().vx > 0);
    QVERIFY(physics.balls.first().x >= Physics::BallRadius);
}

void RackTests::cuePlacementIsValidated()
{
    Physics physics;
    physics.balls.append(BallState(1, 0.5, 0.5));
    QVERIFY(physics.validCuePlacement(1.0, 1.0));
    QVERIFY(!physics.validCuePlacement(0.5, 0.5));   // occupied
    QVERIFY(!physics.validCuePlacement(0.01, 0.01)); // pocket
    QVERIFY(!physics.validCuePlacement(3.0, 0.5));   // off the table
}

void RackTests::eightBallRackIsLegal()
{
    Game game;
    game.start(Game::EightBall, "A", "B", 42);

    const auto &balls = game.rawBalls();
    QCOMPARE(balls.size(), 16);
    QSet<int> ids;
    for (const auto &ball : balls) {
        ids.insert(ball.id);
        QVERIFY(ball.x >= Physics::BallRadius && ball.x <= Physics::Width - Physics::BallRadius);
        QVERIFY(ball.y >= Physics::BallRadius && ball.y <= Physics::Height - Physics::BallRadius);
    }
    for (int id = 0; id <= 15; ++id)
        QVERIFY(ids.contains(id));

    const double spacing = 2 * Physics::BallRadius * 1.006;
    const double rowStep = spacing * std::sqrt(3.0) / 2.0;
    const double apexX = Physics::Width * 0.75;
    const BallState *eight = game.m_physics.find(8);
    QVERIFY(eight);
    QVERIFY(std::abs(eight->x - (apexX + 2 * rowStep)) < 1e-9);
    QVERIFY(std::abs(eight->y - Physics::Height / 2) < 1e-9);

    QList<const BallState *> back;
    for (const auto &ball : balls)
        if (ball.id > 0 && std::abs(ball.x - (apexX + 4 * rowStep)) < 1e-9)
            back.append(&ball);
    QCOMPARE(back.size(), 5);
    std::sort(back.begin(), back.end(), [](const BallState *a, const BallState *b) { return a->y < b->y; });
    const bool lowSolid = back.first()->isSolid();
    const bool highStripe = back.last()->isStripe();
    QVERIFY((lowSolid && highStripe) || (back.first()->isStripe() && back.last()->isSolid()));

    for (int i = 0; i < balls.size(); ++i)
        for (int j = i + 1; j < balls.size(); ++j) {
            const double distance = std::hypot(balls[i].x - balls[j].x, balls[i].y - balls[j].y);
            QVERIFY(distance >= 2 * Physics::BallRadius - 1e-7);
        }
}

void RackTests::nineBallRackIsLegal()
{
    Game game;
    game.start(Game::NineBall, "A", "B", 7);
    QCOMPARE(game.rawBalls().size(), 10);

    const double spacing = 2 * Physics::BallRadius * 1.006;
    const double rowStep = spacing * std::sqrt(3.0) / 2.0;
    const double apexX = Physics::Width * 0.75;
    const BallState *one = game.m_physics.find(1);
    const BallState *nine = game.m_physics.find(9);
    QVERIFY(one);
    QVERIFY(nine);
    QVERIFY(std::abs(one->x - apexX) < 1e-9);
    QVERIFY(std::abs(one->y - Physics::Height / 2) < 1e-9);
    QVERIFY(std::abs(nine->x - (apexX + 2 * rowStep)) < 1e-9);
    QVERIFY(std::abs(nine->y - Physics::Height / 2) < 1e-9);
}

void RackTests::breakSettlesAndSpreads()
{
    Game game;
    game.start(Game::EightBall, "A", "B", 99);
    game.setMyPlayer(-1);

    QHash<int, QPointF> before;
    for (const auto &ball : game.rawBalls())
        if (ball.id > 0)
            before.insert(ball.id, QPointF(ball.x, ball.y));

    game.shoot(0.0, 1.0);
    QCOMPARE(game.phase(), int(Game::Rolling));
    QVERIFY(advanceUntilDone(game));

    int moved = 0;
    for (const auto &ball : game.rawBalls()) {
        if (ball.id == 0 || !ball.active)
            continue;
        const QPointF start = before.value(ball.id);
        if (std::hypot(ball.x - start.x(), ball.y - start.y()) > 0.02)
            ++moved;
    }
    QVERIFY(moved >= 5);

    for (const auto &ball : game.rawBalls()) {
        if (!ball.active)
            continue;
        QVERIFY(ball.x >= 0 && ball.x <= Physics::Width);
        QVERIFY(ball.y >= 0 && ball.y <= Physics::Height);
    }
}

void RackTests::earlyEightBallLoses()
{
    Game game;
    clearTable(game, Game::EightBall);
    game.m_group[0] = 1;
    game.m_group[1] = 2;
    add(game, 0, 1.27, 0.25);
    add(game, 8, 1.27, 0.08);
    add(game, 1, 0.35, 0.95);
    add(game, 9, 2.0, 1.0);

    game.shoot(-M_PI / 2, 0.35);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.winner(), 1);
}

void RackTests::legalEightBallWins()
{
    Game game;
    clearTable(game, Game::EightBall);
    game.m_group[0] = 1;
    game.m_group[1] = 2;
    add(game, 0, 1.27, 0.25);
    add(game, 8, 1.27, 0.08);

    game.shoot(-M_PI / 2, 0.35);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.winner(), 0);
}

void RackTests::nineBallPocketsTheNine()
{
    Game game;
    clearTable(game, Game::NineBall);
    add(game, 0, 1.27, 0.25);
    add(game, 9, 1.27, 0.08);

    game.shoot(-M_PI / 2, 0.35);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.winner(), 0);
}

void RackTests::nineBallWrongFirstContactIsAFoul()
{
    Game game;
    clearTable(game, Game::NineBall);
    add(game, 0, 0.45, 0.635);
    add(game, 1, 1.85, 0.95);
    add(game, 5, 0.80, 0.635);
    add(game, 9, 2.1, 0.3);

    game.shoot(0.0, 0.55);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.phase(), int(Game::BallInHand));
    QCOMPARE(game.currentPlayer(), 1);
    QVERIFY(!game.m_physics.find(0)->active);
}

void RackTests::legalMissPassesTheTurn()
{
    Game game;
    clearTable(game, Game::EightBall);
    game.m_group[0] = 1;
    game.m_group[1] = 2;
    add(game, 0, 0.45, 0.635);
    add(game, 1, 1.40, 0.635);
    add(game, 9, 2.1, 1.0);

    game.shoot(0.0, 0.65);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.phase(), int(Game::Aiming));
    QCOMPARE(game.currentPlayer(), 1);
}

void RackTests::ballInHandPlacementRestoresTheCue()
{
    Game game;
    clearTable(game, Game::NineBall);
    add(game, 0, 0.45, 0.635);
    add(game, 1, 1.85, 0.95);
    add(game, 5, 0.80, 0.635);
    add(game, 9, 2.1, 0.3);
    game.shoot(0.0, 0.55);
    QVERIFY(advanceUntilDone(game));
    QCOMPARE(game.phase(), int(Game::BallInHand));

    QVERIFY(game.canPlaceCueBall(1.0, 0.4));
    game.placeCueBall(1.0, 0.4);
    QCOMPARE(game.phase(), int(Game::Aiming));
    QVERIFY(game.m_physics.find(0)->active);
    QVERIFY(std::abs(game.m_physics.find(0)->x - 1.0) < 1e-9);
}

void RackTests::snapshotRoundTrips()
{
    Game host;
    host.start(Game::NineBall, "Host", "Guest", 5);
    host.setMyPlayer(0);
    const QJsonObject snapshot = host.snapshot();

    Game guest;
    guest.applySnapshot(snapshot);
    QCOMPARE(guest.rules(), int(Game::NineBall));
    QCOMPARE(guest.player0(), QString("Host"));
    QCOMPARE(guest.player1(), QString("Guest"));
    QCOMPARE(guest.balls().size(), host.balls().size());
    QCOMPARE(guest.phase(), host.phase());
    QCOMPARE(guest.currentPlayer(), host.currentPlayer());
    QVERIFY(std::abs(guest.cueX() - host.cueX()) < 1e-9);
    QVERIFY(std::abs(guest.cueY() - host.cueY()) < 1e-9);
}

void RackTests::protocolRoundTrips()
{
    const QJsonObject message{{"t", "state"}, {"n", 12}, {"ok", true}};
    const QByteArray encoded = Protocol::encode(message);
    QVERIFY(encoded.endsWith('\n'));

    QJsonObject decoded;
    QVERIFY(Protocol::decode(encoded.trimmed(), &decoded));
    QCOMPARE(decoded.value("t").toString(), QString("state"));
    QCOMPARE(decoded.value("n").toInt(), 12);

    QVERIFY(!Protocol::decode("not json", &decoded));
    QVERIFY(!Protocol::decode("[1,2,3]", &decoded));
    QVERIFY(!Protocol::decode(QByteArray(Protocol::MaxLineBytes + 10, 'x'), &decoded));
}

void RackTests::addressParsing()
{
    QString host;
    quint16 port = 0;
    QVERIFY(Protocol::parseAddress("192.168.1.5", &host, &port));
    QCOMPARE(host, QString("192.168.1.5"));
    QCOMPARE(port, quint16(46321));

    QVERIFY(Protocol::parseAddress("table.local:5000", &host, &port));
    QCOMPARE(host, QString("table.local"));
    QCOMPARE(port, quint16(5000));

    QVERIFY(Protocol::parseAddress("[::1]:6000", &host, &port));
    QCOMPARE(host, QString("::1"));
    QCOMPARE(port, quint16(6000));

    QVERIFY(!Protocol::parseAddress("   ", &host, &port));
}

void RackTests::omarchyRemoteVerification()
{
    QString reason;
    QVERIFY(Omarchy::verifyRemote(goodAttestation(), &reason));

    QJsonObject notOmarchy = goodAttestation();
    notOmarchy.insert("id", "arch");
    QVERIFY(!Omarchy::verifyRemote(notOmarchy, &reason));

    QJsonObject noHyprland = goodAttestation();
    noHyprland.insert("hyprland", false);
    QVERIFY(!Omarchy::verifyRemote(noHyprland, &reason));

    QJsonObject badHash = goodAttestation();
    badHash.insert("hash", "nope");
    QVERIFY(!Omarchy::verifyRemote(badHash, &reason));
}

void RackTests::omarchyDetectionRejectsOtherSystems()
{
    QTemporaryDir dir;
    QFile release(dir.filePath("os-release"));
    QVERIFY(release.open(QIODevice::WriteOnly));
    release.write("ID=ubuntu\nVERSION_ID=24.04\n");
    release.close();

    const Attestation other = Omarchy::detect(dir.filePath("os-release"), dir.filePath("omarchy"), false);
    QVERIFY(!other.omarchy);
    QVERIFY(other.reason.contains("not an Omarchy", Qt::CaseInsensitive));
    QCOMPARE(other.id, QString("ubuntu"));

    QFile omarchyRelease(dir.filePath("os-release"));
    QVERIFY(omarchyRelease.open(QIODevice::WriteOnly));
    omarchyRelease.write("ID=omarchy\nVERSION_ID=4.5.0\nBUILD_ID=4.5.0\n");
    omarchyRelease.close();
    QVERIFY(QDir(dir.filePath("omarchy")).mkpath("bin"));
    QFile cli(dir.filePath("omarchy/bin/omarchy"));
    QVERIFY(cli.open(QIODevice::WriteOnly));
    cli.write("#!/bin/sh\n");
    cli.close();

    const Attestation noSession = Omarchy::detect(dir.filePath("os-release"),
                                                  dir.filePath("omarchy"), false);
    QVERIFY(!noSession.omarchy);
    QCOMPARE(noSession.version, QString("4.5.0"));
    QVERIFY(!noSession.hash.isEmpty());
}

void RackTests::themeFallsBackAndFollowsPalette()
{
    QTemporaryDir dir;
    Theme theme(dir.path());
    QCOMPARE(theme.accent(), QString("#FFD60A"));
    QCOMPARE(theme.accentForeground(), QString("black"));
    QCOMPARE(theme.radius(), 0);

    QVERIFY(QDir(dir.path()).mkpath("theme"));
    QFile colors(dir.path() + "/theme/colors.toml");
    QVERIFY(colors.open(QIODevice::WriteOnly));
    colors.write("accent = \"#112233\"\nbackground = \"#101010\"\nforeground = \"#eeeeee\"\n");
    colors.close();

    Theme second(dir.path());
    QCOMPARE(second.accent(), QString("#112233"));
    QCOMPARE(second.accentForeground(), QString("white"));
    QCOMPARE(second.background(), QString("#101010"));
}

void RackTests::translatorFallsBackToEnglish()
{
    QTemporaryDir dir;
    Translator translator(dir.path() + "/language", dir.path());
    QCOMPARE(translator.language(), QString("en"));
    QVERIFY(translator.translate("Main", "Save").isNull());
}

void RackTests::translatorFollowsTheLanguageFile()
{
    QTemporaryDir dir;
    auto write = [](const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    QVERIFY(write(dir.path() + "/language", "pt-BR\n"));
    QVERIFY(write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})"));
    QVERIFY(write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})"));

    Translator translator(dir.path() + "/language", dir.path());
    QCOMPARE(translator.language(), QString("pt-BR"));
    QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
    QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
    QVERIFY(translator.translate("Main", "Quit").isNull());
}

void RackTests::hostAndClientHandshake()
{
    HostServer host;
    QVERIFY(host.listen(0));
    QVERIFY(host.port() > 0);

    GameClient client;
    QSignalSpy hello(&host, &HostServer::hello);
    QSignalSpy welcomed(&client, &GameClient::welcomed);
    QSignalSpy shots(&host, &HostServer::shot);

    client.connectToHost(QStringLiteral("127.0.0.1"), host.port(), QStringLiteral("Guest"),
                         goodAttestation());
    QTRY_COMPARE_WITH_TIMEOUT(hello.count(), 1, 5000);
    const int peer = hello.at(0).at(0).toInt();
    QCOMPARE(hello.at(0).at(1).toString(), QString("Guest"));

    host.sendWelcome(peer, 1, 1, QStringLiteral("Host"), goodAttestation());
    QTRY_COMPARE_WITH_TIMEOUT(welcomed.count(), 1, 5000);
    QCOMPARE(welcomed.at(0).at(0).toInt(), 1);
    QCOMPARE(welcomed.at(0).at(1).toInt(), 1);

    client.sendShoot(1.25, 0.8);
    QTRY_COMPARE_WITH_TIMEOUT(shots.count(), 1, 5000);
    QCOMPARE(shots.at(0).at(0).toInt(), peer);
    QVERIFY(std::abs(shots.at(0).at(1).toDouble() - 1.25) < 1e-9);
}

QTEST_MAIN(RackTests)
#include "rack_tests.moc"
