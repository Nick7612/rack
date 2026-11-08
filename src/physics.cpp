#include "physics.h"

#include <algorithm>
#include <cmath>

void Physics::reset()
{
    balls.clear();
}

BallState *Physics::find(int id)
{
    for (auto &ball : balls)
        if (ball.id == id)
            return &ball;
    return nullptr;
}

const BallState *Physics::find(int id) const
{
    for (const auto &ball : balls)
        if (ball.id == id)
            return &ball;
    return nullptr;
}

QList<QPointF> Physics::pocketCenters()
{
    const double w = Width, h = Height;
    return {
        QPointF(0, 0), QPointF(w / 2, 0), QPointF(w, 0),
        QPointF(0, h), QPointF(w / 2, h), QPointF(w, h)
    };
}

bool Physics::inPocket(double x, double y)
{
    for (const auto &pocket : pocketCenters()) {
        const bool side = qFuzzyCompare(pocket.x(), Width / 2);
        const double radius = side ? SidePocketRadius : CornerPocketRadius;
        if (std::hypot(x - pocket.x(), y - pocket.y()) < radius)
            return true;
    }
    return false;
}

bool Physics::insideTable(double x, double y)
{
    return x >= BallRadius && x <= Width - BallRadius
        && y >= BallRadius && y <= Height - BallRadius
        && !inPocket(x, y);
}

bool Physics::validCuePlacement(double x, double y, int ignoreId) const
{
    if (!insideTable(x, y))
        return false;

    for (const auto &ball : balls) {
        if (!ball.active || ball.id == ignoreId)
            continue;
        if (std::hypot(x - ball.x, y - ball.y) < 2 * BallRadius + 0.001)
            return false;
    }
    return true;
}

void Physics::strike(int id, double angle, double power)
{
    auto *ball = find(id);
    if (!ball || !ball->active)
        return;

    const double speed = std::clamp(power, 0.02, 1.0) * MaxShotSpeed;
    ball->vx = std::cos(angle) * speed;
    ball->vy = std::sin(angle) * speed;
}

bool Physics::rolling() const
{
    for (const auto &ball : balls)
        if (ball.active && (ball.vx != 0.0 || ball.vy != 0.0))
            return true;
    return false;
}

void Physics::stopAll()
{
    for (auto &ball : balls) {
        ball.vx = 0;
        ball.vy = 0;
    }
}

void Physics::step(QList<PhysicsEvent> *events)
{
    // Move first, then resolve. At the top speed a ball travels 15 mm per
    // step, well under a ball diameter, so overlap tests cannot tunnel.
    for (auto &ball : balls) {
        if (!ball.active)
            continue;
        ball.x += ball.vx * StepSeconds;
        ball.y += ball.vy * StepSeconds;
    }

    collectPockets(events);
    collideCushions(events);
    collideBalls(events);
    applyFriction();
}

void Physics::collectPockets(QList<PhysicsEvent> *events)
{
    for (auto &ball : balls) {
        if (!ball.active)
            continue;
        if (inPocket(ball.x, ball.y)) {
            ball.active = false;
            ball.vx = 0;
            ball.vy = 0;
            if (events)
                events->append({PhysicsEvent::Pocketed, ball.id, -1});
        }
    }
}

void Physics::collideCushions(QList<PhysicsEvent> *events)
{
    const double r = BallRadius;
    for (auto &ball : balls) {
        if (!ball.active)
            continue;

        if (ball.x < r) {
            ball.x = r;
            ball.vx = -ball.vx * CushionRestitution;
            if (events && std::abs(ball.vx) > 0.05)
                events->append({PhysicsEvent::RailHit, ball.id, -1});
        } else if (ball.x > Width - r) {
            ball.x = Width - r;
            ball.vx = -ball.vx * CushionRestitution;
            if (events && std::abs(ball.vx) > 0.05)
                events->append({PhysicsEvent::RailHit, ball.id, -1});
        }

        if (ball.y < r) {
            ball.y = r;
            ball.vy = -ball.vy * CushionRestitution;
            if (events && std::abs(ball.vy) > 0.05)
                events->append({PhysicsEvent::RailHit, ball.id, -1});
        } else if (ball.y > Height - r) {
            ball.y = Height - r;
            ball.vy = -ball.vy * CushionRestitution;
            if (events && std::abs(ball.vy) > 0.05)
                events->append({PhysicsEvent::RailHit, ball.id, -1});
        }
    }
}

void Physics::collideBalls(QList<PhysicsEvent> *events)
{
    const double diameter = 2 * BallRadius;
    for (int i = 0; i < balls.size(); ++i) {
        for (int j = i + 1; j < balls.size(); ++j) {
            auto &a = balls[i];
            auto &b = balls[j];
            if (!a.active || !b.active)
                continue;

            double dx = b.x - a.x, dy = b.y - a.y;
            double distance = std::hypot(dx, dy);
            if (distance >= diameter)
                continue;
            if (distance < 1e-9) {
                // Exactly coincident balls need an arbitrary but stable normal.
                dx = 1;
                dy = 0;
                distance = 1;
            }

            const double nx = dx / distance, ny = dy / distance;
            const double overlap = diameter - distance;
            a.x -= nx * overlap / 2;
            a.y -= ny * overlap / 2;
            b.x += nx * overlap / 2;
            b.y += ny * overlap / 2;

            const double relative = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
            if (relative < 0) {
                const double impulse = -(1 + BallRestitution) * relative / 2;
                a.vx -= impulse * nx;
                a.vy -= impulse * ny;
                b.vx += impulse * nx;
                b.vy += impulse * ny;
                if (events)
                    events->append({PhysicsEvent::BallHit, a.id, b.id});
            }
        }
    }
}

void Physics::applyFriction()
{
    for (auto &ball : balls) {
        if (!ball.active)
            continue;

        const double speed = std::hypot(ball.vx, ball.vy);
        if (speed == 0)
            continue;

        const double drop = Friction * StepSeconds;
        if (speed <= drop || speed <= StopSpeed) {
            ball.vx = 0;
            ball.vy = 0;
        } else {
            const double scale = (speed - drop) / speed;
            ball.vx *= scale;
            ball.vy *= scale;
        }
    }
}
