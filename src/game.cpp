#include "game.h"

#include <QJsonArray>
#include <QRandomGenerator>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr int kFrameSubsteps = 8; // 8 x 1/480 s = 60 Hz of table time.
}

Game::Game(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Game::advanceFrame);
}

QVariantList Game::balls() const
{
    QVariantList list;
    list.reserve(m_physics.balls.size());
    for (const auto &ball : m_physics.balls) {
        QVariantMap map;
        map.insert("id", ball.id);
        map.insert("x", ball.x);
        map.insert("y", ball.y);
        map.insert("active", ball.active);
        map.insert("stripe", ball.isStripe());
        map.insert("solid", ball.isSolid());
        list.append(map);
    }
    return list;
}

QString Game::rulesName() const
{
    return m_rules == EightBall ? tr("8-Ball") : tr("9-Ball");
}

int Game::remainingFor(int player) const
{
    if (m_rules == NineBall)
        return activeObjectBalls();
    if (player < 0 || player > 1 || m_group[player] == 0)
        return 7;
    int count = 0;
    for (const auto &ball : m_physics.balls)
        if (ball.active && groupOfBall(ball.id) == m_group[player])
            ++count;
    return count;
}

int Game::remaining0() const { return remainingFor(0); }
int Game::remaining1() const { return remainingFor(1); }

int Game::targetBall() const
{
    if (m_rules == NineBall)
        return lowestBall();
    if (onEight(m_current))
        return 8;
    return -1;
}

bool Game::canShoot() const
{
    if (m_phase != Aiming || m_winner >= 0)
        return false;
    if (m_myPlayer >= 0 && m_current != m_myPlayer)
        return false;
    const auto *cue = m_physics.find(0);
    return cue && cue->active;
}

bool Game::ballInHand() const
{
    return m_phase == BallInHand && (m_myPlayer < 0 || m_current == m_myPlayer);
}

bool Game::cueVisible() const
{
    const auto *cue = m_physics.find(0);
    return (cue && cue->active) || m_phase == BallInHand;
}

void Game::setMyPlayer(int player)
{
    player = std::clamp(player, -1, 1);
    if (m_myPlayer == player)
        return;
    m_myPlayer = player;
    emit changed();
}

void Game::setAim(double angle)
{
    if (!std::isfinite(angle))
        return;
    const double normalized = std::atan2(std::sin(angle), std::cos(angle));
    if (std::abs(normalized - m_aimAngle) < 1e-6)
        return;
    m_aimAngle = normalized;
    emit changed();
}

void Game::setAimPower(double power)
{
    const double clamped = std::clamp(power, 0.0, 1.0);
    if (std::abs(clamped - m_aimPower) < 1e-6)
        return;
    m_aimPower = clamped;
    emit changed();
}

void Game::start(Rules rules, const QString &player0, const QString &player1, quint32 seed)
{
    m_practice = false;
    m_rules = rules;
    m_names[0] = player0.isEmpty() ? tr("Player 1") : player0;
    m_names[1] = player1.isEmpty() ? tr("Player 2") : player1;
    m_seed = seed ? seed : QRandomGenerator::global()->generate();
    resetState();
    buildRack(m_seed);
    m_current = 0;
    m_phase = Aiming;
    m_winner = -1;
    m_breakShot = true;
    m_isBreak = true;
    m_cueGhost = headSpot();
    m_message = tr("%1 breaks.").arg(m_names[0]);
    emit changed();
}

void Game::startPractice(Rules rules, const QString &playerName)
{
    start(rules, playerName.isEmpty() ? tr("You") : playerName, tr("Table"));
    m_practice = true;
    m_myPlayer = 0;
    m_message = tr("Practice. Break when you are ready.");
    emit changed();
}

void Game::clear()
{
    m_timer.stop();
    m_physics.reset();
    m_names[0].clear();
    m_names[1].clear();
    m_winner = -1;
    m_phase = Aiming;
    m_pocketedThisShot.clear();
    emit changed();
}

void Game::resetState()
{
    m_timer.stop();
    m_physics.reset();
    m_group[0] = m_group[1] = 0;
    m_phase = Aiming;
    m_current = 0;
    m_winner = -1;
    m_pocketedThisShot.clear();
    m_firstContact = -1;
    m_railAfterContact = false;
    m_targetBeforeShot = -1;
    m_remainingBeforeShot = 0;
    m_aimPower = 0.55;
}

QPointF Game::headSpot() const { return QPointF(Physics::Width * 0.25, Physics::Height / 2); }
QPointF Game::footSpot() const { return QPointF(Physics::Width * 0.75, Physics::Height / 2); }

void Game::addBall(int id, double x, double y)
{
    m_physics.balls.append(BallState(id, x, y));
}

void Game::buildRack(quint32 seed)
{
    m_physics.reset();
    addBall(0, headSpot().x(), headSpot().y());

    std::mt19937 random(seed);
    const double spacing = 2 * Physics::BallRadius * 1.006;
    const double rowStep = spacing * std::sqrt(3.0) / 2.0;
    const double apexX = Physics::Width * 0.75;
    const double centerY = Physics::Height / 2;

    auto placeRow = [&](int row, int count, const std::vector<int> &ids) {
        const double x = apexX + row * rowStep;
        for (int i = 0; i < count; ++i) {
            const double y = centerY + (i - (count - 1) / 2.0) * spacing;
            addBall(ids[i], x, y);
        }
    };

    if (m_rules == EightBall) {
        std::vector<int> solids{1, 2, 3, 4, 5, 6, 7};
        std::vector<int> stripes{9, 10, 11, 12, 13, 14, 15};
        std::shuffle(solids.begin(), solids.end(), random);
        std::shuffle(stripes.begin(), stripes.end(), random);

        std::vector<int> positions(15, 0);
        positions[4] = 8;                    // centre of the third row
        positions[10] = solids.back();       // one back corner is a solid
        positions[14] = stripes.back();      // the other is a stripe
        solids.pop_back();
        stripes.pop_back();

        std::vector<int> rest = solids;
        rest.insert(rest.end(), stripes.begin(), stripes.end());
        std::shuffle(rest.begin(), rest.end(), random);
        for (int i = 0, n = 0; i < 15; ++i)
            if (positions[i] == 0)
                positions[i] = rest[n++];

        int cursor = 0;
        for (int row = 0; row < 5; ++row) {
            const int count = row + 1;
            std::vector<int> ids(positions.begin() + cursor, positions.begin() + cursor + count);
            placeRow(row, count, ids);
            cursor += count;
        }
    } else {
        std::vector<int> rest{2, 3, 4, 5, 6, 7, 8};
        std::shuffle(rest.begin(), rest.end(), random);
        std::vector<int> positions(9, 0);
        positions[0] = 1; // apex
        positions[4] = 9; // centre of the diamond
        for (int i = 0, n = 0; i < 9; ++i)
            if (positions[i] == 0)
                positions[i] = rest[n++];

        // A diamond pointing at the foot spot: 1, 2, 3, 2, 1.
        static constexpr int diamond[5] = {1, 2, 3, 2, 1};
        int cursor = 0;
        for (int row = 0; row < 5; ++row) {
            const int count = diamond[row];
            std::vector<int> ids(positions.begin() + cursor, positions.begin() + cursor + count);
            placeRow(row, count, ids);
            cursor += count;
        }
    }

    m_cueGhost = headSpot();
}

int Game::groupOfBall(int id) const
{
    if (id >= 1 && id <= 7)
        return 1;
    if (id >= 9 && id <= 15)
        return 2;
    return 0;
}

bool Game::groupsAssigned() const
{
    return m_group[0] != 0 && m_group[1] != 0;
}

bool Game::onEight(int player) const
{
    return groupsAssigned() && player >= 0 && player <= 1
        && m_group[player] != 0 && remainingFor(player) == 0;
}

int Game::activeObjectBalls() const
{
    int count = 0;
    for (const auto &ball : m_physics.balls)
        if (ball.active && ball.id > 0)
            ++count;
    return count;
}

int Game::lowestBall() const
{
    int lowest = 0;
    for (const auto &ball : m_physics.balls) {
        if (!ball.active || ball.id <= 0)
            continue;
        if (lowest == 0 || ball.id < lowest)
            lowest = ball.id;
    }
    return lowest;
}

void Game::shoot(double angle, double power)
{
    if (m_phase != Aiming || m_winner >= 0)
        return;
    auto *cue = m_physics.find(0);
    if (!cue || !cue->active)
        return;

    setAim(angle);
    power = std::clamp(power, 0.02, 1.0);
    m_aimPower = power;

    m_pocketedThisShot.clear();
    m_firstContact = -1;
    m_railAfterContact = false;
    m_targetBeforeShot = lowestBall();
    m_remainingBeforeShot = remainingFor(m_current);
    m_breakShot = m_isBreak;
    m_phase = Rolling;

    m_physics.strike(0, m_aimAngle, power);
    emit shotStarted(m_aimAngle, power);
    emit changed();
    m_timer.start();
}

bool Game::canPlaceCueBall(double x, double y) const
{
    return m_phase == BallInHand && m_physics.validCuePlacement(x, y);
}

void Game::placeCueBall(double x, double y)
{
    if (m_phase != BallInHand || !m_physics.validCuePlacement(x, y))
        return;

    auto *cue = m_physics.find(0);
    if (!cue)
        return;
    cue->active = true;
    cue->x = x;
    cue->y = y;
    cue->vx = cue->vy = 0;
    m_cueGhost = QPointF(x, y);
    m_phase = Aiming;
    setMessage(tr("%1 to shoot.").arg(m_names[m_current]));
    emit changed();
}

void Game::previewCueBall(double x, double y)
{
    if (m_phase != BallInHand || !m_physics.validCuePlacement(x, y))
        return;
    if (qFuzzyCompare(m_cueGhost.x(), x) && qFuzzyCompare(m_cueGhost.y(), y))
        return;
    m_cueGhost = QPointF(x, y);
    emit changed();
}

void Game::rackAgain()
{
    const int myPlayer = m_myPlayer;
    const bool practice = m_practice;
    const QString name0 = m_names[0], name1 = m_names[1];
    start(m_rules, name0, name1, QRandomGenerator::global()->generate());
    m_myPlayer = myPlayer;
    m_practice = practice;
    m_message = practice ? tr("Fresh rack.") : tr("%1 breaks.").arg(m_names[0]);
    emit changed();
}

void Game::noteEvents(const QList<PhysicsEvent> &events)
{
    for (const auto &event : events) {
        switch (event.type) {
        case PhysicsEvent::BallHit:
            playSound(QStringLiteral("click"));
            if (m_firstContact < 0 && (event.a == 0 || event.b == 0))
                m_firstContact = event.a == 0 ? event.b : event.a;
            break;
        case PhysicsEvent::RailHit:
            playSound(QStringLiteral("rail"));
            if (m_firstContact >= 0)
                m_railAfterContact = true;
            break;
        case PhysicsEvent::Pocketed:
            if (!m_pocketedThisShot.contains(event.a))
                m_pocketedThisShot.append(event.a);
            playSound(QStringLiteral("pocket"));
            break;
        }
    }
}

void Game::advanceFrame()
{
    if (m_phase != Rolling)
        return;

    QList<PhysicsEvent> events;
    for (int i = 0; i < kFrameSubsteps; ++i)
        m_physics.step(&events);
    noteEvents(events);

    if (!m_physics.rolling()) {
        m_timer.stop();
        finishShot();
    }
    emit changed();
}

void Game::finishShot()
{
    if (m_rules == EightBall)
        evaluateEightBall();
    else
        evaluateNineBall();
    emit shotFinished();
    emit changed();
}

void Game::finishTurn(bool foul, const QString &reason, const QList<int> &objectPockets)
{
    if (foul) {
        giveBallInHand(m_practice ? 0 : otherPlayer(), reason);
        return;
    }

    setMessage(tr("%1 to shoot.").arg(m_names[m_current]));
    if (objectPockets.isEmpty())
        switchTurn();
    emit changed();
}

void Game::evaluateEightBall()
{
    QList<int> objects;
    for (int id : m_pocketedThisShot)
        if (id > 0)
            objects.append(id);
    const bool scratch = m_pocketedThisShot.contains(0);
    const bool eight = m_pocketedThisShot.contains(8);
    const bool wasBreak = m_isBreak;
    m_isBreak = false;

    if (wasBreak && eight) {
        rerack();
        setMessage(tr("The 8-ball went in on the break. Re-rack."));
        return;
    }

    bool foul = false;
    QString reason;
    if (m_firstContact < 0) {
        foul = true;
        reason = tr("No object ball was contacted.");
    } else if (scratch) {
        foul = true;
        reason = tr("Cue ball pocketed.");
    } else if (!groupsAssigned() && m_firstContact == 8) {
        foul = true;
        reason = tr("The 8-ball was hit first.");
    } else if (groupsAssigned() && onEight(m_current) && m_firstContact != 8) {
        foul = true;
        reason = tr("The 8-ball must be hit first.");
    } else if (groupsAssigned() && !onEight(m_current)
               && groupOfBall(m_firstContact) != m_group[m_current]) {
        foul = true;
        reason = tr("The wrong group was hit first.");
    } else if (objects.isEmpty() && !m_railAfterContact) {
        foul = true;
        reason = tr("No rail after contact.");
    }

    if (eight) {
        const bool legalWin = !foul && groupsAssigned() && m_remainingBeforeShot == 0;
        if (legalWin)
            endMatch(m_current, tr("%1 pocketed the 8-ball.").arg(m_names[m_current]));
        else
            endMatch(otherPlayer(), tr("%1 pocketed the 8-ball early.").arg(m_names[m_current]));
        return;
    }

    if (foul) {
        giveBallInHand(m_practice ? 0 : otherPlayer(), reason + QLatin1Char(' ') + tr("Ball in hand."));
        return;
    }

    if (wasBreak) {
        if (objects.isEmpty())
            switchTurn();
        else
            setMessage(tr("%1 to shoot.").arg(m_names[m_current]));
        return;
    }

    if (!groupsAssigned() && !objects.isEmpty()) {
        int first = 0;
        for (int id : m_pocketedThisShot) {
            if (id > 0 && id != 8) {
                first = id;
                break;
            }
        }
        if (first > 0) {
            m_group[m_current] = groupOfBall(first);
            m_group[otherPlayer()] = 3 - m_group[m_current];
            const QString groupName = m_group[m_current] == 1 ? tr("solids") : tr("stripes");
            setMessage(tr("%1 is on %2.").arg(m_names[m_current], groupName));
        }
    }

    bool pocketedOwn = false;
    if (groupsAssigned()) {
        for (int id : objects)
            if (groupOfBall(id) == m_group[m_current])
                pocketedOwn = true;
    } else {
        pocketedOwn = !objects.isEmpty();
    }

    if (!pocketedOwn)
        switchTurn();
    emit changed();
}

void Game::evaluateNineBall()
{
    QList<int> objects;
    for (int id : m_pocketedThisShot)
        if (id > 0)
            objects.append(id);
    const bool scratch = m_pocketedThisShot.contains(0);
    const bool nine = m_pocketedThisShot.contains(9);
    const bool wasBreak = m_isBreak;
    m_isBreak = false;

    bool foul = false;
    QString reason;
    if (m_firstContact < 0) {
        foul = true;
        reason = tr("No object ball was contacted.");
    } else if (m_targetBeforeShot > 0 && m_firstContact != m_targetBeforeShot) {
        foul = true;
        reason = tr("The %1 was not hit first.").arg(m_targetBeforeShot);
    } else if (scratch) {
        foul = true;
        reason = tr("Cue ball pocketed.");
    } else if (objects.isEmpty() && !m_railAfterContact) {
        foul = true;
        reason = tr("No rail after contact.");
    }

    if (nine) {
        if (!foul) {
            endMatch(m_current, tr("%1 pocketed the 9-ball.").arg(m_names[m_current]));
        } else {
            spotBall(9);
            giveBallInHand(m_practice ? 0 : otherPlayer(),
                           tr("The 9-ball was spotted.") + QLatin1Char(' ') + tr("Ball in hand."));
        }
        return;
    }

    if (foul) {
        giveBallInHand(m_practice ? 0 : otherPlayer(), reason + QLatin1Char(' ') + tr("Ball in hand."));
        return;
    }

    if (objects.isEmpty()) {
        switchTurn();
    } else {
        setMessage(tr("%1 to shoot.").arg(m_names[m_current]));
        Q_UNUSED(wasBreak)
        emit changed();
    }
}

void Game::giveBallInHand(int player, const QString &reason)
{
    if (m_winner >= 0)
        return;

    m_current = m_practice ? 0 : player;
    auto *cue = m_physics.find(0);
    if (cue) {
        m_cueGhost = QPointF(std::clamp(cue->x, Physics::BallRadius, Physics::Width - Physics::BallRadius),
                             std::clamp(cue->y, Physics::BallRadius, Physics::Height - Physics::BallRadius));
        if (!m_physics.validCuePlacement(m_cueGhost.x(), m_cueGhost.y())) {
            // Walk toward the centre until there is a legal spot to offer.
            for (int i = 0; i < 30; ++i) {
                const double t = i / 30.0;
                const double x = m_cueGhost.x() * (1 - t) + Physics::Width / 2 * t;
                const double y = m_cueGhost.y() * (1 - t) + Physics::Height / 2 * t;
                if (m_physics.validCuePlacement(x, y)) {
                    m_cueGhost = QPointF(x, y);
                    break;
                }
            }
        }
        cue->active = false;
        cue->vx = cue->vy = 0;
    }
    m_phase = BallInHand;
    setMessage(reason);
    emit changed();
}

void Game::switchTurn()
{
    m_current = m_practice ? 0 : otherPlayer();
    m_phase = Aiming;
    setMessage(tr("%1 to shoot.").arg(m_names[m_current]));
}

void Game::endMatch(int winner, const QString &reason)
{
    m_timer.stop();
    m_winner = winner;
    m_phase = Finished;
    setMessage(reason);
    emit matchEnded(winner, reason);
    emit messageAdded(reason);
    emit changed();
}

void Game::rerack()
{
    resetState();
    buildRack(m_seed);
    m_breakShot = true;
    m_isBreak = true;
    m_phase = Aiming;
    m_winner = -1;
    m_message = tr("%1 breaks again.").arg(m_names[m_current]);
    emit changed();
}

void Game::spotBall(int id)
{
    auto *ball = m_physics.find(id);
    if (!ball)
        return;
    ball->active = true;
    ball->vx = ball->vy = 0;

    const QPointF spot = footSpot();
    ball->x = spot.x();
    ball->y = spot.y();
    for (int step = 1; step <= 40; ++step) {
        if (m_physics.validCuePlacement(ball->x, ball->y, id))
            return;
        ball->x = spot.x() + step * Physics::BallRadius * 2.1;
        if (ball->x > Physics::Width - Physics::BallRadius) {
            ball->x = spot.x() - step * Physics::BallRadius * 2.1;
            if (ball->x < Physics::BallRadius)
                break;
        }
    }
    ball->x = Physics::Width / 2;
    ball->y = Physics::Height / 2;
}

void Game::setMessage(const QString &message)
{
    if (m_message == message)
        return;
    m_message = message;
    emit messageAdded(message);
}

void Game::playSound(const QString &kind)
{
    emit soundCue(kind);
}

QJsonObject Game::snapshot() const
{
    QJsonObject object;
    object.insert("rules", m_rules);
    object.insert("phase", m_phase);
    object.insert("current", m_current);
    object.insert("group0", m_group[0]);
    object.insert("group1", m_group[1]);
    object.insert("winner", m_winner);
    object.insert("message", m_message);
    object.insert("remaining0", remaining0());
    object.insert("remaining1", remaining1());
    object.insert("target", targetBall());
    object.insert("breaking", m_isBreak);

    QJsonArray names;
    names.append(m_names[0]);
    names.append(m_names[1]);
    object.insert("names", names);

    QJsonArray balls;
    for (const auto &ball : m_physics.balls) {
        QJsonObject entry;
        entry.insert("i", ball.id);
        entry.insert("x", ball.x);
        entry.insert("y", ball.y);
        entry.insert("a", ball.active);
        balls.append(entry);
    }
    object.insert("balls", balls);

    QJsonArray cue;
    cue.append(m_cueGhost.x());
    cue.append(m_cueGhost.y());
    object.insert("cue", cue);
    return object;
}

void Game::applySnapshot(const QJsonObject &object)
{
    m_rules = static_cast<Rules>(object.value("rules").toInt(m_rules));
    m_phase = object.value("phase").toInt(m_phase);
    m_current = object.value("current").toInt(m_current);
    m_group[0] = object.value("group0").toInt(m_group[0]);
    m_group[1] = object.value("group1").toInt(m_group[1]);
    m_winner = object.value("winner").toInt(m_winner);
    m_message = object.value("message").toString(m_message);
    m_isBreak = object.value("breaking").toBool(m_isBreak);

    const QJsonArray names = object.value("names").toArray();
    if (names.size() == 2) {
        m_names[0] = names.at(0).toString();
        m_names[1] = names.at(1).toString();
    }

    const QJsonArray balls = object.value("balls").toArray();
    if (!balls.isEmpty()) {
        m_physics.balls.clear();
        for (const auto &value : balls) {
            const QJsonObject entry = value.toObject();
            BallState ball;
            ball.id = entry.value("i").toInt();
            ball.x = entry.value("x").toDouble();
            ball.y = entry.value("y").toDouble();
            ball.active = entry.value("a").toBool(true);
            m_physics.balls.append(ball);
        }
    }

    const QJsonArray cue = object.value("cue").toArray();
    if (cue.size() == 2)
        m_cueGhost = QPointF(cue.at(0).toDouble(), cue.at(1).toDouble());

    emit changed();
}
