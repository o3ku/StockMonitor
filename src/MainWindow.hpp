#pragma once

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMetaEnum>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizeGrip>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#ifdef Q_OS_WIN
    #include <windows.h>
#endif

#include "Logger.hpp"
#include "StockApiService.hpp"
#include "StockConfig.hpp"
#include "StockInfo.hpp"
#include "StockSearchResult.hpp"
#include "StockSearchWidget.hpp"
#include "StockTableView.hpp"

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QFrame(parent)
        , config_(std::make_shared<StockConfig>())
        , titleBar_(new QWidget(this))
        , searchBox_(new QLineEdit(this))
        , btns_({
              createTitleBarButton(":/%1/theme-toggle.svg", "Switch Theme", &MainWindow::toggleTheme),
              createTitleBarButton(":/%1/close-button.svg", "Close", &MainWindow::onCloseButtonClicked),
          })
        , table_(new StockTableView(this))
        , model_(new StockModel(config_, this))
        , m_refreshTimer(new QTimer(this))
        , m_dragging(false)
        , m_refreshInterval(5000)
        , m_saveSettingsTimer(new QTimer(this))
    {
        setupUi();
        setupGlobalShortcut();
        setupSystemTray();
        applyTheme();

        // loadUISettings();  // Load settings before setupTimer to use correct refresh interval
        // setupTimer();

        // Setup settings save timer (single shot, 500ms delay)
        // m_saveSettingsTimer->setSingleShot(true);
        // m_saveSettingsTimer->setInterval(500);
        // connect(m_saveSettingsTimer, &QTimer::timeout, this, &MainWindow::saveUISettings);

        // // Install event filter to capture wheel events from child widgets
        // installEventFilter(this);

        // // Also install event filter on child widgets
        // if (searchBox_)
        // {
        //     searchBox_->installEventFilter(this);
        // }
        // if (table_)
        // {
        //     table_->installEventFilter(this);
        // }
        // if (m_centralWidget)
        // {
        //     m_centralWidget->installEventFilter(this);
        // }

        // // Connect API service signals

        // connect(&StockApiService::instance(), &StockApiService::stockInfosReceived, this, &MainWindow::onStockInfosReceived);
        // connect(&StockApiService::instance(), &StockApiService::errorOccurred, this, &MainWindow::onApiError);

        // StockApiService::instance().setApiSource(ConfigManager::instance().getConfig().apiSource);
        // loadStockList();
    }

    ~MainWindow()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->stop();
        }
        saveStockList();

#ifdef Q_OS_WIN
        UnregisterHotKey((HWND)winId(), 1);
#endif
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && titleBar_ && titleBar_->geometry().contains(event->pos()))
        {
            m_dragging = true;
            m_dragPosition = event->globalPos() - frameGeometry().topLeft();
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons() & Qt::LeftButton && m_dragging)
        {
            move(event->globalPos() - m_dragPosition);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_dragging = false;
            event->accept();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            hide();
            event->accept();
        }
        else
        {
            QWidget::keyPressEvent(event);
        }
    }

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override
    {
        if (eventType == "windows_generic_MSG")
        {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_HOTKEY)
            {
                if (msg->wParam == 1)  // Our hotkey ID
                {
                    toggleWindowVisibility();
                    return true;
                }
            }
        }
        return QWidget::nativeEvent(eventType, message, result);
    }
#endif

    void wheelEvent(QWheelEvent *event) override
    {
        if (event->modifiers() & Qt::AltModifier)
        {
            // Handle transparency control
            qreal currentOpacity = windowOpacity();

            // Try multiple ways to get wheel direction
            int wheelDeltaY = event->angleDelta().y();
            int wheelDeltaX = event->angleDelta().x();
            int pixelDeltaY = event->pixelDelta().y();
            int pixelDeltaX = event->pixelDelta().x();

            // Determine effective wheel direction
            int effectiveDelta = 0;
            if (wheelDeltaY != 0)
            {
                effectiveDelta = wheelDeltaY;
            }
            else if (pixelDeltaY != 0)
            {
                effectiveDelta = pixelDeltaY;
            }
            else if (wheelDeltaX != 0)
            {
                effectiveDelta = wheelDeltaX;
            }
            else if (pixelDeltaX != 0)
            {
                effectiveDelta = pixelDeltaX;
            }
            else
            {
                // If all wheel data is 0, use a simple toggle approach
                // This is a fallback for when wheel data is lost
                static bool lastDirection = true;  // true = increase opacity
                effectiveDelta = lastDirection ? 120 : -120;
                lastDirection = !lastDirection;  // Toggle for next time
            }

            // Up scroll (positive delta) = more opaque (less transparent)
            // Down scroll (negative delta) = more transparent
            qreal delta = effectiveDelta > 0 ? 0.05 : -0.05;
            qreal newOpacity = qBound(0.3, currentOpacity + delta, 1.0);

            // Only update if opacity actually changed
            if (qAbs(newOpacity - currentOpacity) > 0.001)
            {
                setWindowOpacity(newOpacity);
                config_->windowOpacity = newOpacity;

                // Use timer to delay saving settings (throttling)
                m_saveSettingsTimer->start();
            }
        }
    }

private:
    StockInfo createEmptyStockInfo(const QString &code, const QString &name = QString())
    {
        StockInfo stockInfo;
        stockInfo.setCode(code);
        stockInfo.setName(name.isEmpty() ? code : name);
        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        // Initialize price data to 0, will be updated by timer
        stockInfo.setCurrentPrice(0.0);
        stockInfo.setYesterday(0.0);
        stockInfo.setChange(0.0);
        stockInfo.setChangePercent(0.0);
        stockInfo.setHigh(0.0);
        stockInfo.setLow(0.0);

        return stockInfo;
    }

    void updateSearchWidgetExistingStocks()
    {
        if (searchBox_ && table_)
        {
            QStringList stockCodes;
            QList<StockInfo> stocks = table_->stockData();
            for (const StockInfo &stock : stocks)
            {
                stockCodes.append(stock.code());
            }
            // searchBox_->setExistingStocks(stockCodes);
        }
    }

private slots:
    void onStockSelected(const StockSearchResult &result)
    {
        StockInfo stockInfo = createEmptyStockInfo(result.code(), result.name());
        table_->addStock(stockInfo);
        saveStockList();
        updateSearchWidgetExistingStocks();
    }

    void onStockInfoReceived(const StockInfo &stockInfo)
    {
        // Single stock info received - not used in current implementation
    }

    void onStockInfosReceived(const QList<StockInfo> &stockInfos)
    {

        // Update stock data with API names (don't preserve original names if they are just codes)
        QList<StockInfo> updatedStocks;
        for (const StockInfo &newStock : stockInfos)
        {
            StockInfo stock = newStock;

            // Only preserve original name if it's different from the code (i.e., it's a real name)
            for (const StockInfo &originalStock : table_->stockData())
            {
                if (originalStock.code() == newStock.code())
                {
                    // If original name is the same as code, use the new name from API
                    if (originalStock.name() != originalStock.code() && !newStock.name().isEmpty())
                    {
                        // Keep the original name if it's a real name and API name is not empty
                        stock.setName(originalStock.name());
                    }
                    // Otherwise use the name from API (newStock already has it)
                    break;
                }
            }
            updatedStocks.append(stock);
        }

        // Update table data - this will trigger automatic table refresh via dataChanged signal
        table_->updateStockData(updatedStocks);
    }

    void onRefreshTimer()
    {
        QList<StockInfo> stockData = table_->stockData();
        if (!stockData.isEmpty())
        {
            QStringList codes;
            for (const StockInfo &stock : stockData)
            {
                codes.append(stock.code());
            }

            // StockApiService::instance().getStockInfosAsync(codes);
        }
    }

    void onApiError(const QString &error)
    {
        Logger::error(QString("API Error: %1").arg(error));
        // Show error message for critical errors
        if (error.contains("network", Qt::CaseInsensitive))
        {
            QMessageBox::warning(this, "Network Error", QString("Network request failed: %1\nPlease check your network connection.").arg(error));
        }
    }

    void onCloseButtonClicked()
    {
        // Check if Alt key is pressed
        if (QApplication::keyboardModifiers() & Qt::AltModifier)
        {
            // Alt+Click: Exit application directly
            QApplication::quit();
        }
        else
        {
            // Normal click: Hide to system tray instead of quitting
            hide();
        }
    }

    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger)  // Left click
        {
            if (isVisible() && !isMinimized())
            {
                hide();
            }
            else
            {
                show();
                raise();
                activateWindow();
            }
        }
    }

    void onTrayMenuExit()
    {
        QApplication::quit();
    }

private:
    template <class F>
    auto createTitleBarButton(const QString &iconPath, const QString &tooltip, F &&func)
    {
        auto btn = new QPushButton(this);
        btn->setProperty("iconpath", iconPath);
        btn->setFixedSize(30, 30);
        btn->setToolTip(tooltip);
        connect(btn, &QPushButton::clicked, this, func);
        return btn;
    }

    void setupUi()
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setWindowFlag(Qt::WindowSystemMenuHint, false);
        setWindowTitle("Stock Monitor");
        setWindowIcon(QIcon(":/app-icon.svg"));
        setMinimumSize(260, 360);

        titleBar_->setFixedHeight(40);
        titleBar_->setObjectName("titleBar");
        QHBoxLayout *titleLayout = new QHBoxLayout(titleBar_);
        titleLayout->setContentsMargins(5, 5, 5, 5);
        titleLayout->setSpacing(0);
        titleLayout->addWidget(searchBox_, 1);
        titleLayout->addSpacing(20);
        titleLayout->addWidget(btns_[0]);
        titleLayout->addWidget(btns_[1]);

        // connect(table_, &StockTableView::stockListChanged, this, &MainWindow::saveStockList);
        // connect(table_, &StockTableView::stockListChanged, this, &MainWindow::updateSearchWidgetExistingStocks);
        table_->setModel(model_);
        auto wrap = new QVBoxLayout;
        wrap->setContentsMargins(5, 5, 5, 5);
        wrap->addWidget(table_, 1);

        auto statusBar = new QStatusBar(this);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(5);
        layout->addWidget(titleBar_);
        layout->addLayout(wrap, 1);
        layout->addWidget(statusBar);

        setWindowOpacity(config_->windowOpacity);
    }

    void setupTimer()
    {
        m_refreshTimer->setInterval(m_refreshInterval);
        connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);

        QTimer::singleShot(1000, this, [this]() {
            m_refreshTimer->start();
        });
    }

    void setupGlobalShortcut()
    {
#ifdef Q_OS_WIN

        auto upperKey = config_->hotkeyKey.toUpper();
        QString keyChar = upperKey.split('+', Qt::SkipEmptyParts).last().trimmed();

        UINT modifiers = 0;
        if (upperKey.contains("CTRL")) modifiers |= MOD_CONTROL;
        if (upperKey.contains("ALT")) modifiers |= MOD_ALT;
        if (upperKey.contains("SHIFT")) modifiers |= MOD_SHIFT;

        UINT vkCode = 0;
        if (keyChar.length() == 1)
        {
            vkCode = keyChar.at(0).toUpper().toLatin1();
        }
        else
        {
            static QMap<QString, UINT> key2code{
                { "F1", VK_F1 },
                { "F2", VK_F2 },
                { "F3", VK_F3 },
                { "F4", VK_F4 },
                { "F5", VK_F5 },
                { "F6", VK_F6 },
                { "F7", VK_F7 },
                { "F8", VK_F8 },
                { "F9", VK_F9 },
                { "F10", VK_F10 },
                { "F11", VK_F11 },
                { "F12", VK_F12 },
                { "SPACE", VK_SPACE },
                { "ENTER", VK_RETURN },
                { "ESCAPE", VK_ESCAPE },
            };
            vkCode = key2code.value(keyChar, 'S');
        }

        if (!RegisterHotKey((HWND)winId(), 1, modifiers, vkCode))
        {
            DWORD error = GetLastError();
            Logger::error(QString("Failed to register global hotkey %1 (Error: %2)").arg(config_->hotkeyKey).arg(error));
        }
        else
        {
            Logger::info(QString("Successfully registered global hotkey: %1").arg(config_->hotkeyKey));
        }
#endif
    }

    void setupSystemTray()
    {
        // Check if system tray is available
        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
            Logger::error("System tray is not available on this system");
            return;
        }

        auto menu = new QMenu(this);
        QAction *exitAction = new QAction("Exit", this);
        menu->addAction(exitAction);
        connect(exitAction, &QAction::triggered, this, &MainWindow::onTrayMenuExit);

        auto tray = new QSystemTrayIcon(this);
        tray->setIcon(windowIcon());
        tray->setToolTip(windowTitle());
        tray->setContextMenu(menu);
        tray->show();

        connect(tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    }

    void toggleWindowVisibility()
    {
        if (isVisible() && !isMinimized())
        {
            hide();
        }
        else
        {
            show();
            raise();
            activateWindow();
        }
    }

    void saveStockList()
    {
        // QStringList stockCodes;
        // QList<StockInfo> stockData = table_->stockData();
        // for (const StockInfo &stock : stockData)
        // {
        //     stockCodes.append(stock.code());
        // }

        // // 使用 ConfigManager 保存股票列表
        // ConfigManager::instance().setStockCodes(stockCodes);
    }

    void loadStockList()
    {
        // // 使用 ConfigManager 加载股票列表
        // QStringList stockCodes = ConfigManager::instance().getConfig().stockCodes;

        // if (stockCodes.isEmpty())
        // {
        //     requestStockPrices();
        //     return;
        // }

        // for (const QString &code : stockCodes)
        // {
        //     QString trimmedCode = code.trimmed();
        //     if (trimmedCode.isEmpty()) continue;

        //     StockInfo stockInfo = createEmptyStockInfo(trimmedCode);
        //     table_->addStock(stockInfo);
        // }

        // updateSearchWidgetExistingStocks();
        // requestStockPrices();
    }

    void toggleTheme()
    {
        auto btn = sender();
        auto index = btn->property("index").toInt();
        auto next = (index + 1) % config_->availableThemes.size();
        btn->setProperty("index", next);
        applyTheme(config_->availableThemes.value(next));
    }

    void applyTheme(const QString &theme = "")
    {
        if (!theme.isEmpty()) config_->theme = theme;

        updateButtonIcons();

        QStringList css;
        for (auto &&path : QStringList{ ":/base.css", QString(":/%1/style.css").arg(config_->theme) })
        {
            QFile f(path);
            if (f.open(QFile::ReadOnly | QFile::Text))
            {
                css << f.readAll();
            }
        };

        style()->unpolish(this);
        setStyleSheet(css.join("\n"));
        style()->polish(this);
    }

    void updateButtonIcons()
    {
        for (auto &btn : btns_)
        {
            btn->setIconSize(QSize(16, 16));
            btn->setIcon(QIcon(btn->property("iconpath").toString().arg(config_->theme)));
        }
    }

private:
    std::shared_ptr<StockConfig> config_;

    QWidget *titleBar_;
    QLineEdit *searchBox_;
    QList<QPushButton *> btns_;
    StockTableView *table_;
    StockModel *model_;

    QTimer *m_refreshTimer;

    // Window dragging
    bool m_dragging;
    QPoint m_dragPosition;

    // System tray
    int m_refreshInterval;

    // Event throttling
    QTimer *m_saveSettingsTimer;
};
