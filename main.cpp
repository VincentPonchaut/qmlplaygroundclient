#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QtWebView>

#include "applicationcontrol.h"

#if defined(Q_OS_ANDROID)
#include "Multicastlock.h"
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationName("QmlPlayground");
    QCoreApplication::setOrganizationDomain("QmlPlayground.com");
    QCoreApplication::setApplicationName("QmlPlaygroundClient");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QGuiApplication app(argc, argv);
    QtWebView::initialize();

#if defined(Q_OS_ANDROID)
    MulticastLock mcLock; // automatically calls acquire
#endif

    QQmlApplicationEngine engine;

    ApplicationControl appControl;
    appControl.setEngine(&engine);
    engine.rootContext()->setContextProperty("appControl", &appControl);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
