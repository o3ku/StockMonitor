#include "MainWindow.hpp"
#include "StockApiService.hpp"
#include "StockConfig.hpp"
#include <QApplication>
#include <QHotkey>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QTextStream>

namespace
{
struct ApiTestResult
{
    bool searchOk{ false };
    bool infoOk{ false };
    QString error;
};

ApiTestResult runProviderTest(StockApiService &service, const QString &providerName, const QString &keyword, const QStringList &codes, int timeoutMs)
{
    ApiTestResult result;
    bool exactMatch = service.setActiveProvider(providerName);
    if (!exactMatch)
    {
        result.error = QString("Provider mismatch: requested %1, using %2").arg(providerName, service.activeProviderName());
        return result;
    }

    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QMetaObject::Connection errorConn = QObject::connect(&service, &StockApiService::errorOccurred, &loop, [&](const QString &err) {
            result.error = err;
            loop.quit();
        });
        QMetaObject::Connection searchConn = QObject::connect(
                &service, &StockApiService::searchCompleted, &loop, [&](quint64, const QString &, const QList<StockInfo> &results) {
            for (const StockInfo &info : results)
            {
                if (codes.contains(info.code()))
                {
                    result.searchOk = true;
                    break;
                }
            }
            loop.quit();
        });

        timer.start(timeoutMs);
        service.searchStocksAsync(keyword);
        loop.exec();

        QObject::disconnect(searchConn);
        QObject::disconnect(errorConn);
    }

    if (!result.searchOk && result.error.isEmpty())
    {
        result.error = "Search returned no matching codes";
        return result;
    }

    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QMetaObject::Connection errorConn = QObject::connect(&service, &StockApiService::errorOccurred, &loop, [&](const QString &err) {
            result.error = err;
            loop.quit();
        });
        QMetaObject::Connection infoConn = QObject::connect(&service, &StockApiService::stockInfosReceived, &loop, [&](const QList<StockInfo> &infos) {
            for (const StockInfo &info : infos)
            {
                if (codes.contains(info.code()))
                {
                    result.infoOk = true;
                    break;
                }
            }
            loop.quit();
        });

        timer.start(timeoutMs);
        service.getStockInfosAsync(codes);
        loop.exec();

        QObject::disconnect(infoConn);
        QObject::disconnect(errorConn);
    }

    if (!result.infoOk && result.error.isEmpty())
    {
        result.error = "Stock info returned empty results";
    }

    return result;
}

int runApiTests()
{
    QStringList providers{ "sina", "tencent", "eastmoney" };
    struct MarketCase
    {
        QString name;
        QString keyword;
        QStringList codes;
    };

    QList<MarketCase> cases{
        { "A", "600519", { "SH600519", "SZ000001" } },
        { "HK", "00700", { "HK00700", "HK00941" } },
        { "US", "AAPL", { "AAPL", "TSLA" } },
    };
    const int timeoutMs = 8000;

    bool allOk = true;
    auto &service = StockApiService::instance();
    QTextStream out(stdout);

    for (const QString &provider : providers)
    {
        for (const MarketCase &market : cases)
        {
            ApiTestResult result = runProviderTest(service, provider, market.keyword, market.codes, timeoutMs);
            if (!result.searchOk || !result.infoOk)
            {
                allOk = false;
                out << "[" << provider << "][" << market.name << "] failed: " << result.error << "\n";
            }
            else
            {
                out << "[" << provider << "][" << market.name << "] search and info OK\n";
            }
        }
    }

    return allOk ? 0 : 1;
}
}  // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (app.arguments().contains("--api-test"))
    {
        return runApiTests();
    }

    const QString serverName = "StockMonitor_Local_Server";

    // Use QLocalSocket to check if another instance is running
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) {
        // Another instance is running, tell it to show its window
        socket.write("show");
        socket.waitForBytesWritten(500);
        return 0;
    }

    // No other instance running, start the server
    QLocalServer::removeServer(serverName); // cleanup from previous crashes
    QLocalServer *server = new QLocalServer(&app);
    if (!server->listen(serverName)) {
        qWarning() << "Failed to start local server:" << server->errorString();
        return 1;
    }

    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("StockMonitor");
    app.setApplicationVersion(STOCKMONITOR_VERSION);
    app.setOrganizationName("StockMonitor");
    auto config = std::make_shared<StockConfig>();
    MainWindow window(config);

    QObject::connect(server, &QLocalServer::newConnection, [server, &window]() {
        QLocalSocket *clientSocket = server->nextPendingConnection();
        QObject::connect(clientSocket, &QLocalSocket::readyRead, [clientSocket, &window]() {
            if (clientSocket->readAll() == "show") {
                window.show();
                window.raise();
                window.activateWindow();
            }
            clientSocket->disconnectFromServer();
        });
    });

    QHotkey hotkey(QKeySequence(config->hotkey), true, &app);
    QObject::connect(&hotkey, &QHotkey::activated, &window, &MainWindow::toggleActive);
    window.show();

    return app.exec();
}
