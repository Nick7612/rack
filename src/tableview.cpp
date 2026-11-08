#include "tableview.h"

#include "game.h"
#include "physics.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
QColor ballColor(int id)
{
    static const QColor solids[] = {
        QColor("#f6c945"), QColor("#2f6fd0"), QColor("#d94b45"), QColor("#7b4bb7"),
        QColor("#e58a33"), QColor("#3f9a5c"), QColor("#8e3a3f")
    };
    if (id >= 1 && id <= 7)
        return solids[id - 1];
    if (id >= 9 && id <= 15)
        return solids[id - 9];
    return QColor("#17181c");
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}
}

TableView::TableView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setFlag(ItemHasContents, true);
}

QObject *TableView::gameObject() const
{
    return m_gameObject;
}

void TableView::setGameObject(QObject *object)
{
    if (m_gameObject == object)
        return;
    if (m_game)
        disconnect(m_game, nullptr, this, nullptr);
    m_gameObject = object;
    m_game = qobject_cast<Game *>(object);
    if (m_game) {
        connect(m_game, &Game::changed, this, [this] { update(); });
        connect(m_game, &Game::messageAdded, this, [this] { update(); });
    }
    emit gameChanged();
    update();
}

void TableView::setAccent(const QString &accent)
{
    if (m_accent == accent)
        return;
    m_accent = accent;
    emit accentChanged();
    update();
}

void TableView::setRadius(int radius)
{
    if (m_radius == radius)
        return;
    m_radius = radius;
    emit radiusChanged();
    update();
}

void TableView::setRemoteAim(double angle)
{
    if (qFuzzyCompare(m_remoteAim, angle))
        return;
    m_remoteAim = angle;
    emit remoteAimChanged();
    update();
}

void TableView::setRemoteAimVisible(bool visible)
{
    if (m_remoteAimVisible == visible)
        return;
    m_remoteAimVisible = visible;
    emit remoteAimChanged();
    update();
}

QColor TableView::feltColor() const
{
    const QColor accent(m_accent);
    return QColor::fromRgbF(0.030 + accent.redF() * 0.105,
                            0.185 + accent.greenF() * 0.115,
                            0.135 + accent.blueF() * 0.100);
}

void TableView::layout() const
{
    const qreal margin = qMax<qreal>(6, qMin(width(), height()) * 0.015);
    m_rail = qMax<qreal>(10, qMin(width(), height()) * 0.055);
    const qreal availableW = qMax<qreal>(1, width() - 2 * margin - 2 * m_rail);
    const qreal availableH = qMax<qreal>(1, height() - 2 * margin - 2 * m_rail);
    m_scale = qMin(availableW / Physics::Width, availableH / Physics::Height);
    const qreal tableW = Physics::Width * m_scale;
    const qreal tableH = Physics::Height * m_scale;
    m_tableRect = QRectF((width() - tableW) / 2, (height() - tableH) / 2, tableW, tableH);
}

QPointF TableView::mapToTable(qreal x, qreal y) const
{
    layout();
    return QPointF((x - m_tableRect.left()) / m_scale, (m_tableRect.bottom() - y) / m_scale);
}

QPointF TableView::mapToView(qreal x, qreal y) const
{
    layout();
    return QPointF(m_tableRect.left() + x * m_scale, m_tableRect.bottom() - y * m_scale);
}

void TableView::paint(QPainter *painter)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    layout();
    drawTable(painter);
    drawBalls(painter);
    drawAim(painter);
}

void TableView::drawTable(QPainter *painter)
{
    const QRectF outer = m_tableRect.adjusted(-m_rail, -m_rail, m_rail, m_rail);
    const qreal radius = m_radius > 0 ? qMin<qreal>(m_radius, m_rail * 0.7) : m_rail * 0.22;

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 110));
    painter->drawRoundedRect(outer.translated(0, 7), radius, radius);

    QLinearGradient wood(outer.topLeft(), outer.bottomLeft());
    wood.setColorAt(0.0, QColor("#65452a"));
    wood.setColorAt(0.45, QColor("#462c18"));
    wood.setColorAt(1.0, QColor("#261609"));
    painter->setBrush(wood);
    painter->drawRoundedRect(outer, radius, radius);

    painter->setPen(QPen(QColor(0, 0, 0, 90), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(outer.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // The felt.
    QLinearGradient felt(m_tableRect.topLeft(), m_tableRect.bottomRight());
    const QColor baize = feltColor();
    felt.setColorAt(0.0, baize.lighter(112));
    felt.setColorAt(0.55, baize);
    felt.setColorAt(1.0, baize.darker(118));
    painter->setPen(Qt::NoPen);
    painter->setBrush(felt);
    painter->drawRoundedRect(m_tableRect, qMin<qreal>(6, m_rail * 0.2), qMin<qreal>(6, m_rail * 0.2));

    // Cushion inner edge.
    painter->setPen(QPen(withAlpha(baize.darker(160), 190), qMax<qreal>(1.5, m_rail * 0.075)));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(m_tableRect.adjusted(1, 1, -1, -1), qMin<qreal>(6, m_rail * 0.2),
                             qMin<qreal>(6, m_rail * 0.2));

    // Pockets: leather ring, then the hole.
    for (const auto &pocket : Physics::pocketCenters()) {
        const QPointF center = mapToView(pocket.x(), pocket.y());
        const bool side = qFuzzyCompare(pocket.x(), Physics::Width / 2);
        const qreal r = (side ? 0.074 : 0.080) * m_scale;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#1b1008"));
        painter->drawEllipse(center, r * 1.18, r * 1.18);
        QRadialGradient hole(center, r);
        hole.setColorAt(0.0, QColor("#000000"));
        hole.setColorAt(0.72, QColor("#050505"));
        hole.setColorAt(1.0, QColor("#1a1a1a"));
        painter->setBrush(hole);
        painter->drawEllipse(center, r, r);
    }

    // Rail sights.
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 248, 220, 160));
    const qreal sight = qMax<qreal>(1.5, m_rail * 0.075);
    for (int i = 1; i <= 3; ++i) {
        const qreal x = m_tableRect.left() + m_tableRect.width() * i / 4.0;
        painter->drawEllipse(QPointF(x, outer.top() + m_rail * 0.5), sight, sight);
        painter->drawEllipse(QPointF(x, outer.bottom() - m_rail * 0.5), sight, sight);
    }
    for (int i = 1; i <= 3; ++i) {
        const qreal y = m_tableRect.top() + m_tableRect.height() * i / 4.0;
        painter->drawEllipse(QPointF(outer.left() + m_rail * 0.5, y), sight, sight);
        painter->drawEllipse(QPointF(outer.right() - m_rail * 0.5, y), sight, sight);
    }

    // A quiet wordmark on the bottom rail.
    QFont font = painter->font();
    font.setPixelSize(qMax(8, int(m_rail * 0.44)));
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, m_rail * 0.08);
    painter->setFont(font);
    painter->setPen(QColor(255, 255, 255, 38));
    painter->drawText(QRectF(outer.left(), outer.bottom() - m_rail, outer.width(), m_rail),
                      Qt::AlignCenter, QStringLiteral("RACK"));
}

void TableView::drawBall(QPainter *painter, int id, double x, double y, qreal alpha)
{
    const QPointF center = mapToView(x, y);
    const qreal r = Physics::BallRadius * m_scale;
    painter->save();
    painter->setOpacity(alpha);

    QRadialGradient shadow(center + QPointF(r * 0.22, r * 0.30), r * 1.45);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 120));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(shadow);
    painter->drawEllipse(center + QPointF(r * 0.25, r * 0.32), r * 1.25, r * 1.1);

    const QColor base = id == 0 ? QColor("#f2f0e9") : ballColor(id);
    const bool stripe = id >= 9 && id <= 15;

    QPainterPath circle;
    circle.addEllipse(center, r, r);
    painter->setClipPath(circle);

    if (stripe) {
        painter->fillPath(circle, QColor("#f4f1e8"));
        painter->fillRect(QRectF(center.x() - r, center.y() - r * 0.54, r * 2, r * 1.08), base);
    } else {
        QRadialGradient body(center + QPointF(-r * 0.32, -r * 0.36), r * 1.5);
        body.setColorAt(0.0, base.lighter(145));
        body.setColorAt(0.55, base);
        body.setColorAt(1.0, base.darker(155));
        painter->fillPath(circle, body);
    }

    // Shading and a specular dot keep the flat circles looking like balls.
    QRadialGradient shade(center, r);
    shade.setColorAt(0.62, QColor(0, 0, 0, 0));
    shade.setColorAt(1.0, QColor(0, 0, 0, 105));
    painter->fillPath(circle, shade);
    painter->setBrush(QColor(255, 255, 255, 120));
    painter->drawEllipse(center + QPointF(-r * 0.34, -r * 0.38), r * 0.20, r * 0.14);
    painter->setClipping(false);

    painter->setPen(QPen(QColor(0, 0, 0, 120), qMax<qreal>(0.7, r * 0.055)));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, r, r);

    if (id > 0) {
        const qreal disc = r * 0.44;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(248, 246, 238));
        painter->drawEllipse(center, disc, disc);

        QFont font = painter->font();
        font.setPixelSize(qMax(6, int(r * 0.80)));
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(QColor("#141418"));
        painter->drawText(QRectF(center.x() - disc, center.y() - disc, disc * 2, disc * 2),
                          Qt::AlignCenter, QString::number(id));
    }
    painter->restore();
}

void TableView::drawBalls(QPainter *painter)
{
    if (!m_game)
        return;

    for (const auto &ball : m_game->rawBalls()) {
        if (!ball.active)
            continue;
        drawBall(painter, ball.id, ball.x, ball.y);
    }

    if (m_game->phase() == Game::BallInHand) {
        painter->save();
        painter->setPen(QPen(QColor(m_accent), qMax<qreal>(1.4, m_rail * 0.06),
                             Qt::DashLine));
        painter->setBrush(QColor(255, 255, 255, 24));
        const QPointF center = mapToView(m_game->cueX(), m_game->cueY());
        const qreal r = Physics::BallRadius * m_scale;
        painter->drawEllipse(center, r * 1.25, r * 1.25);
        painter->restore();
        drawBall(painter, 0, m_game->cueX(), m_game->cueY(), 0.45);
    }
}

void TableView::drawAim(QPainter *painter)
{
    if (!m_game || m_game->winner() >= 0)
        return;

    const BallState *cue = nullptr;
    for (const auto &ball : m_game->rawBalls())
        if (ball.id == 0 && ball.active)
            cue = &ball;

    if (!cue)
        return;

    const qreal r = Physics::BallRadius * m_scale;
    const QPointF center = mapToView(cue->x, cue->y);
    const QColor accent(m_accent);

    if (m_remoteAimVisible) {
        const QPointF direction(std::cos(m_remoteAim), -std::sin(m_remoteAim));
        const QPointF end = center + direction * (0.80 * m_scale);
        painter->setPen(QPen(withAlpha(accent, 120), qMax<qreal>(1.2, r * 0.12), Qt::DashLine));
        painter->drawLine(center, end);
        painter->setBrush(withAlpha(accent, 34));
        painter->setPen(QPen(withAlpha(accent, 90), 1));
        painter->drawEllipse(center, r * 1.6, r * 1.6);
    }

    if (!m_game->canShoot())
        return;

    const double dx = std::cos(m_game->aimAngle());
    const double dy = std::sin(m_game->aimAngle());
    const double diameter = 2 * Physics::BallRadius;
    double travel = 10;

    for (const auto &ball : m_game->rawBalls()) {
        if (!ball.active || ball.id == 0)
            continue;
        const double ox = ball.x - cue->x;
        const double oy = ball.y - cue->y;
        const double projection = ox * dx + oy * dy;
        if (projection <= 0)
            continue;
        const double perpendicular = ox * ox + oy * oy - projection * projection;
        const double discriminant = diameter * diameter - perpendicular;
        if (discriminant < 0)
            continue;
        const double distance = projection - std::sqrt(discriminant);
        if (distance > 0 && distance < travel)
            travel = distance;
    }

    const double wallX = Physics::Width - Physics::BallRadius;
    const double wallY = Physics::Height - Physics::BallRadius;
    if (dx > 1e-6)
        travel = std::min(travel, (wallX - cue->x) / dx);
    else if (dx < -1e-6)
        travel = std::min(travel, (Physics::BallRadius - cue->x) / dx);
    if (dy > 1e-6)
        travel = std::min(travel, (wallY - cue->y) / dy);
    else if (dy < -1e-6)
        travel = std::min(travel, (Physics::BallRadius - cue->y) / dy);
    travel = std::max(0.0, travel);

    const QPointF ghost = center + QPointF(dx, -dy) * (travel * m_scale);

    painter->setPen(QPen(withAlpha(accent, 190), qMax<qreal>(1.2, r * 0.11), Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(center, ghost);

    painter->setPen(QPen(withAlpha(accent, 235), qMax<qreal>(1.1, r * 0.10)));
    painter->setBrush(QColor(255, 255, 255, 18));
    painter->drawEllipse(ghost, r, r);

    // The cue stick pulls back with power.
    const double pull = 0.055 + m_game->aimPower() * 0.30;
    const QPointF back(dx, -dy);
    const QPointF tip = center - back * (pull * m_scale);
    const QPointF butt = center - back * ((pull + 0.92) * m_scale);
    painter->setPen(QPen(QColor(224, 194, 142), qMax<qreal>(2.4, r * 0.55), Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(tip, butt);
    painter->setPen(QPen(QColor(43, 76, 112), qMax<qreal>(2.6, r * 0.58), Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(tip, tip - back * (0.05 * m_scale));
}
