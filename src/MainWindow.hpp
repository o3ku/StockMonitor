#pragma once

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "Logger.hpp"
#include "StockApiService.hpp"
#include "StockConfig.hpp"
#include "StockInfo.hpp"
#include "StockSearchResultTable.hpp"
#include "StockTableView.hpp"

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    explicit MainWindow(const std::shared_ptr<StockConfig> &config, QWidget *parent = nullptr)
        : QFrame(parent)
        , config_(config)
        , apiService_(StockApiService::instance())
        , titleBar_(new QWidget(this))
        , input_(new QLineEdit(this))
        , searchResultWidget_(new StockSearchResultTable(config_, this))
        , btns_({
              createTitleBarButton(":/%1/close-button.svg", "Close", &MainWindow::onCloseButtonClicked),
              createTitleBarButton(":/%1/settings.svg", "Settings", &MainWindow::openSettings),
          })
        , table_(new StockTableView(config_, this))
        , settingsOverlay_(new QWidget(this))
        , settingsPanel_(new QFrame(settingsOverlay_))
        , settingsApiSource_(new QComboBox(settingsPanel_))
        , settingsRefreshSpin_(new QSpinBox(settingsPanel_))
        , settingsAutoHideCombo_(new QComboBox(settingsPanel_))
        , settingsThemeCombo_(new QComboBox(settingsPanel_))
        , settingsOpacitySpin_(new QDoubleSpinBox(settingsPanel_))
        , settingsRowHeightSpin_(new QSpinBox(settingsPanel_))
        , autoHideTimer_(new QTimer(this))
        , refreshTimer_(new QTimer(this))
        , searchDelayTimer_(new QTimer(this))
        , labels_({
              { "SH000001", { "沪", new QLabel(this) } },
              { "SZ399001", { "深", new QLabel(this) } },
              { "SZ399006", { "创", new QLabel(this) } },
          })
    {
        setupUi();
        setupSettingsOverlay();
        setupSystemTray();
        applyTheme();
        connect(&apiService_, &StockApiService::errorOccurred, this, &MainWindow::onApiError);
        connect(&apiService_, &StockApiService::stockInfosReceived, this, &MainWindow::handleStockInfosReceived);
        connect(&apiService_, &StockApiService::searchCompleted, this, [this](const QList<StockInfo> &results) {
            if (!input_->text().isEmpty()) {
                searchResultWidget_->showResults(results);
            }
        });
        applyApiSource();

        setAttribute(Qt::WA_DeleteOnClose, false);

        searchDelayTimer_->setSingleShot(true);
        connect(searchDelayTimer_, &QTimer::timeout, [this] {
            focusSearchInput();
            auto text = input_->text().trimmed();
            if (text.isEmpty())
            {
                hideSearchResults();
                return;
            }
            apiService_.searchStocksAsync(text);
        });
        connect(input_, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (text.isEmpty())
            {
                hideSearchResults();
                return;
            }
            searchDelayTimer_->start(500);
        });
        connect(searchResultWidget_, &StockSearchResultTable::stockSelected, this, [this](auto &&code) {
            hideSearchResults();
            input_->clear();
            focusSearchInput();
            table_->addStock(code);
            startRefresh();
        });

        refreshTimer_->setSingleShot(true);
        connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::startRefresh);
        autoHideTimer_->setSingleShot(true);
        connect(autoHideTimer_, &QTimer::timeout, this, &MainWindow::onAutoHideTimeout);
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *curr) {
            if (!isAutoHideEnabled()) return;
            while (curr != nullptr)
            {
                if (curr == this)
                {
                    if (cursorInside_)
                    {
                        autoHideTimer_->stop();
                    }
                    else
                    {
                        startAutoHideTimer();
                    }
                    return;
                }
                curr = curr->parentWidget();
            }
            startAutoHideTimer();
        });

        installEventFilter(this);
        titleBar_->installEventFilter(this);
        input_->installEventFilter(this);
        table_->installEventFilter(this);
        searchResultWidget_->installEventFilter(this);
    }

    ~MainWindow()
    {
    }

public:
    void toggleActive()
    {
        if (isActiveWindow())
        {
            hide();
        }
        else
        {
            input_->setFocus();
            activateWindow();
            show();
        }
    };

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto etype = event->type();
        if (shouldResetAutoHideTimer(etype))
        {
            startAutoHideTimer();
        }
        if (etype == QEvent::KeyPress)
        {
            autoHideTimer_->stop();
            auto ke = static_cast<QKeyEvent *>(event);
            auto key = ke->key();
            if ((ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == Qt::NoModifier)
            {
                const QString text = ke->text();
                if (!text.isEmpty())
                {
                    const QChar ch = text.at(0);
                    if (ch.isLetterOrNumber() && ch.unicode() < 128 && !input_->hasFocus())
                    {
                        focusSearchInput();
                        input_->insert(text);
                        return true;
                    }
                }
            }
            if (settingsOverlay_->isVisible() && key == Qt::Key_Escape)
            {
                closeSettingsOverlay();
                return true;
            }
            if (watched == input_ || watched == searchResultWidget_ || watched == this || watched == table_)
            {
                switch (key)
                {
                case Qt::Key_Escape:
                    if (input_->text().isEmpty())
                    {
                        hide();
                        return true;
                    }
                    else
                    {
                        input_->clear();
                        hideSearchResults();
                        return true;
                    }
                    break;
                case Qt::Key_Up:
                    if (searchResultWidget_->isVisible())
                        searchResultWidget_->selectPrevRow();
                    else
                        table_->selectPrevRow();
                    return true;
                case Qt::Key_Down:
                    if (searchResultWidget_->isVisible())
                        searchResultWidget_->selectNextRow();
                    else
                        table_->selectNextRow();
                    return true;
                case Qt::Key_Return:
                case Qt::Key_Enter:
                    if (searchResultWidget_->isVisible()) searchResultWidget_->chooseCurrentRow(searchResultWidget_->currentIndex());
                    return true;
                case Qt::Key_Delete:
                    if (input_->text().isEmpty())
                    {
                        table_->removeCurrent();
                        return true;
                    }
                    break;
                }
            }
        }
        else if (watched == this)
        {
            if (etype == QEvent::Wheel && ((QWheelEvent *)event)->modifiers() & Qt::AltModifier)
            {
                auto changeOpacity = [this, currentOpacity = windowOpacity()](int angle) {
                    qreal newOpacity = qBound(0.2, currentOpacity + (angle > 0 ? +0.05 : -0.05), 1.0);
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
            else if (etype == QEvent::Resize)
            {
                syncSearchResultsGeometry();
                syncSettingsOverlayGeometry();
            }
            else if (etype == QEvent::Show)
            {
                syncSearchResultsGeometry();
                syncSettingsOverlayGeometry();
                if (!config_->lastGeometry.isEmpty()) restoreGeometry(config_->lastGeometry);
                cursorInside_ = rect().contains(mapFromGlobal(QCursor::pos()));
                if (cursorInside_)
                {
                    autoHideTimer_->stop();
                }
                else
                {
                    startAutoHideTimer();
                }
            }
            else if (etype == QEvent::Close || etype == QEvent::Hide)
            {
                config_->lastGeometry = saveGeometry();
                hide();
                return true;
            }
        }
        else if (watched == titleBar_)
        {
            static QPoint mousePressedPosition;
            auto me = dynamic_cast<QMouseEvent *>(event);
            if (etype == QEvent::MouseButtonPress && me->button() == Qt::LeftButton)
            {
                mousePressedPosition = me->globalPos() - geometry().topLeft();
                return true;
            }
            else if (etype == QEvent::MouseMove && !mousePressedPosition.isNull() && me->buttons() & Qt::LeftButton)
            {
                move(me->globalPos() - mousePressedPosition);
                return true;
            }
            else if (etype == QEvent::MouseButtonRelease && me->button() != Qt::LeftButton)
                mousePressedPosition = {};
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void enterEvent(QEvent *event) override
    {
        cursorInside_ = true;
        autoHideTimer_->stop();
        QFrame::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        cursorInside_ = false;
        startAutoHideTimer();
        QFrame::leaveEvent(event);
    }

private:
    bool isAutoHideEnabled() const
    {
        return config_->autoHideDelay > 0 && !suspendAutoHide_;
    }

    void startAutoHideTimer()
    {
        if (isAutoHideEnabled()) autoHideTimer_->start(config_->autoHideDelay);
    }

    bool shouldResetAutoHideTimer(QEvent::Type type) const
    {
        if (!isAutoHideEnabled()) return false;
        switch (type)
        {
        case QEvent::KeyRelease:
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Wheel:
            return true;
        default:
            return false;
        }
    }

    void hideSearchResults()
    {
        searchResultWidget_->hide();
    }

    void focusSearchInput()
    {
        input_->setFocus();
    }

    void syncSearchResultsGeometry()
    {
        searchResultWidget_->setGeometry(table_->geometry());
    }

    void syncSettingsOverlayGeometry()
    {
        settingsOverlay_->setGeometry(rect());
    }

    void onAutoHideTimeout()
    {
        if (!isAutoHideEnabled()) return;
        if (cursorInside_)
        {
            startAutoHideTimer();
            return;
        }
        hide();
    }

    void startRefresh()
    {
        apiService_.getStockInfosAsync(config_->stockCodes + labels_.keys());
        if (config_->refreshInterval >= 100)
        {
            refreshTimer_->start(config_->refreshInterval);
        }
    }

    void handleStockInfosReceived(const QList<StockInfo> &stockInfos)
    {
        for (const StockInfo &info : stockInfos)
        {
            auto &&[title, label] = labels_.value(info.code());
            if (label != nullptr)
            {
                label->setText(QString("<span style=\"color:%0\">%1</span><span style=\"color:%2\">%3</span>")
                        .arg(config_->getTextColor().name())
                        .arg(title)
                        .arg(config_->getPriceColor(info.isUp()).name())
                        .arg(info.changePercentText()));
                // 设置tooltip，与表格保持一致
                label->setToolTip(info.detailedTooltip());
            }
        }

        table_->updateStocksDetail(stockInfos);
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
            QApplication::quit();
        }
        else
        {
            // Normal click: Hide to system tray instead of quitting
            hide();
        }
    }

    void openSettings()
    {
        if (settingsOverlay_->isVisible())
        {
            closeSettingsOverlay();
            return;
        }
        showSettingsOverlay();
    }

    void setupSettingsOverlay()
    {
        settingsOverlay_->setObjectName("settingsOverlay");
        settingsOverlay_->hide();

        settingsPanel_->setObjectName("settingsPanel");
        settingsPanel_->setFrameShape(QFrame::NoFrame);

        auto form = new QFormLayout(settingsPanel_);
        form->setContentsMargins(16, 16, 16, 16);
        form->setSpacing(5); // Reduced spacing

        settingsApiSource_->addItem("新浪财经", "sina");
        settingsApiSource_->addItem("腾讯财经", "tencent");
        settingsApiSource_->addItem("东方财富", "eastmoney");
        settingsApiSource_->setObjectName("settingsCombo");
        form->addRow("数据来源:", settingsApiSource_);

        settingsRefreshSpin_->setRange(1000, 60000);
        settingsRefreshSpin_->setSingleStep(1000);
        settingsRefreshSpin_->setSuffix(" 毫秒");
        form->addRow("刷新间隔:", settingsRefreshSpin_);

        settingsAutoHideCombo_->addItem("禁用", 0);
        settingsAutoHideCombo_->addItem("2秒", 2000);
        settingsAutoHideCombo_->addItem("5秒", 5000);
        settingsAutoHideCombo_->addItem("10秒", 10000);
        settingsAutoHideCombo_->setObjectName("settingsCombo");
        form->addRow("自动隐藏:", settingsAutoHideCombo_);

        settingsThemeCombo_->addItem("浅色", "light");
        settingsThemeCombo_->addItem("深色", "dark");
        settingsThemeCombo_->setObjectName("settingsCombo");
        form->addRow("主题:", settingsThemeCombo_);

        settingsOpacitySpin_->setRange(0.3, 1.0);
        settingsOpacitySpin_->setSingleStep(0.05);
        settingsOpacitySpin_->setDecimals(2);
        form->addRow("窗口透明度:", settingsOpacitySpin_);

        settingsRowHeightSpin_->setRange(20, 50);
        settingsRowHeightSpin_->setSingleStep(1);
        settingsRowHeightSpin_->setSuffix(" 像素");
        form->addRow("行高:", settingsRowHeightSpin_);

        auto overlayLayout = new QVBoxLayout(settingsOverlay_);
        overlayLayout->setContentsMargins(0, 0, 0, 0);
        overlayLayout->addStretch(1);
        overlayLayout->addWidget(settingsPanel_, 0, Qt::AlignHCenter);
        overlayLayout->addStretch(1);

        settingsOverlay_->installEventFilter(this);
        settingsPanel_->installEventFilter(this);
        settingsApiSource_->installEventFilter(this);
        settingsRefreshSpin_->installEventFilter(this);
        settingsAutoHideCombo_->installEventFilter(this);
        settingsThemeCombo_->installEventFilter(this);
        settingsOpacitySpin_->installEventFilter(this);
        settingsRowHeightSpin_->installEventFilter(this);
    }

    void loadSettingsOverlayValues()
    {
        int apiIndex = settingsApiSource_->findData(config_->apiSource);
        settingsApiSource_->setCurrentIndex(apiIndex != -1 ? apiIndex : 0);

        settingsRefreshSpin_->setValue(config_->refreshInterval);
        int autoHideIndex = settingsAutoHideCombo_->findData(config_->autoHideDelay);
        settingsAutoHideCombo_->setCurrentIndex(autoHideIndex != -1 ? autoHideIndex : 0);

        int themeIndex = settingsThemeCombo_->findData(config_->theme);
        settingsThemeCombo_->setCurrentIndex(themeIndex != -1 ? themeIndex : 0);

        settingsOpacitySpin_->setValue(config_->windowOpacity);
        settingsRowHeightSpin_->setValue(config_->rowHeight);
    }

    void applySettingsOverlayValues()
    {
        config_->apiSource = settingsApiSource_->currentData().toString();
        config_->refreshInterval = settingsRefreshSpin_->value();
        config_->theme = settingsThemeCombo_->currentData().toString();
        config_->windowOpacity = settingsOpacitySpin_->value();
        config_->rowHeight = settingsRowHeightSpin_->value();
        config_->autoHideDelay = settingsAutoHideCombo_->currentData().toInt();

        config_->save();
        setWindowOpacity(config_->windowOpacity);
        table_->applyRowHeight(config_->rowHeight);
        applyTheme();
        applyApiSource();
    }

    void showSettingsOverlay()
    {
        suspendAutoHide_ = true;
        autoHideTimer_->stop();
        loadSettingsOverlayValues();
        syncSettingsOverlayGeometry();
        settingsOverlay_->raise();
        settingsOverlay_->show();
    }

    void closeSettingsOverlay()
    {
        if (!settingsOverlay_->isVisible()) return;
        applySettingsOverlayValues();
        settingsOverlay_->hide();
        suspendAutoHide_ = false;
        if (!cursorInside_) startAutoHideTimer();
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
        setMinimumSize(260, 200);

        input_->setObjectName("searchInput");
        titleBar_->setFixedHeight(40);
        titleBar_->setObjectName("titleBar");
        QHBoxLayout *titleLayout = new QHBoxLayout(titleBar_);
        titleLayout->setContentsMargins(5, 5, 5, 5);
        titleLayout->setSpacing(0);
        titleLayout->addWidget(input_, 1);
        titleLayout->addSpacing(30);
        titleLayout->addWidget(btns_[0]);

        auto wrap = new QVBoxLayout;
        wrap->setContentsMargins(5, 0, 5, 0);
        wrap->addWidget(table_, 1);

        auto statusBar = new QStatusBar(this);
        statusBar->setContentsMargins(0, 0, 0, 0);
        statusBar->addWidget(btns_[1]);
        for (auto &&pair : labels_.values())
        {
            statusBar->addWidget(pair.second);
        }

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins({});
        layout->setSpacing(0);
        layout->addWidget(titleBar_);
        layout->addLayout(wrap, 1);
        layout->addWidget(statusBar);

        searchResultWidget_->raise();
        searchResultWidget_->hide();
        setWindowOpacity(config_->windowOpacity);
    }

    void setupSystemTray()
    {
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

        qApp->style()->unpolish(qApp);
        qApp->setStyleSheet(css.join("\n"));
        qApp->style()->polish(qApp);
    }

    void applyApiSource()
    {
        QString requested = config_->apiSource;
        bool exactMatch = apiService_.setActiveProvider(requested);
        config_->apiSource = apiService_.activeProviderName();
        Logger::info(QString("Use %1 API").arg(config_->apiSource));
        if (!exactMatch)
        {
            Logger::error(QString("API source '%1' not found, fallback to %2").arg(requested, config_->apiSource));
        }
        startRefresh();
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
    StockApiService &apiService_;

    QWidget *titleBar_;
    QLineEdit *input_;
    StockSearchResultTable *searchResultWidget_;
    QList<QPushButton *> btns_;
    StockTableView *table_;
    QWidget *settingsOverlay_;
    QFrame *settingsPanel_;
    QComboBox *settingsApiSource_;
    QSpinBox *settingsRefreshSpin_;
    QComboBox *settingsAutoHideCombo_;
    QComboBox *settingsThemeCombo_;
    QDoubleSpinBox *settingsOpacitySpin_;
    QSpinBox *settingsRowHeightSpin_;
    QTimer *autoHideTimer_;
    QTimer *refreshTimer_;
    QTimer *searchDelayTimer_;
    bool cursorInside_{ false };
    bool suspendAutoHide_{ false };

    QMap<QString, QPair<QString, QLabel *>> labels_;
};
