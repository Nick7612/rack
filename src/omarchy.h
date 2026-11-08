#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

// A community gate, not DRM. The app only plays on Omarchy, and a host asks
// every guest for the same attestation. Because the client is open source a
// determined non-Omarchy user can fake it; this keeps the table honest for
// normal use without pretending to be copy protection.
struct Attestation {
    bool omarchy = false;
    QString id;
    QString version;
    QString build;
    QString hyprland;
    QByteArray hash;
    QString reason;

    QJsonObject toJson() const;
};

class Omarchy {
public:
    static Attestation detect();
    static Attestation detect(const QString &osReleasePath, const QString &omarchyPath,
                              bool hyprlandSession);
    static bool verifyRemote(const QJsonObject &object, QString *reason = nullptr);
    static QString hyprlandVersion();
    static QString omarchyVersionFile();
};
