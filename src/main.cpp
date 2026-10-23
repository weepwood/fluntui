#include "MainWindow.h"
#include <QApplication>
#include <QFont>
#include <QSettings>
#include <QStringList>
#include <QTimer>

int main(int argc, char* argv[]) {
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Fluent System Info"));
    QApplication::setOrganizationName(QStringLiteral("weepwood"));
    QApplication::setApplicationVersion(QString::fromLatin1(FSI_APP_VERSION));
    fluent::initializeResources();
    app.setStyle(QStringLiteral("Fusion"));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);
    MainWindow window;
    window.show();
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
