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
#include "StockSearchResultWindow.hpp"
#include "StockTableView.hpp"

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr)
        : QFrame(parent)
        , config_(std::make_shared<StockConfig>())
        , api_(StockApiService::instance().get("sina"))
        , titleBar_(new QWidget(this))
        , input_(new QLineEdit(this))
        , searchResultWidget_(new StockSearchWidget(config_, api_, this))
        , btns_({
              createTitleBarButton(":/%1/theme-toggle.svg", "Switch Theme", &MainWindow::toggleTheme),
              createTitleBarButton(":/%1/close-button.svg", "Close", &MainWindow::onCloseButtonClicked),
          })
        , table_(new StockTableView(config_, this))
        , refreshTimer_(new QTimer(this))
        , searchDelayTimer_(new QTimer(this))
    {
        setupUi();
        setupGlobalShortcut();
        setupSystemTray();
        applyTheme();
        startRefresh();

        refreshTimer_->setSingleShot(true);
        searchDelayTimer_->setSingleShot(true);

        connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::startRefresh);
        connect(api_, &IStockApiProvider::errorOccurred, this, [] {
        });
        connect(api_, &IStockApiProvider::stockInfosReceived, table_, &StockTableView::updateStocksDetail);

        connect(input_, &QLineEdit::textChanged, this, [this] {
            searchDelayTimer_->start(500);
        });
        connect(searchDelayTimer_, &QTimer::timeout, this, [this] {
            // showInputMode();
            // result_->hide();
            input_->setFocus();
            auto text = input_->text().trimmed();
            if (text.isEmpty()) return;
            qDebug() << "search stock" << api_->name() << text;
            api_->searchStocksAsync(text);
        });
        connect(api_, &IStockApiProvider::searchCompleted, searchResultWidget_, &StockSearchWidget::showResults);
        connect(searchResultWidget_, &StockSearchWidget::stockSelected, this, [this](auto &&code) {
            searchResultWidget_->hide();
            input_->clear();
            input_->setFocus();
            table_->addStock(code);
            startRefresh();
        });

        installEventFilter(this);
        titleBar_->installEventFilter(this);
        input_->installEventFilter(this);
        searchResultWidget_->installEventFilter(this);
    }

    ~MainWindow()
    {
#ifdef Q_OS_WIN
        UnregisterHotKey((HWND)winId(), 1);
#endif
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto etype = event->type();
        if (etype == QEvent::KeyPress)
        {
            auto ke = static_cast<QKeyEvent *>(event);
            auto key = ke->key();
            if (watched == input_ || watched == searchResultWidget_)
            {
                switch (key)
                {
                case Qt::Key_Escape:
                    if (input_->text().isEmpty())
                        return eventFilter(this, event);
                    else
                    {
                        input_->clear();
                    }
                    break;
                case Qt::Key_Up:
                    searchResultWidget_->selectPrevRow();
                    break;
                case Qt::Key_Down:
                    searchResultWidget_->selectNextRow();
                    break;
                case Qt::Key_Return:
                case Qt::Key_Enter:
                    searchResultWidget_->chooseCurrentRow(searchResultWidget_->currentIndex());
                    break;
                }
            }
            else if (watched == this && key == Qt::Key_Escape)
            {
                hide();
            }
        }
        else if (watched == this && etype == QEvent::Wheel && ((QWheelEvent *)event)->modifiers() & Qt::AltModifier)
        {
            auto changeOpacity = [this, currentOpacity = windowOpacity()](int angle) {
                qreal newOpacity = qBound(0.2, currentOpacity + (angle > 0 ? -0.05 : 0.05), 1.0);
                if (qAbs(newOpacity - currentOpacity) > 0.001)
                {
                    setWindowOpacity(newOpacity);
                    config_->windowOpacity = newOpacity;
                }
            };
            auto we = (QWheelEvent *)event;
            if (QPoint numPixels = we->pixelDelta(); !numPixels.isNull())
            {
                changeOpacity(numPixels.x());
            }
            else if (QPoint numDegrees = we->angleDelta() / 8; !numDegrees.isNull())
            {
                changeOpacity(numDegrees.x());
            }
        }
        else if (watched == this && (etype == QEvent::Resize || etype == QEvent::Show))
        {
            searchResultWidget_->setGeometry(table_->geometry());
        }
        else if (watched == titleBar_)
        {
            auto me = dynamic_cast<QMouseEvent *>(event);
            if (etype == QEvent::MouseButtonPress && me->button() == Qt::LeftButton)
            {
                mousePressedPosition_ = me->globalPos() - geometry().topLeft();
                return true;
            }
            else if (etype == QEvent::MouseMove && !mousePressedPosition_.isNull() && me->buttons() & Qt::LeftButton)
            {
                move(me->globalPos() - mousePressedPosition_);
                return true;
            }
            else if (etype == QEvent::MouseButtonRelease && me->button() != Qt::LeftButton)
                mousePressedPosition_ = {};
            return true;
        }
        return QWidget::eventFilter(watched, event);
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

private:
    void startRefresh()
    {
        api_->getStockInfosAsync(config_->stockCodes);
        refreshTimer_->start(1000);
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

        input_->setObjectName("searchInput");
        titleBar_->setFixedHeight(40);
        titleBar_->setObjectName("titleBar");
        QHBoxLayout *titleLayout = new QHBoxLayout(titleBar_);
        titleLayout->setContentsMargins(5, 5, 5, 5);
        titleLayout->setSpacing(0);
        titleLayout->addWidget(input_, 1);
        titleLayout->addSpacing(20);
        titleLayout->addWidget(btns_[0]);
        titleLayout->addWidget(btns_[1]);

        // connect(table_, &StockTableView::stockListChanged, this, &MainWindow::saveStockList);
        // connect(table_, &StockTableView::stockListChanged, this, &MainWindow::updateSearchWidgetExistingStocks);
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

        searchResultWidget_->raise();
        searchResultWidget_->hide();
        setWindowOpacity(config_->windowOpacity);
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
        if (!isActiveWindow())
        {
            show();
            activateWindow();
        }
        else
        {
            hide();
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
    IStockApiProvider *api_{ nullptr };

    QWidget *titleBar_;
    QLineEdit *input_;
    StockSearchWidget *searchResultWidget_;
    QList<QPushButton *> btns_;
    StockTableView *table_;

    // Window dragging
    QPoint mousePressedPosition_;

    QTimer *refreshTimer_;
    QTimer *searchDelayTimer_;
};
