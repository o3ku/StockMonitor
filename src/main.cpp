#include "MainWindow.hpp"
#include "StockConfig.hpp"
#include <QApplication>
#include <QHotkey>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("StockMonitor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("StockMonitor");
    app.setStyle("Fusion");

    auto config = std::make_shared<StockConfig>();
    MainWindow window(config);
    QHotkey hotkey(QKeySequence(config->hotkey), true, &app);
    QObject::connect(&hotkey, &QHotkey::activated, qApp, [&]() {
        if (window.isActiveWindow())
            window.hide();
        else
        {
            window.activateWindow();
            window.show();
        }
    });

    return app.exec();
}