#ifndef STOCKSEARCHWIDGET_HPP
#define STOCKSEARCHWIDGET_HPP

#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QApplication>

#include "StockSearchResult.hpp"
#include "StockApiService.hpp"
#include "StockSearchResultWindow.hpp"

class StockSearchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StockSearchWidget(QWidget *parent = nullptr)
        : QWidget(parent), m_searchLineEdit(nullptr), m_searchTimer(new QTimer(this)),
          m_mainLayout(nullptr), m_resultWindow(nullptr), m_isSearching(false)
    {
        setupUI();

        m_searchTimer->setSingleShot(true);
        m_searchTimer->setInterval(SEARCH_DELAY);
        connect(m_searchTimer, &QTimer::timeout, this, &StockSearchWidget::onSearchTimer);

        connect(&StockApiService::instance(), &StockApiService::searchCompleted,
                this, &StockSearchWidget::onSearchCompleted);

        // Create search result window
        m_resultWindow = new StockSearchResultWindow(this);
        connect(m_resultWindow, &StockSearchResultWindow::stockSelected,
                this, &StockSearchWidget::onResultWindowStockSelected);
        connect(m_resultWindow, &StockSearchResultWindow::escapePressed,
                this, &StockSearchWidget::escapePressed);

        // Install application-wide event filter for focus monitoring
        qApp->installEventFilter(this);
    }

signals:
    void stockSelected(const StockSearchResult &result);
    void escapePressed();

private slots:
    void onTextChanged()
    {
        QString text = m_searchLineEdit->text().trimmed();

        m_searchTimer->stop();

        if (text.isEmpty() || text.length() < 1)
        {
            hideResults();
            return;
        }

        m_searchTimer->start();
    }

    void onSearchTimer()
    {
        QString text = m_searchLineEdit->text().trimmed();
        if (!text.isEmpty() && text.length() >= 1 && !m_isSearching)
        {
            Logger::info(QString("Auto search: '%1'").arg(text));
            m_isSearching = true;
            StockApiService::instance().searchStocksAsync(text);
        }
    }

    void onSearchCompleted(const QList<StockSearchResult> &results)
    {
        m_isSearching = false;

        if (!results.isEmpty())
        {
            Logger::info(QString("Search completed with %1 results").arg(results.size()));
            showResults(results);
        }
        else
        {
            Logger::info("No matching stocks found");
            hideResults();
        }
    }

    void onResultWindowStockSelected(const StockSearchResult &result)
    {
        Logger::info(QString("Stock selected from result window: %1 - %2").arg(result.code(), result.name()));

        // Clear search text when adding stock
        m_searchLineEdit->clear();
        hideResults();
        emit stockSelected(result);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);

        // Set focus to search input when widget becomes visible
        if (m_searchLineEdit)
        {
            m_searchLineEdit->setFocus();
        }
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // Handle ESC key in search line edit
        if (obj == m_searchLineEdit && event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape)
            {
                // Always hide search results first
                hideResults();

                // Check if search box has content
                if (!m_searchLineEdit->text().isEmpty())
                {
                    // If content is not empty, clear it
                    m_searchLineEdit->clear();
                }
                else
                {
                    // If content is empty, hide main window
                    emit escapePressed();
                }
                return true; // Event handled
            }
        }

        // Handle focus changes for auto-hiding search results
        if (event->type() == QEvent::FocusIn && m_resultWindow->isVisible())
        {
            QWidget *focusWidget = qobject_cast<QWidget *>(obj);
            if (focusWidget)
            {
                // Check if the new focus widget is related to search functionality
                bool isSearchRelated = false;

                QWidget *widget = focusWidget;
                while (widget)
                {
                    if (widget == this || widget == m_resultWindow)
                    {
                        isSearchRelated = true;
                        break;
                    }
                    widget = widget->parentWidget();
                }

                // If focus moved to unrelated widget, hide results
                if (!isSearchRelated)
                {
                    QTimer::singleShot(50, this, [this]()
                                       { hideResults(); });
                }
            }
        }

        return QWidget::eventFilter(obj, event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape)
        {
            // Always hide search results first
            hideResults();

            // Check if search box has content
            if (!m_searchLineEdit->text().isEmpty())
            {
                // If content is not empty, clear it
                m_searchLineEdit->clear();
            }
            else
            {
                // If content is empty, hide main window
                emit escapePressed();
            }
            return;
        }

        // Forward keyboard events to result window (if it's visible)
        if (m_resultWindow->isVisible())
        {
            QApplication::sendEvent(m_resultWindow, event);
            return;
        }

        QWidget::keyPressEvent(event);
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        // Delay hiding to allow clicking on result window
        QTimer::singleShot(100, this, [this]()
                           {
            // Check if focus is on search box or result window
            QWidget *focusWidget = QApplication::focusWidget();
            bool shouldHide = true;

            if (focusWidget)
            {
                // Check if focus is on search line edit
                if (focusWidget == m_searchLineEdit)
                {
                    shouldHide = false;
                }
                // Check if focus is on result window or its children
                else if (m_resultWindow->isVisible())
                {
                    QWidget *widget = focusWidget;
                    while (widget)
                    {
                        if (widget == m_resultWindow)
                        {
                            shouldHide = false;
                            break;
                        }
                        widget = widget->parentWidget();
                    }
                }
            }

            if (shouldHide)
            {
                hideResults();
            } });

        QWidget::focusOutEvent(event);
    }

private:
    void setupUI()
    {
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);

        QWidget *searchContainer = new QWidget(this);
        QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
        searchLayout->setContentsMargins(0, 0, 0, 0);
        searchLayout->setSpacing(5);

        m_searchLineEdit = new QLineEdit(this);
        m_searchLineEdit->setObjectName("searchInput");
        m_searchLineEdit->setPlaceholderText("Enter stock code or name to search...");
        m_searchLineEdit->installEventFilter(this);
        searchLayout->addWidget(m_searchLineEdit);
        connect(m_searchLineEdit, &QLineEdit::textChanged, this, &StockSearchWidget::onTextChanged);

        m_mainLayout->addWidget(searchContainer);
    }

    void showResults(const QList<StockSearchResult> &results)
    {
        // Calculate search box position on screen
        QPoint globalPos = m_searchLineEdit->mapToGlobal(QPoint(0, m_searchLineEdit->height()));

        // Get search box width to match result window width
        int searchBoxWidth = m_searchLineEdit->width();

        // Show search result window with matching width
        m_resultWindow->showResults(results, globalPos, searchBoxWidth);

        // Ensure search box keeps focus after showing results
        m_searchLineEdit->setFocus();

        Logger::info(QString("Showing %1 search suggestions in popup window").arg(results.size()));
    }

    void hideResults()
    {
        m_resultWindow->hideResults();
    }

    QLineEdit *m_searchLineEdit;
    QTimer *m_searchTimer;
    QVBoxLayout *m_mainLayout;
    StockSearchResultWindow *m_resultWindow;

    bool m_isSearching;

    static const int SEARCH_DELAY = 300;
};

#endif // STOCKSEARCHWIDGET_HPP
