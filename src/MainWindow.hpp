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
#include <QMouseEvent>
#include <QPalette>
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
        , settingsNetworkProxy_(new QComboBox(settingsPanel_))
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
        connect(&apiService_, &StockApiService::searchCompleted, this, [this](quint64 requestId, const QString &keyword, const QList<StockInfo> &results) {
            if (requestId == latestSearchRequestId_ && input_->text().trimmed() == keyword && !keyword.isEmpty())
            {
                syncSearchResultsGeometry();
                const bool hasVisibleResults = searchResultWidget_->showResults(results);
                setSearchNoResults(!hasVisibleResults);
                focusSearchInput();
            }
        });
        applyNetworkProxy();
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
            latestSearchRequestId_ = apiService_.searchStocksAsync(text);
        });
        connect(input_, &QLineEdit::textChanged, this, [this](const QString &text) {
            setSearchNoResults(false);
            if (text.trimmed().isEmpty())
            {
                latestSearchRequestId_ = 0;
                hideSearchResults();
                return;
            }
            searchDelayTimer_->start(500);
        });
        connect(searchResultWidget_, &StockSearchResultTable::stockSelected, this, [this](auto &&code) {
            hideSearchResults();
            setSearchNoResults(false);
            input_->clear();
            focusSearchInput();
            table_->addStock(code);
            startRefresh();
        });

        refreshTimer_->setSingleShot(true);
        connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::startRefresh);
        autoHideTimer_->setSingleShot(true);
        connect(autoHideTimer_, &QTimer::timeout, this, &MainWindow::onAutoHideTimeout);
        connect(table_, &StockTableView::contextMenuOpened, this, &MainWindow::pauseAutoHide);
        connect(table_, &StockTableView::contextMenuClosed, this, &MainWindow::resumeAutoHide);
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
        table_->viewport()->installEventFilter(this);
        searchResultWidget_->installEventFilter(this);
        searchResultWidget_->viewport()->installEventFilter(this);
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
        if (handleWindowDrag(watched, event)) return true;
        if (etype == QEvent::KeyPress)
        {
            autoHideTimer_->stop();
            auto ke = static_cast<QKeyEvent *>(event);
            auto key = ke->key();
            if (key == Qt::Key_Comma && (ke->modifiers() & Qt::ControlModifier) && !(ke->modifiers() & (Qt::AltModifier | Qt::MetaModifier)))
            {
                openSettings();
                return true;
            }
            if (key == Qt::Key_L && (ke->modifiers() & Qt::ControlModifier) && !(ke->modifiers() & (Qt::AltModifier | Qt::MetaModifier)))
            {
                switchTheme();
                return true;
            }
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
            else if (etype == QEvent::Close)
            {
                config_->lastGeometry = saveGeometry();
                autoHideTimer_->stop();
                autoHideSuspendDepth_ = 0;
                hide();
                return true;
            }
            else if (etype == QEvent::Hide)
            {
                config_->lastGeometry = saveGeometry();
                autoHideTimer_->stop();
                autoHideSuspendDepth_ = 0;
            }
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
        return isVisible() && config_->autoHideDelay > 0 && autoHideSuspendDepth_ == 0;
    }

    void startAutoHideTimer()
    {
        if (isAutoHideEnabled()) autoHideTimer_->start(config_->autoHideDelay);
    }

    void pauseAutoHide()
    {
        ++autoHideSuspendDepth_;
        autoHideTimer_->stop();
    }

    void resumeAutoHide()
    {
        if (autoHideSuspendDepth_ > 0)
        {
            --autoHideSuspendDepth_;
        }

        if (autoHideSuspendDepth_ == 0 && !cursorInside_)
        {
            startAutoHideTimer();
        }
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
        case QEvent::InputMethod:
        case QEvent::Wheel:
            return true;
        default:
            return false;
        }
    }

    bool handleWindowDrag(QObject *watched, QEvent *event)
    {
        const QEvent::Type type = event->type();
        if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove && type != QEvent::MouseButtonRelease)
        {
            return false;
        }

        QWidget *source = qobject_cast<QWidget *>(watched);
        QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (!source || !mouseEvent) return false;

        if (type == QEvent::MouseButtonPress)
        {
            if (mouseEvent->button() != Qt::LeftButton || !canStartWindowDrag(source, mouseEvent->pos())) return false;
            windowDragActive_ = true;
            windowDragOffset_ = mouseEvent->globalPos() - frameGeometry().topLeft();
            windowDragSource_ = source;
            source->grabMouse();
            return true;
        }

        if (type == QEvent::MouseMove && windowDragActive_ && (mouseEvent->buttons() & Qt::LeftButton))
        {
            move(mouseEvent->globalPos() - windowDragOffset_);
            return true;
        }

        if (type == QEvent::MouseButtonRelease && windowDragActive_ && mouseEvent->button() == Qt::LeftButton)
        {
            if (windowDragSource_) windowDragSource_->releaseMouse();
            windowDragActive_ = false;
            windowDragSource_ = nullptr;
            return true;
        }

        return false;
    }

    bool canStartWindowDrag(QWidget *source, const QPoint &pos) const
    {
        if (settingsOverlay_->isVisible() && (source == settingsOverlay_ || settingsOverlay_->isAncestorOf(source))) return false;
        if (source == table_->viewport()) return !table_->indexAt(pos).isValid();
        if (source == searchResultWidget_->viewport()) return !searchResultWidget_->indexAt(pos).isValid();

        QWidget *target = source->childAt(pos);
        if (!target) target = source;

        for (QWidget *widget = target; widget; widget = widget->parentWidget())
        {
            if (isWindowDragBlockedBy(widget)) return false;
            if (widget == this) break;
        }

        return true;
    }

    bool isWindowDragBlockedBy(QWidget *widget) const
    {
        return widget == input_ || widget == table_ || widget == searchResultWidget_ || widget == settingsOverlay_ || widget == settingsPanel_
            || qobject_cast<QPushButton *>(widget) || qobject_cast<QLineEdit *>(widget) || qobject_cast<QTableView *>(widget)
            || qobject_cast<QComboBox *>(widget) || qobject_cast<QSpinBox *>(widget) || qobject_cast<QDoubleSpinBox *>(widget);
    }

    void hideSearchResults()
    {
        searchResultWidget_->hide();
    }

    void setSearchNoResults(bool noResults)
    {
        if (input_->property("searchNoResults").toBool() == noResults) return;
        input_->setProperty("searchNoResults", noResults);
        input_->style()->unpolish(input_);
        input_->style()->polish(input_);
        input_->update();
    }

    void focusSearchInput()
    {
        input_->setFocus();
    }

    void syncSearchResultsGeometry()
    {
        searchResultWidget_->setGeometry(table_->geometry());
        searchResultWidget_->raise();
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
        QStringList codes;
        codes.reserve(config_->stockCodes.size() + labels_.size());
        codes.append(config_->stockCodes);
        for (auto it = labels_.constBegin(); it != labels_.constEnd(); ++it)
        {
            codes.append(it.key());
        }
        apiService_.getStockInfosAsync(codes);
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
                label->setToolTip(statusLabelTooltip(info));
            }
        }

        table_->updateStocksDetail(stockInfos);
    }

    QString statusLabelTooltip(const StockInfo &info) const
    {
        return QString("%1(%2) %3(%4)\n成交: %5(%6)")
            .arg(info.name())
            .arg(info.code())
            .arg(QString::number(info.currentPrice(), 'f', 2))
            .arg(info.changePercentText())
            .arg(info.turnoverText())
            .arg(info.volumeText());
    }

    void onApiError(const QString &error)
    {
        // Polling errors are routine (offline, API hiccup); log only, never modal-spam.
        Logger::error(QString("API Error: %1").arg(error));
    }

    void onCloseButtonClicked()
    {
        if (QApplication::keyboardModifiers() & Qt::AltModifier)
        {
            QApplication::quit();
            return;
        }
        // No tray icon: hiding would strand the window behind the hotkey only.
        if (!QSystemTrayIcon::isSystemTrayAvailable())
        {
            QApplication::quit();
            return;
        }
        hide();
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

        settingsNetworkProxy_->addItem("关闭", "off");
        settingsNetworkProxy_->addItem("系统", "system");
        settingsNetworkProxy_->setObjectName("settingsCombo");
        form->addRow("网络代理:", settingsNetworkProxy_);

        settingsRefreshSpin_->setRange(1000, 60000);
        settingsRefreshSpin_->setSingleStep(1000);
        settingsRefreshSpin_->setSuffix(" 毫秒");
        form->addRow("刷新间隔:", settingsRefreshSpin_);

        settingsAutoHideCombo_->addItem("禁用", 0);
        settingsAutoHideCombo_->addItem("3秒", 3000);
        settingsAutoHideCombo_->addItem("6秒", 6000);
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
        settingsNetworkProxy_->installEventFilter(this);
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

        int proxyIndex = settingsNetworkProxy_->findData(config_->networkProxy);
        settingsNetworkProxy_->setCurrentIndex(proxyIndex != -1 ? proxyIndex : 0);

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
        config_->networkProxy = settingsNetworkProxy_->currentData().toString();
        if (config_->networkProxy != "system")
        {
            config_->networkProxy = "off";
        }
        config_->refreshInterval = settingsRefreshSpin_->value();
        config_->theme = settingsThemeCombo_->currentData().toString();
        config_->windowOpacity = settingsOpacitySpin_->value();
        config_->rowHeight = settingsRowHeightSpin_->value();
        config_->autoHideDelay = settingsAutoHideCombo_->currentData().toInt();

        config_->save();
        setWindowOpacity(config_->windowOpacity);
        table_->applyRowHeight(config_->rowHeight);
        applyTheme();
        applyNetworkProxy();
        applyApiSource();
    }

    void showSettingsOverlay()
    {
        pauseAutoHide();
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
        resumeAutoHide();
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
        statusBar->installEventFilter(this);
        statusBar->addWidget(btns_[1]);
        for (auto &&pair : labels_.values())
        {
            pair.second->setAlignment(Qt::AlignCenter);
            pair.second->installEventFilter(this);
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


    void switchTheme()
    {
        int index = config_->availableThemes.indexOf(config_->theme);
        if (index < 0) index = 0;
        const int next = (index + 1) % config_->availableThemes.size();
        const QString theme = config_->availableThemes.value(next);
        applyTheme(theme);
        config_->save();

        const int comboIndex = settingsThemeCombo_->findData(theme);
        if (comboIndex != -1) settingsThemeCombo_->setCurrentIndex(comboIndex);
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
        updateSettingsComboPopups();
        refreshStyledWidgets();
    }

    void refreshStyledWidgets()
    {
        style()->unpolish(this);
        style()->polish(this);
        update();
        table_->viewport()->update();
        searchResultWidget_->viewport()->update();
    }

    void updateSettingsComboPopups()
    {
        const bool dark = config_->isDarkTheme();
        const QColor text = dark ? QColor("#e7ecf2") : QColor("#1f2933");
        const QColor background = dark ? QColor("#18212b") : QColor("#ffffff");
        const QColor selectedText = dark ? QColor("#ffffff") : QColor("#111827");
        const QColor selectedBackground = dark ? QColor("#223f46") : QColor("#d7ece8");
        const QColor border = dark ? QColor("#344252") : QColor("#c9d3df");
        const QColor hover = dark ? QColor("#253140") : QColor("#ecf3f8");

        const QString viewStyle = QString(
            "QAbstractItemView { background-color: %1; color: %2; border: 1px solid %3; outline: 0; }"
            "QAbstractItemView::item { min-height: 22px; padding: 4px 8px; }"
            "QAbstractItemView::item:hover { background-color: %4; color: %2; }"
            "QAbstractItemView::item:selected { background-color: %5; color: %6; }")
                                      .arg(background.name(), text.name(), border.name(), hover.name(), selectedBackground.name(), selectedText.name());

        for (QComboBox *combo : { settingsApiSource_, settingsNetworkProxy_, settingsAutoHideCombo_, settingsThemeCombo_ })
        {
            if (!combo || !combo->view()) continue;
            QPalette palette = combo->view()->palette();
            palette.setColor(QPalette::Base, background);
            palette.setColor(QPalette::Text, text);
            palette.setColor(QPalette::Window, background);
            palette.setColor(QPalette::HighlightedText, selectedText);
            palette.setColor(QPalette::Highlight, selectedBackground);
            combo->view()->setPalette(palette);
            combo->view()->setStyleSheet(viewStyle);
            combo->view()->update();
        }
    }

    void applyNetworkProxy()
    {
        apiService_.setNetworkProxy(config_->networkProxy);
        Logger::info(QString("Network proxy: %1").arg(config_->networkProxy));
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
    QComboBox *settingsNetworkProxy_;
    QSpinBox *settingsRefreshSpin_;
    QComboBox *settingsAutoHideCombo_;
    QComboBox *settingsThemeCombo_;
    QDoubleSpinBox *settingsOpacitySpin_;
    QSpinBox *settingsRowHeightSpin_;
    QTimer *autoHideTimer_;
    QTimer *refreshTimer_;
    QTimer *searchDelayTimer_;
    quint64 latestSearchRequestId_{ 0 };
    bool cursorInside_{ false };
    int autoHideSuspendDepth_{ 0 };
    bool windowDragActive_{ false };
    QPoint windowDragOffset_;
    QWidget *windowDragSource_{ nullptr };

    QMap<QString, QPair<QString, QLabel *>> labels_;
};
