#pragma once

#include <QColor>
#include <QObject>
#include <QQuickPaintedItem>
#include <QRectF>

class Game;

// Paints the table, balls, cue, and guides with QPainter. The QML layer owns
// the surrounding HUD and mouse handling; this item owns the green baize.
class TableView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QObject *game READ gameObject WRITE setGameObject NOTIFY gameChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(int radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(double remoteAim READ remoteAim WRITE setRemoteAim NOTIFY remoteAimChanged)
    Q_PROPERTY(bool remoteAimVisible READ remoteAimVisible WRITE setRemoteAimVisible
                   NOTIFY remoteAimChanged)

public:
    explicit TableView(QQuickItem *parent = nullptr);

    QObject *gameObject() const;
    void setGameObject(QObject *game);
    QString accent() const { return m_accent; }
    void setAccent(const QString &accent);
    int radius() const { return m_radius; }
    void setRadius(int radius);
    double remoteAim() const { return m_remoteAim; }
    void setRemoteAim(double angle);
    bool remoteAimVisible() const { return m_remoteAimVisible; }
    void setRemoteAimVisible(bool visible);

    void paint(QPainter *painter) override;

    Q_INVOKABLE QPointF mapToTable(qreal x, qreal y) const;
    Q_INVOKABLE QPointF mapToView(qreal x, qreal y) const;

signals:
    void gameChanged();
    void accentChanged();
    void radiusChanged();
    void remoteAimChanged();

private:
    Game *game() const { return m_game; }
    void layout() const;
    void drawTable(QPainter *painter);
    void drawBalls(QPainter *painter);
    void drawAim(QPainter *painter);
    void drawBall(QPainter *painter, int id, double x, double y, qreal alpha = 1.0);
    QColor feltColor() const;

    QObject *m_gameObject = nullptr;
    Game *m_game = nullptr;
    QString m_accent = QStringLiteral("#FFD60A");
    int m_radius = 0;
    double m_remoteAim = 0;
    bool m_remoteAimVisible = false;

    mutable qreal m_scale = 1;
    mutable QRectF m_tableRect;
    mutable qreal m_rail = 10;
};
