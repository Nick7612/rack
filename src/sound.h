#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSoundEffect>

// Short synthesized clicks, rails, and pocket drops. Rate-limited so a break
// does not turn into a burst of sixteen overlapping samples.
class Sound : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit Sound(QObject *parent = nullptr);

    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);

    Q_INVOKABLE void play(const QString &kind);

signals:
    void mutedChanged();
    void volumeChanged();

private:
    void applyVolume();

    QSoundEffect m_click;
    QSoundEffect m_rail;
    QSoundEffect m_pocket;
    QHash<QString, QElapsedTimer> m_lastPlayed;
    bool m_muted = false;
    qreal m_volume = 0.6;
    bool m_available = true;
};
