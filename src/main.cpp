// rack — Omarchy-only multiplayer billiards.

#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "game.h"
#include "omarchy.h"
#include "tableview.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    if (arguments.contains(QStringLiteral("--check"))) {
        QCoreApplication app(argc, argv);
        app.setApplicationName(QStringLiteral("rack"));
        const Attestation attestation = Omarchy::detect();
        QJsonObject object = attestation.toJson();
        object.insert("reason", attestation.reason);
        object.insert("protocol", 1);
        const QByteArray output = QJsonDocument(object).toJson(QJsonDocument::Compact);
        fputs(output.constData(), stdout);
        fputc('\n', stdout);
        return attestation.omarchy ? 0 : 1;
    }

    if (arguments.contains(QStringLiteral("--version"))) {
        QCoreApplication app(argc, argv);
        fputs("rack 0.1.0\n", stdout);
        return 0;
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rack"));
    app.setApplicationDisplayName(QStringLiteral("Rack"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setDesktopFileName(QStringLiteral("rack"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("rack")));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    qmlRegisterType<TableView>("Rack", 1, 0, "TableView");
    qmlRegisterUncreatableType<Game>("Rack", 1, 0, "Game", QStringLiteral("Game lives in Backend"));

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    int rules = backend.rules();
    QString name;
    QString joinAddress;
    QString screenshot;
    bool host = false;
    bool practice = false;
    bool local = false;

    for (int i = 0; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (argument == QLatin1String("--name") && i + 1 < arguments.size())
            name = arguments.at(++i);
        else if (argument == QLatin1String("--join") && i + 1 < arguments.size())
            joinAddress = arguments.at(++i);
        else if (argument == QLatin1String("--screenshot") && i + 1 < arguments.size())
            screenshot = arguments.at(++i);
        else if (argument == QLatin1String("--host"))
            host = true;
        else if (argument == QLatin1String("--practice"))
            practice = true;
        else if (argument == QLatin1String("--local"))
            local = true;
        else if (argument == QLatin1String("--9ball"))
            rules = Game::NineBall;
        else if (argument == QLatin1String("--8ball"))
            rules = Game::EightBall;
    }

    if (!name.isEmpty())
        backend.setPlayerName(name);

    QTimer::singleShot(0, &backend, [&backend, rules, joinAddress, host, practice, local] {
        if (!joinAddress.isEmpty())
            backend.joinGame(joinAddress);
        else if (host)
            backend.hostGame(rules);
        else if (practice)
            backend.startPractice(rules);
        else if (local)
            backend.startLocal(rules);
    });

    // A development aid: render the current screen to a file, then exit. With
    // QT_QPA_PLATFORM=offscreen this captures the real QML without a display.
    if (!screenshot.isEmpty()) {
        QTimer::singleShot(900, &app, [&app, &engine, screenshot] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
            if (!window) {
                app.exit(2);
                return;
            }
            const QImage image = window->grabWindow();
            if (!image.save(screenshot))
                qWarning("Could not write %s", qPrintable(screenshot));
            // quit() can be deferred by an event-loop locker held while the
            // frame is grabbed, and the deferred quit never arrives. exit()
            // returns from exec() unconditionally.
            QTimer::singleShot(0, &app, [&app] { app.exit(0); });
        });
    }

    return app.exec();
}
