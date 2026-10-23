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
    // CI uses offscreen Qt; only smoke-test headless-safe widgets here.
    // The native FluentQt window chrome requires a real Windows desktop.
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        fluent::basicinput::Button button(QStringLiteral("FluentQt smoke"));
        fluent::layout::Card card;
        button.show();
        card.show();
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
        return app.exec();
    }
    MainWindow window;
    window.show();
    return app.exec();
}
