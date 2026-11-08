#include "omarchy.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
QHash<QString, QString> readOsRelease(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return values;
    const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0 || line.startsWith(QLatin1Char('#')))
            continue;
        QString value = line.mid(equals + 1).trimmed();
        if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))
            && value.endsWith(value.front()))
            value = value.mid(1, value.size() - 2);
        values.insert(line.left(equals).trimmed(), value);
    }
    return values;
}

bool validHash(const QString &hash)
{
    static const QRegularExpression expression("^[0-9a-f]{64}$");
    return expression.match(hash).hasMatch();
}
}

QString Omarchy::omarchyVersionFile()
{
    return QStringLiteral("/usr/share/omarchy/version");
}

QString Omarchy::hyprlandVersion()
{
    const QString hyprctl = QStandardPaths::findExecutable("hyprctl");
    if (hyprctl.isEmpty())
        return {};
    QProcess process;
    process.start(hyprctl, {"version"});
    if (!process.waitForFinished(350))
        return {};
    const QString first = QString::fromUtf8(process.readAllStandardOutput()).section(QLatin1Char('\n'), 0, 0);
    static const QRegularExpression expression("Hyprland\\s+([0-9][^\\s]*)");
    const auto match = expression.match(first);
    return match.hasMatch() ? match.captured(1) : QString();
}

Attestation Omarchy::detect()
{
    return detect(QStringLiteral("/etc/os-release"), QStringLiteral("/usr/share/omarchy"),
                  !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"));
}

Attestation Omarchy::detect(const QString &osReleasePath, const QString &omarchyPath, bool hyprlandSession)
{
    Attestation attestation;
    const auto os = readOsRelease(osReleasePath);
    attestation.id = os.value("ID");
    attestation.version = os.value("VERSION_ID", os.value("BUILD_ID"));
    attestation.build = os.value("BUILD_ID", attestation.version);

    const bool installPresent = QFileInfo(omarchyPath + "/bin/omarchy").isFile()
        || QFileInfo(omarchyPath).isDir();
    attestation.hyprland = hyprlandSession ? hyprlandVersion() : QString();

    QByteArray material;
    QFile release(osReleasePath);
    if (release.open(QIODevice::ReadOnly))
        material += release.readAll();
    QFile versionFile(omarchyPath + "/version");
    if (versionFile.open(QIODevice::ReadOnly))
        material += versionFile.readAll();
    material += "rack-omarchy-v1";
    attestation.hash = QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();

    if (attestation.id != QLatin1String("omarchy")) {
        attestation.reason = QObject::tr("This is not an Omarchy system (ID=%1).").arg(attestation.id);
    } else if (!installPresent) {
        attestation.reason = QObject::tr("The Omarchy install was not found at %1.").arg(omarchyPath);
    } else if (!hyprlandSession) {
        attestation.reason = QObject::tr("Rack needs an Omarchy Hyprland session.");
    } else if (attestation.hyprland.isEmpty()) {
        attestation.reason = QObject::tr("Hyprland could not be reached.");
    } else {
        attestation.omarchy = true;
        attestation.reason = QObject::tr("Omarchy %1 · Hyprland %2")
                                 .arg(attestation.version, attestation.hyprland);
    }
    return attestation;
}

bool Omarchy::verifyRemote(const QJsonObject &object, QString *reason)
{
    auto fail = [&](const QString &message) {
        if (reason)
            *reason = message;
        return false;
    };

    if (object.value("id").toString() != QLatin1String("omarchy"))
        return fail(QObject::tr("The other player is not running Omarchy."));
    if (object.value("version").toString().isEmpty())
        return fail(QObject::tr("The other player did not report an Omarchy version."));
    if (!object.value("hyprland").toBool())
        return fail(QObject::tr("The other player is not in an Omarchy Hyprland session."));
    if (!validHash(object.value("hash").toString()))
        return fail(QObject::tr("The other player's Omarchy attestation is malformed."));
    if (reason)
        *reason = object.value("version").toString();
    return true;
}

QJsonObject Attestation::toJson() const
{
    return {
        {"id", id},
        {"version", version},
        {"build", build},
        {"hyprland", !hyprland.isEmpty()},
        {"hyprlandVersion", hyprland},
        {"hash", QString::fromLatin1(hash)},
        {"ok", omarchy}
    };
}
