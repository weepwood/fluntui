#include "ChatWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QTimer>

int main(int argc, char* argv[]) {
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("weepwood"));
    QCoreApplication::setApplicationName(QStringLiteral("FluentChat"));
    fluent::initializeResources();
    app.setStyle(QStringLiteral("Fusion"));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);
    if (app.arguments().contains(QStringLiteral("--smoke"))) {
        fluent::basicinput::Button button(QStringLiteral("FluentChat"));
        fluent::layout::Card card;
        button.show();
        card.show();
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
        return app.exec();
    }
    ChatWindow window;
    window.show();
    QTimer::singleShot(0, &window, [&window] { window.authenticate(); });
    return app.exec();
}
