

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QUrl>

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qputenv("QML_IMPORT_TRACE", "1");
    QQmlApplicationEngine engine;
    
    engine.load(QUrl(QStringLiteral("qrc:///contents/ui/ExampleApp.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
