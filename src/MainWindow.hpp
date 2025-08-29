#pragma once

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
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
#include <QSpinBox>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "Logger.hpp"
#include "StockApiService.hpp"
#include "StockConfig.hpp"
#include "StockConfigDialog.hpp"
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
        , titleBar_(new QWidget(this))
        , input_(new QLineEdit(this))
        , searchResultWidget_(new StockSearchResultTable(config_, this))
        , btns_({
              createTitleBarButton(":/%1/close-button.svg", "Close", &MainWindow::onCloseButtonClicked),
              createTitleBarButton(":/%1/settings.svg", "Settings", &MainWindow::openSettings),
          })
        , table_(new StockTableView(config_, this))
        , autoHideTimer_(new QTimer(this))
        , refreshTimer_(new QTimer(this))
        , labels_({
              { "SH000001", { "沪", new QLabel(this) } },
              { "SZ399001", { "深", new QLabel(this) } },
              { "SZ399006", { "创", new QLabel(this) } },
          })
    {
        setupUi();
        setupSystemTray();
        applyTheme();
        applyApiSource();

        setAttribute(Qt::WA_DeleteOnClose, false);

        connect(input_, &QLineEdit::textChanged, this, [this](auto &&text) {
            if (text.isEmpty())
            {
                searchResultWidget_->hide();
                return;
            }
            QTimer::singleShot(500, [this] {
                input_->setFocus();
                auto text = input_->text().trimmed();
                if (text.isEmpty())
                {
                    searchResultWidget_->hide();
                    return;
                }
                api_->searchStocksAsync(text);
            });
        });
        connect(searchResultWidget_, &StockSearchResultTable::stockSelected, this, [this](auto &&code) {
            searchResultWidget_->hide();
            input_->clear();
            input_->setFocus();
            table_->addStock(code);
            startRefresh();
        });

        refreshTimer_->setSingleShot(true);
        connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::startRefresh);
        autoHideTimer_->setSingleShot(true);
        connect(autoHideTimer_, &QTimer::timeout, this, &QWidget::hide);
        connect(qApp, &QApplication::focusChanged, this, [this](auto prev, auto curr) {
            if (config_->autoHideDelay <= 0) return;
            while (curr != nullptr)
            {
                if (curr == this)
                {
                    autoHideTimer_->stop();
                    return;
                }
                curr = curr->parentWidget();
            }
            autoHideTimer_->start(config_->autoHideDelay);
        });

        installEventFilter(this);
        titleBar_->installEventFilter(this);
        input_->installEventFilter(this);
        searchResultWidget_->installEventFilter(this);

        if (!config->lastGeometry.isEmpty()) restoreGeometry(config->lastGeometry);
    }

    ~MainWindow()
    {
        config_->lastGeometry = saveGeometry();
    }

public:
    void toggleActive()
    {
        if (isActiveWindow())
        {
            config_->lastGeometry = saveGeometry();
            hide();
        }
        else
        {
            if (!config_->lastGeometry.isEmpty()) restoreGeometry(config_->lastGeometry);
            input_->setFocus();
            activateWindow();
            show();
        }

        // if (isVisible())
        //     hide();
        // else
        // {
        //     show();
        //     activateWindow();
        //     setFocus();
        // }
    };

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto etype = event->type();
        if (etype == QEvent::KeyPress)
        {
            auto ke = static_cast<QKeyEvent *>(event);
            auto key = ke->key();
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
                        searchResultWidget_->hide();
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
            else if (etype == QEvent::Resize || etype == QEvent::Show)
            {
                searchResultWidget_->setGeometry(table_->geometry());
            }
            else if (etype == QEvent::Close)
            {
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

private:
    void startRefresh()
    {
        api_->getStockInfosAsync(config_->stockCodes + labels_.keys());
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
            // Alt+Click: Exit application directly
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
        StockConfigDialog dialog(config_, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            config_->save();
            applyTheme();
            applyApiSource();
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
        api_ = StockApiService::instance().get(config_->apiSource);
        Logger::info(QString("Use %1 API").arg(api_->name()));
        config_->apiSource = api_->name();

        connect(api_, &IStockApiProvider::errorOccurred, this, [] {
        });
        connect(api_, &IStockApiProvider::stockInfosReceived, this, &MainWindow::handleStockInfosReceived);
        connect(api_, &IStockApiProvider::searchCompleted, searchResultWidget_, &StockSearchResultTable::showResults);
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
    IStockApiProvider *api_{ nullptr };

    QWidget *titleBar_;
    QLineEdit *input_;
    StockSearchResultTable *searchResultWidget_;
    QList<QPushButton *> btns_;
    StockTableView *table_;
    QTimer *autoHideTimer_;
    QTimer *refreshTimer_;

    QMap<QString, QPair<QString, QLabel *>> labels_;
};
