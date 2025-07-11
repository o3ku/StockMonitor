#pragma once

#include <QApplication>
#include <QDesktopWidget>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#include "Logger.hpp"
#include "StockSearchResult.hpp"

class StockSearchResultWindow : public QWidget
{
    Q_OBJECT

public:
    explicit StockSearchResultWindow(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_resultsListWidget(nullptr)
        , m_mainLayout(nullptr)
        , m_selectedIndex(-1)
    {
        setupUI();

        // Set window properties: stay on top, frameless, tool window
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFocusPolicy(Qt::StrongFocus);
    }

    void showResults(const QList<StockSearchResult> &results, const QPoint &position, int width = 298)
    {
        // Sort results according to market priority
        m_searchResults = sortResultsByMarket(results);
        m_selectedIndex = -1;

        m_resultsListWidget->clear();

        int maxItems = qMin(results.size(), 6);

        for (int i = 0; i < maxItems; ++i)
        {
            const StockSearchResult &result = m_searchResults[i];
            QWidget *itemWidget = createResultItem(result, i);

            QListWidgetItem *item = new QListWidgetItem();
            item->setSizeHint(QSize(width, 35));
            m_resultsListWidget->addItem(item);
            m_resultsListWidget->setItemWidget(item, itemWidget);
        }

        // Set window size and position - use provided width
        int windowHeight = maxItems * 35;
        setFixedSize(width, windowHeight);

        // Ensure window stays within screen bounds
        QScreen *screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();

        int x = position.x();
        int y = position.y();

        // Check right boundary
        if (x + width > screenGeometry.right())
        {
            x = screenGeometry.right() - width;
        }

        // Check bottom boundary
        if (y + height() > screenGeometry.bottom())
        {
            y = position.y() - height();
        }

        // Ensure not exceeding top-left boundaries
        x = qMax(x, screenGeometry.left());
        y = qMax(y, screenGeometry.top());

        move(x, y);
        show();
        raise();
        // Don't activate window or set focus to keep search box focused
        // activateWindow();
        // setFocus();

        Logger::info(QString("Showing %1 search suggestions in popup window").arg(maxItems));
    }

    void hideResults()
    {
        hide();
    }

signals:
    void stockSelected(const StockSearchResult &result);
    void escapePressed();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Up)
        {
            if (m_selectedIndex > 0)
            {
                updateSelectedItem(m_selectedIndex - 1);
            }
            event->accept();
        }
        else if (event->key() == Qt::Key_Down)
        {
            if (m_selectedIndex < m_searchResults.size() - 1)
            {
                updateSelectedItem(m_selectedIndex + 1);
            }
            event->accept();
        }
        else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        {
            if (m_selectedIndex >= 0 && m_selectedIndex < m_searchResults.size())
            {
                emit stockSelected(m_searchResults[m_selectedIndex]);
            }
            event->accept();
        }
        else if (event->key() == Qt::Key_Escape)
        {
            hideResults();
            // Emit signal to notify main window to hide
            emit escapePressed();
            event->accept();
        }
        else
        {
            QWidget::keyPressEvent(event);
        }
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        // Delay hiding to allow clicking on results
        QTimer::singleShot(100, this, [this]() {
            // Check if focus moved to search widget or stayed within result window
            QWidget *focusWidget = QApplication::focusWidget();
            bool shouldHide = true;

            if (focusWidget)
            {
                // Check if focus moved to parent search widget
                QWidget *widget = focusWidget;
                while (widget)
                {
                    if (widget == parentWidget())  // Search widget
                    {
                        shouldHide = false;
                        break;
                    }
                    if (widget == this)  // Still within result window
                    {
                        shouldHide = false;
                        break;
                    }
                    widget = widget->parentWidget();
                }
            }

            if (shouldHide)
            {
                hide();
            }
        });

        QWidget::focusOutEvent(event);
    }

private slots:
    void onItemClicked(int row)
    {
        if (row >= 0 && row < m_searchResults.size())
        {
            emit stockSelected(m_searchResults[row]);
        }
    }

private:
    void setupUI()
    {
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);

        m_resultsListWidget = new QListWidget(this);
        m_resultsListWidget->setObjectName("searchResultsList");

        // Disable scroll bars
        m_resultsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_resultsListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        connect(m_resultsListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
            int row = m_resultsListWidget->row(item);
            onItemClicked(row);
        });

        m_mainLayout->addWidget(m_resultsListWidget);
    }

    QWidget *createResultItem(const StockSearchResult &result, int index)
    {
        QWidget *itemWidget = new QWidget();
        itemWidget->setFixedHeight(35);

        QHBoxLayout *layout = new QHBoxLayout(itemWidget);
        layout->setContentsMargins(5, 2, 5, 2);
        layout->setSpacing(5);

        // Stock code label
        QLabel *codeLabel = new QLabel(result.code());
        codeLabel->setObjectName("stockCodeLabel");
        codeLabel->setFixedWidth(70);

        // Stock name label
        QLabel *nameLabel = new QLabel(result.name());
        nameLabel->setObjectName("stockNameLabel");

        layout->addWidget(codeLabel);
        layout->addWidget(nameLabel);

        return itemWidget;
    }

    void updateSelectedItem(int index)
    {
        if (index >= 0 && index < m_resultsListWidget->count())
        {
            m_selectedIndex = index;
            m_resultsListWidget->setCurrentRow(index);
        }
    }

    QList<StockSearchResult> sortResultsByMarket(const QList<StockSearchResult> &results) const
    {
        QList<StockSearchResult> sortedResults = results;

        // Sort by market priority
        std::sort(sortedResults.begin(), sortedResults.end(), [this](const StockSearchResult &a, const StockSearchResult &b) {
            return getMarketPriority(a.code()) < getMarketPriority(b.code());
        });

        return sortedResults;
    }

    int getMarketPriority(const QString &code) const
    {
        QString upperCode = code.toUpper();

        // 1. 深圳股票 (Shenzhen stocks) - 000xxx, 002xxx, 300xxx
        if (upperCode.startsWith("SZ") || (upperCode.startsWith("0") && !upperCode.startsWith("0F")) || upperCode.startsWith("3"))
        {
            return 1;
        }
        // 2. 上海股票 (Shanghai stocks) - 600xxx, 601xxx, 603xxx, 688xxx
        else if (upperCode.startsWith("SH") || upperCode.startsWith("6"))
        {
            return 2;
        }
        // 3. 深圳基金 (Shenzhen funds) - 0Fxxxx
        else if (upperCode.startsWith("0F"))
        {
            return 3;
        }
        // 4. 上海基金 (Shanghai funds) - 5xxxxx, 1Fxxxx
        else if (upperCode.startsWith("5") || upperCode.startsWith("1F"))
        {
            return 4;
        }
        // 5. 其它市场 (Other markets)
        else
        {
            return 5;
        }
    }

    QListWidget *m_resultsListWidget;
    QVBoxLayout *m_mainLayout;

    QList<StockSearchResult> m_searchResults;
    int m_selectedIndex;
};
