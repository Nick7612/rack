#include "sound.h"
#include <QAudioDevice>

#include <QMediaDevices>
#include <QUrl>

Sound::Sound(QObject *parent) : QObject(parent)
{
    m_available = !QMediaDevices::audioOutputs().isEmpty();

    m_click.setSource(QUrl(QStringLiteral("qrc:/sounds/click.wav")));
    m_rail.setSource(QUrl(QStringLiteral("qrc:/sounds/rail.wav")));
    m_pocket.setSource(QUrl(QStringLiteral("qrc:/sounds/pocket.wav")));
    for (auto *effect : {&m_click, &m_rail, &m_pocket}) {
        effect->setLoopCount(1);
        effect->setVolume(m_volume);
    }
}

void Sound::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    emit mutedChanged();
}

void Sound::setVolume(qreal volume)
{
    volume = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(volume, m_volume))
        return;
    m_volume = volume;
    applyVolume();
    emit volumeChanged();
}

void Sound::applyVolume()
{
    m_click.setVolume(m_volume);
    m_rail.setVolume(m_volume);
    m_pocket.setVolume(m_volume);
}

void Sound::play(const QString &kind)
{
    if (m_muted || !m_available || m_volume <= 0.001)
        return;

    QSoundEffect *effect = nullptr;
    int minimumGap = 24;
    if (kind == QLatin1String("click")) {
        effect = &m_click;
    } else if (kind == QLatin1String("rail")) {
        effect = &m_rail;
        minimumGap = 35;
    } else if (kind == QLatin1String("pocket")) {
        effect = &m_pocket;
        minimumGap = 70;
    }
    if (!effect)
        return;

    auto &timer = m_lastPlayed[kind];
    if (timer.isValid() && timer.elapsed() < minimumGap)
        return;
    timer.start();
    effect->play();
}
