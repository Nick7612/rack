#pragma once

#include <QList>
#include <QPointF>

// A ball on the table, in metres. The playing surface is 2.54 x 1.27 m with
// the origin at the bottom-left corner, so all of the numbers below read like
// a real table rather than in pixels.
struct BallState {
    int id = 0;
    double x = 0;
    double y = 0;
    double vx = 0;
    double vy = 0;
    bool active = true;

    bool isStripe() const { return id >= 9 && id <= 15; }
    bool isSolid() const { return id >= 1 && id <= 7; }

    BallState() = default;
    BallState(int id, double x, double y) : id(id), x(x), y(y) {}
};

struct PhysicsEvent {
    enum Type { BallHit, RailHit, Pocketed };
    Type type = RailHit;
    int a = 0;
    int b = -1;
};

// Rolling-ball billiards. There is no spin: the cue strikes through the
// centre of the ball, balls collide as equal masses, and cloth friction bleeds
// speed until everything stops. The fixed step keeps a break from tunnelling.
class Physics {
public:
    static constexpr double Width = 2.54;
    static constexpr double Height = 1.27;
    static constexpr double BallRadius = 0.028575;
    static constexpr double CornerPocketRadius = 0.062;
    static constexpr double SidePocketRadius = 0.067;
    static constexpr double Friction = 0.22;
    static constexpr double CushionRestitution = 0.82;
    static constexpr double BallRestitution = 0.95;
    static constexpr double StopSpeed = 0.012;
    static constexpr double StepSeconds = 1.0 / 480.0;
    static constexpr double MaxShotSpeed = 7.2;

    QList<BallState> balls;

    void reset();
    void step(QList<PhysicsEvent> *events = nullptr);
    bool rolling() const;
    void strike(int id, double angle, double power);
    void stopAll();

    static QList<QPointF> pocketCenters();
    static bool inPocket(double x, double y);
    static bool insideTable(double x, double y);
    bool validCuePlacement(double x, double y, int ignoreId = 0) const;

    BallState *find(int id);
    const BallState *find(int id) const;

private:
    void collideBalls(QList<PhysicsEvent> *events);
    void collideCushions(QList<PhysicsEvent> *events);
    void collectPockets(QList<PhysicsEvent> *events);
    void applyFriction();
};
