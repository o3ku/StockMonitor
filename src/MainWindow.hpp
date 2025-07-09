#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QSizeGrip>
#include <QResizeEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "StockSearchWidget.hpp"
#include "StockTableView.hpp"
#include "StockSearchResult.hpp"
#include "StockInfo.hpp"
#include "StockApiService.hpp"
#include "Logger.hpp"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent), m_searchWidget(nullptr), m_tableView(nullptr),
          m_refreshTimer(new QTimer(this)), m_centralWidget(nullptr), m_mainLayout(nullptr),
          m_titleBar(nullptr), m_closeButton(nullptr), m_themeToggleButton(nullptr),
          m_resizeGrip(nullptr), m_dragging(false), m_trayIcon(nullptr), m_trayMenu(nullptr),
          m_isDarkTheme(false), m_refreshInterval(5000), m_windowOpacity(1.0),
          m_saveSettingsTimer(new QTimer(this))
    {
        setupUI();
        loadUISettings(); // Load settings before setupTimer to use correct refresh interval
        setupTimer();
        setupGlobalShortcut();
        setupSystemTray();

        // Setup settings save timer (single shot, 500ms delay)
        m_saveSettingsTimer->setSingleShot(true);
        m_saveSettingsTimer->setInterval(500);
        connect(m_saveSettingsTimer, &QTimer::timeout, this, &MainWindow::saveUISettings);

        // Install event filter to capture wheel events from child widgets
        installEventFilter(this);

        // Also install event filter on child widgets
        if (m_searchWidget)
        {
            m_searchWidget->installEventFilter(this);
        }
        if (m_tableView)
        {
            m_tableView->installEventFilter(this);
        }
        if (m_centralWidget)
        {
            m_centralWidget->installEventFilter(this);
        }

        // Connect API service signals
        connect(&StockApiService::instance(), &StockApiService::stockInfoReceived,
                this, &MainWindow::onStockInfoReceived);
        connect(&StockApiService::instance(), &StockApiService::stockInfosReceived,
                this, &MainWindow::onStockInfosReceived);
        connect(&StockApiService::instance(), &StockApiService::errorOccurred,
                this, &MainWindow::onApiError);

        loadStockList();
    }

    ~MainWindow()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->stop();
        }
        saveStockList();

#ifdef Q_OS_WIN
        // Unregister global hotkey
        UnregisterHotKey((HWND)winId(), 1);
#endif
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->geometry().contains(event->pos()))
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

    void wheelEvent(QWheelEvent *event) override
    {
        // Transparency control is now handled in eventFilter
        // Pass the event to the parent
        QMainWindow::wheelEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            // Hide window when ESC is pressed
            hide();
            Logger::info("Window hidden via ESC key");
            event->accept();
        }
        else
        {
            // Pass the event to the parent for other keys
            QMainWindow::keyPressEvent(event);
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
                if (msg->wParam == 1) // Our hotkey ID
                {
                    toggleWindowVisibility();
                    return true;
                }
            }
        }
        return QMainWindow::nativeEvent(eventType, message, result);
    }
#endif

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // Capture wheel events from child widgets when Alt is pressed
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

            if (wheelEvent->modifiers() & Qt::AltModifier)
            {
                // Handle transparency control
                qreal currentOpacity = windowOpacity();

                // Try multiple ways to get wheel direction
                int wheelDeltaY = wheelEvent->angleDelta().y();
                int wheelDeltaX = wheelEvent->angleDelta().x();
                int pixelDeltaY = wheelEvent->pixelDelta().y();
                int pixelDeltaX = wheelEvent->pixelDelta().x();

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
                    static bool lastDirection = true; // true = increase opacity
                    effectiveDelta = lastDirection ? 120 : -120;
                    lastDirection = !lastDirection; // Toggle for next time
                }

                // Up scroll (positive delta) = more opaque (less transparent)
                // Down scroll (negative delta) = more transparent
                qreal delta = effectiveDelta > 0 ? 0.05 : -0.05;
                qreal newOpacity = qBound(0.3, currentOpacity + delta, 1.0);

                // Only update if opacity actually changed
                if (qAbs(newOpacity - currentOpacity) > 0.001)
                {
                    setWindowOpacity(newOpacity);
                    m_windowOpacity = newOpacity;

                    // Use timer to delay saving settings (throttling)
                    m_saveSettingsTimer->start();
                }
                return true; // Event handled
            }
        }

        return QMainWindow::eventFilter(obj, event);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QMainWindow::resizeEvent(event);

        // Update resize grip position when window is resized
        if (m_resizeGrip)
        {
            m_resizeGrip->move(width() - m_resizeGrip->width(), height() - m_resizeGrip->height());
        }
    }

private slots:
    void onStockSelected(const StockSearchResult &result)
    {
        Logger::info(QString("MainWindow::onStockSelected - Stock selected: %1 - %2").arg(result.code(), result.name()));

        // Create StockInfo from search result
        StockInfo stockInfo;
        stockInfo.setCode(result.code());
        stockInfo.setName(result.name());
        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        // Initialize price data to 0, will be updated by timer
        stockInfo.setCurrentPrice(0.0);
        stockInfo.setYesterday(0.0);
        stockInfo.setChange(0.0);
        stockInfo.setChangePercent(0.0);
        stockInfo.setHigh(0.0);
        stockInfo.setLow(0.0);

        // Add to table
        m_tableView->addStock(stockInfo);
        saveStockList();

        // Immediately request price update for the new stock
        requestStockPrices();
        Logger::info(QString("Triggered immediate price update after adding stock: %1").arg(result.code()));
    }

    void onStockInfoReceived(const StockInfo &stockInfo)
    {
        // Single stock info received - not used in current implementation
    }

    void onStockInfosReceived(const QList<StockInfo> &stockInfos)
    {
        Logger::info(QString("Received price data for %1 stocks, triggering table refresh").arg(stockInfos.size()));

        // Update stock data with API names (don't preserve original names if they are just codes)
        QList<StockInfo> updatedStocks;
        for (const StockInfo &newStock : stockInfos)
        {
            StockInfo stock = newStock;

            // Only preserve original name if it's different from the code (i.e., it's a real name)
            for (const StockInfo &originalStock : m_tableView->stockData())
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
        m_tableView->updateStockData(updatedStocks);

        Logger::info(QString("Table data updated and refreshed for %1 stocks").arg(stockInfos.size()));
    }

    void onRefreshTimer()
    {
        requestStockPrices();
    }

    void requestStockPrices()
    {
        QList<StockInfo> stockData = m_tableView->stockData();
        if (!stockData.isEmpty())
        {
            QStringList codes;
            for (const StockInfo &stock : stockData)
            {
                codes.append(stock.code());
            }

            StockApiService::instance().getStockInfosAsync(codes);
            Logger::info(QString("Requesting real-time prices for %1 stocks").arg(codes.size()));
        }
    }

    void onApiError(const QString &error)
    {
        Logger::error(QString("API Error: %1").arg(error));
        // Show error message for critical errors
        if (error.contains("network", Qt::CaseInsensitive))
        {
            QMessageBox::warning(this, "Network Error",
                                 QString("Network request failed: %1\nPlease check your network connection.").arg(error));
        }
    }

    void onCloseButtonClicked()
    {
        // Check if Alt key is pressed
        if (QApplication::keyboardModifiers() & Qt::AltModifier)
        {
            // Alt+Click: Exit application directly
            Logger::info("Alt+Click detected on close button - exiting application");
            QApplication::quit();
        }
        else
        {
            // Normal click: Hide to system tray instead of quitting
            hide();
            if (m_trayIcon && m_trayIcon->isVisible())
            {
                Logger::info("Window hidden to system tray");
            }
        }
    }

    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger) // Left click
        {
            if (isVisible() && !isMinimized())
            {
                hide();
                Logger::info("Window hidden via tray icon click");
            }
            else
            {
                show();
                raise();
                activateWindow();
                Logger::info("Window shown via tray icon click");
            }
        }
    }

    void onTrayMenuExit()
    {
        Logger::info("Application exit requested from tray menu");
        QApplication::quit();
    }

    void onThemeToggleClicked()
    {
        m_isDarkTheme = !m_isDarkTheme;
        applyTheme();
        saveThemePreference();
        updateThemeButtonIcon();
        updateCloseButtonIcon();
    }

private:
    void setupUI()
    {
        // Make window frameless
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        setWindowTitle("Stock Monitor");

        // Set window icon for taskbar only (not window title bar)
        QIcon windowIcon(":/app-icon.svg");
        if (windowIcon.isNull())
        {
            windowIcon = QIcon(":/app-icon-256.svg");
        }
        if (!windowIcon.isNull())
        {
            // Only set for taskbar, not for window title bar
            setWindowIcon(windowIcon);
        }

        // Remove window icon from title bar for frameless window
        setWindowFlag(Qt::WindowSystemMenuHint, false);

        m_centralWidget = new QWidget(this);
        m_centralWidget->setObjectName("mainWindow");
        setCentralWidget(m_centralWidget);

        m_mainLayout = new QVBoxLayout(m_centralWidget);
        m_mainLayout->setContentsMargins(2, 2, 2, 2);
        m_mainLayout->setSpacing(0);

        // Create custom title bar
        setupTitleBar();

        // Create content area
        QWidget *contentWidget = new QWidget();
        QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(5, 5, 5, 5);
        contentLayout->setSpacing(10);

        // Create table view
        m_tableView = new StockTableView(this);
        connect(m_tableView, &StockTableView::stockListChanged,
                this, &MainWindow::saveStockList);

        // Add table view to content layout
        contentLayout->addWidget(m_tableView);

        m_mainLayout->addWidget(m_titleBar);
        m_mainLayout->addWidget(contentWidget);

        // Add resize grip in the bottom-right corner
        setupResizeGrip();
    }

    void setupTitleBar()
    {
        m_titleBar = new QWidget();
        m_titleBar->setFixedHeight(40);
        m_titleBar->setObjectName("titleBar");

        // Create search widget for title bar
        m_searchWidget = new StockSearchWidget(this);
        connect(m_searchWidget, &StockSearchWidget::stockSelected,
                this, &MainWindow::onStockSelected);
        connect(m_searchWidget, &StockSearchWidget::escapePressed,
                this, [this]()
                { hide(); Logger::info("Window hidden via ESC key from search widget"); });

        // Create theme toggle button
        m_themeToggleButton = new QPushButton();
        m_themeToggleButton->setFixedSize(30, 30);
        m_themeToggleButton->setObjectName("themeToggleButton");
        m_themeToggleButton->setToolTip("Switch Theme");
        updateThemeButtonIcon(); // Set initial icon
        connect(m_themeToggleButton, &QPushButton::clicked, this, &MainWindow::onThemeToggleClicked);

        // Create close button
        m_closeButton = new QPushButton();
        m_closeButton->setFixedSize(30, 30);
        m_closeButton->setObjectName("closeButton");
        m_closeButton->setToolTip("Close");
        updateCloseButtonIcon(); // Set initial icon
        connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::onCloseButtonClicked);

        QHBoxLayout *titleLayout = new QHBoxLayout(m_titleBar);
        titleLayout->setContentsMargins(5, 5, 5, 5);
        titleLayout->setSpacing(0);
        titleLayout->addWidget(m_searchWidget, 1);
        titleLayout->addSpacing(20);
        titleLayout->addWidget(m_themeToggleButton);
        titleLayout->addWidget(m_closeButton);
    }

    void setupResizeGrip()
    {
        // Create resize grip for bottom-right corner
        m_resizeGrip = new QSizeGrip(this);
        m_resizeGrip->setFixedSize(16, 16);

        // Position the resize grip in the bottom-right corner
        m_resizeGrip->move(width() - m_resizeGrip->width(), height() - m_resizeGrip->height());

        // Make sure resize grip stays on top
        m_resizeGrip->raise();
    }

    void setupTimer()
    {
        m_refreshTimer->setInterval(m_refreshInterval);
        connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);
        m_refreshTimer->start();
    }

    void setupGlobalShortcut()
    {
#ifdef Q_OS_WIN
        // Load hotkey configuration from settings
        QString exeDir = QApplication::applicationDirPath();
        QString iniFile = exeDir + "/stocks.ini";

        QSettings settings(iniFile, QSettings::IniFormat);

        // Default hotkey: Ctrl+Alt+S
        QString keyString = settings.value("Hotkey/Key", "Ctrl+Alt+S").toString();

        // Save default value if not exist
        if (!settings.contains("Hotkey/Key"))
        {
            settings.setValue("Hotkey/Key", keyString);
            settings.sync();
        }

        // Parse hotkey string (case insensitive)
        QString lowerKey = keyString.toLower();
        bool useCtrl = lowerKey.contains("ctrl");
        bool useAlt = lowerKey.contains("alt");
        bool useShift = lowerKey.contains("shift");

        // Extract the key character (last part after '+')
        QStringList parts = keyString.split('+');
        QString keyChar = parts.isEmpty() ? "S" : parts.last().trimmed().toUpper();
        if (keyChar.length() > 1)
        {
            keyChar = keyChar.left(1); // Take only first character
        }

        UINT modifiers = 0;
        if (useCtrl)
            modifiers |= MOD_CONTROL;
        if (useAlt)
            modifiers |= MOD_ALT;
        if (useShift)
            modifiers |= MOD_SHIFT;

        UINT vkCode = keyChar.isEmpty() ? 'S' : keyChar.at(0).toLatin1();

        if (!RegisterHotKey((HWND)winId(), 1, modifiers, vkCode))
        {
            Logger::error(QString("Failed to register global hotkey %1").arg(keyString));
        }
        else
        {
            Logger::info(QString("Global hotkey %1 registered successfully").arg(keyString));
        }
#endif
    }

    void setupSystemTray()
    {
        Logger::info("Setting up system tray...");

        // Check if system tray is available
        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
            Logger::error("System tray is not available on this system");
            return;
        }

        Logger::info("System tray is available");

        // Create tray icon
        m_trayIcon = new QSystemTrayIcon(this);

        // Set icon from resources
        QIcon icon(":/app-icon.svg");
        if (icon.isNull())
        {
            // Try 256px fallback
            icon = QIcon(":/app-icon-256.svg");
        }
        if (icon.isNull())
        {
            // Create a simple fallback icon if resource not found
            QPixmap pixmap(16, 16);
            pixmap.fill(Qt::green);
            QPainter painter(&pixmap);
            painter.setPen(Qt::white);
            painter.drawText(pixmap.rect(), Qt::AlignCenter, "$");
            icon = QIcon(pixmap);
            Logger::error("Using fallback icon for system tray");
        }
        m_trayIcon->setIcon(icon);
        m_trayIcon->setToolTip("Stock Monitor");

        // Create tray menu
        m_trayMenu = new QMenu(this);

        QAction *exitAction = new QAction("Exit", this);
        connect(exitAction, &QAction::triggered, this, &MainWindow::onTrayMenuExit);
        m_trayMenu->addAction(exitAction);

        m_trayIcon->setContextMenu(m_trayMenu);

        // Connect tray icon signals
        connect(m_trayIcon, &QSystemTrayIcon::activated,
                this, &MainWindow::onTrayIconActivated);

        // Show tray icon
        m_trayIcon->show();
        Logger::info("System tray icon initialized and shown");
    }

    void toggleWindowVisibility()
    {
        if (isVisible() && !isMinimized())
        {
            // Hide window
            hide();
            Logger::info("Window hidden via global hotkey");
        }
        else
        {
            // Show and activate window
            show();
            raise();
            activateWindow();
            Logger::info("Window shown via global hotkey");
        }
    }

    void saveStockList()
    {
        // Save INI file in exe directory
        QString exeDir = QApplication::applicationDirPath();
        QString iniFile = exeDir + "/stocks.ini";

        QSettings settings(iniFile, QSettings::IniFormat);
        QStringList stockCodes;

        QList<StockInfo> stockData = m_tableView->stockData();
        for (const StockInfo &stock : stockData)
        {
            stockCodes.append(stock.code());
        }

        // Save as comma-separated string to avoid @Invalid() when empty
        QString stockCodesString = stockCodes.isEmpty() ? QString("") : stockCodes.join(", ");
        settings.setValue("StockList/StockCodes", stockCodesString);
        settings.sync();
        Logger::info(QString("Saved %1 stock codes to %2").arg(stockCodes.size()).arg(iniFile));
    }

    void loadStockList()
    {
        // Load INI file from exe directory
        QString exeDir = QApplication::applicationDirPath();
        QString iniFile = exeDir + "/stocks.ini";

        QSettings settings(iniFile, QSettings::IniFormat);

        // Load as string and split by comma to handle both old and new formats
        QString stockCodesString = settings.value("StockList/StockCodes").toString();
        QStringList stockCodes;

        if (!stockCodesString.isEmpty())
        {
            stockCodes = stockCodesString.split(",", Qt::SkipEmptyParts);
            // Trim whitespace from each code
            for (QString &code : stockCodes)
            {
                code = code.trimmed();
            }
        }

        if (stockCodes.isEmpty())
        {
            // If no saved stocks, add sample stocks
            Logger::info(QString("No saved stocks found, added sample stocks. Config file: %1").arg(iniFile));
            // Immediately request price data for sample stocks
            requestStockPrices();
            return;
        }

        for (const QString &code : stockCodes)
        {
            QString trimmedCode = code.trimmed(); // Remove any whitespace
            if (trimmedCode.isEmpty())
                continue;

            StockInfo stockInfo;
            stockInfo.setCode(trimmedCode);
            stockInfo.setName(trimmedCode); // Will be updated when data is fetched
            stockInfo.setLastUpdate(QDateTime::currentDateTime());

            // Initialize price data to 0, will be updated by timer
            stockInfo.setCurrentPrice(0.0);
            stockInfo.setYesterday(0.0);
            stockInfo.setChange(0.0);
            stockInfo.setChangePercent(0.0);
            stockInfo.setHigh(0.0);
            stockInfo.setLow(0.0);

            m_tableView->addStock(stockInfo);
        }

        Logger::info(QString("Loaded %1 stock codes from %2").arg(stockCodes.size()).arg(iniFile));

        // Immediately request price data after loading stocks
        requestStockPrices();
    }

    void applyTheme()
    {
        QString baseStyleSheet;
        QString themeStyleSheet;

        // Load base styles
        QFile baseFile(":/base.css");
        if (baseFile.open(QFile::ReadOnly | QFile::Text))
        {
            QTextStream stream(&baseFile);
            baseStyleSheet = stream.readAll();
        }
        else
        {
            Logger::error("Failed to load base.css");
        }

        // Load theme-specific styles
        QString themeFile = m_isDarkTheme ? ":/dark.css" : ":/light.css";
        QFile file(themeFile);
        if (file.open(QFile::ReadOnly | QFile::Text))
        {
            QTextStream stream(&file);
            themeStyleSheet = stream.readAll();
        }
        else
        {
            Logger::error(QString("Failed to load theme file: %1").arg(themeFile));
        }

        // Combine base and theme styles
        QString combinedStyleSheet = baseStyleSheet + "\n" + themeStyleSheet;
        qApp->setStyleSheet(combinedStyleSheet);
        Logger::info(QString("Applied theme: %1").arg(m_isDarkTheme ? "Dark" : "Light"));
    }

    void saveUISettings()
    {
        QString exeDir = QApplication::applicationDirPath();
        QString iniFile = exeDir + "/stocks.ini";

        QSettings settings(iniFile, QSettings::IniFormat);

        // Save theme
        QString themeValue = m_isDarkTheme ? "dark" : "light";
        settings.setValue("UI/Theme", themeValue);

        // Save window opacity (rounded to 2 decimal places)
        settings.setValue("UI/WindowOpacity", QString::number(m_windowOpacity, 'f', 2));

        // Save refresh interval
        settings.setValue("UI/RefreshInterval", m_refreshInterval);

        settings.sync();
        Logger::info(QString("Saved UI settings: Theme=%1, Opacity=%2, RefreshInterval=%3ms")
                         .arg(themeValue)
                         .arg(m_windowOpacity, 0, 'f', 2)
                         .arg(m_refreshInterval));
    }

    void saveThemePreference()
    {
        saveUISettings();
    }

    void scheduleSettingsSave()
    {
        // Restart the timer to delay saving (throttling mechanism)
        m_saveSettingsTimer->start();
    }

    void loadUISettings()
    {
        QString exeDir = QApplication::applicationDirPath();
        QString iniFile = exeDir + "/stocks.ini";

        QSettings settings(iniFile, QSettings::IniFormat);

        // Load theme
        QString themeValue = settings.value("UI/Theme", "light").toString().toLower();
        m_isDarkTheme = (themeValue == "dark");

        // Load window opacity
        m_windowOpacity = settings.value("UI/WindowOpacity", 1.0).toReal();
        m_windowOpacity = qBound(0.3, m_windowOpacity, 1.0); // Ensure valid range
        setWindowOpacity(m_windowOpacity);

        // Load refresh interval
        m_refreshInterval = settings.value("UI/RefreshInterval", 5000).toInt();
        m_refreshInterval = qMax(1000, m_refreshInterval); // Minimum 1 second

        applyTheme();
        updateThemeButtonIcon();
        updateCloseButtonIcon();

        Logger::info(QString("Loaded UI settings: Theme=%1, Opacity=%2, RefreshInterval=%3ms")
                         .arg(themeValue)
                         .arg(m_windowOpacity, 0, 'f', 2)
                         .arg(m_refreshInterval));
    }

    void loadThemePreference()
    {
        loadUISettings();
    }

    void updateThemeButtonIcon()
    {
        if (m_themeToggleButton)
        {
            QString iconPath = m_isDarkTheme ? ":/theme-toggle-light.svg" : ":/theme-toggle.svg";

            QIcon themeIcon(iconPath);
            m_themeToggleButton->setIcon(themeIcon);
            m_themeToggleButton->setIconSize(QSize(16, 16));
            m_themeToggleButton->setToolTip(m_isDarkTheme ? "Switch to Light Theme" : "Switch to Dark Theme");
        }
    }

    void updateCloseButtonIcon()
    {
        if (m_closeButton)
        {
            QString iconPath = m_isDarkTheme ? ":/close-button-light.svg" : ":/close-button.svg";
            QIcon closeIcon(iconPath);
            m_closeButton->setIcon(closeIcon);
            m_closeButton->setIconSize(QSize(16, 16));
        }
    }

    // UI Components
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    QWidget *m_titleBar;
    QPushButton *m_closeButton;
    QPushButton *m_themeToggleButton;
    QSizeGrip *m_resizeGrip;

    // Main widgets
    StockSearchWidget *m_searchWidget;
    StockTableView *m_tableView;
    QTimer *m_refreshTimer;

    // Window dragging
    bool m_dragging;
    QPoint m_dragPosition;

    // System tray
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

    // Theme
    bool m_isDarkTheme;

    // UI Settings
    int m_refreshInterval;
    qreal m_windowOpacity;

    // Event throttling
    QTimer *m_saveSettingsTimer;
};

#endif // MAINWINDOW_HPP
