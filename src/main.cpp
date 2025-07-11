#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("StockMonitor");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("StockMonitor");
    app.setStyle("Fusion");

    MainWindow window;
    window.show();
    return app.exec();
}
