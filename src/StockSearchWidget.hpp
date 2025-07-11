#pragma once

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class StockSearchWidget : public QLineEdit
{
    Q_OBJECT

public:
    explicit StockSearchWidget(QWidget *parent = nullptr)
        : QLineEdit(parent)
        , m_searchTimer(new QTimer(this))
        , m_mainLayout(nullptr)
        // , m_resultWindow(nullptr)
        , m_isSearching(false)
    {
        setObjectName("searchInput");
        setPlaceholderText("Enter stock code or name to search...");
        installEventFilter(this);
        connect(this, &QLineEdit::textChanged, this, &StockSearchWidget::onTextChanged);

        m_searchTimer->setSingleShot(true);
        m_searchTimer->setInterval(SEARCH_DELAY);
        connect(m_searchTimer, &QTimer::timeout, this, &StockSearchWidget::onSearchTimer);

        // connect(&StockApiService::instance(), &StockApiService::searchCompleted, this, &StockSearchWidget::onSearchCompleted);

        // Create search result window
        // m_resultWindow = new StockSearchResultWindow(this);
        // connect(m_resultWindow, &StockSearchResultWindow::stockSelected, this, &StockSearchWidget::onResultWindowStockSelected);
        // connect(m_resultWindow, &StockSearchResultWindow::escapePressed, this, &StockSearchWidget::escapePressed);

        // // Install application-wide event filter for focus monitoring
        // qApp->installEventFilter(this);
    }

    void setExistingStocks(const QStringList &stockCodes)
    {
        m_existingStockCodes = stockCodes;
    }

signals:
    void stockSelected(const StockSearchResult &result);
    void escapePressed();

private slots:
    void onTextChanged()
    {
        QString text = this->text();

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
        QString text = this->text().trimmed();
        if (!text.isEmpty() && text.length() >= 1 && !m_isSearching)
        {
            m_isSearching = true;
            // StockApiService::instance().searchStocksAsync(text);
        }
    }

    void onSearchCompleted(const QList<StockSearchResult> &results)
    {
        m_isSearching = false;

        // Filter out stocks that already exist in the table
        QList<StockSearchResult> filteredResults;
        for (const StockSearchResult &result : results)
        {
            QString resultCode = result.code().toUpper();
            bool alreadyExists = false;

            for (const QString &existingCode : m_existingStockCodes)
            {
                if (resultCode == existingCode.toUpper())
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists)
            {
                filteredResults.append(result);
            }
        }

        if (!filteredResults.isEmpty())
        {
            showResults(filteredResults);
        }
        else
        {
            hideResults();
        }
    }

    void onResultWindowStockSelected(const StockSearchResult &result)
    {
        clear();
        hideResults();
        emit stockSelected(result);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        // m_searchLineEdit->setFocus();
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // // Handle ESC key in search line edit
        // if (obj == m_searchLineEdit && event->type() == QEvent::KeyPress)
        // {
        //     QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        //     if (keyEvent->key() == Qt::Key_Escape)
        //     {
        //         // Always hide search results first
        //         hideResults();

        //         // Check if search box has content
        //         if (!m_searchLineEdit->text().isEmpty())
        //         {
        //             // If content is not empty, clear it
        //             m_searchLineEdit->clear();
        //         }
        //         else
        //         {
        //             // If content is empty, hide main window
        //             emit escapePressed();
        //         }
        //         return true;  // Event handled
        //     }
        // }

        // // Handle focus changes for auto-hiding search results
        // if (event->type() == QEvent::FocusIn && m_resultWindow->isVisible())
        // {
        //     QWidget *focusWidget = qobject_cast<QWidget *>(obj);
        //     if (focusWidget)
        //     {
        //         // Check if the new focus widget is related to search functionality
        //         bool isSearchRelated = false;

        //         QWidget *widget = focusWidget;
        //         while (widget)
        //         {
        //             if (widget == this || widget == m_resultWindow)
        //             {
        //                 isSearchRelated = true;
        //                 break;
        //             }
        //             widget = widget->parentWidget();
        //         }

        //         // If focus moved to unrelated widget, hide results
        //         if (!isSearchRelated)
        //         {
        //             QTimer::singleShot(50, this, [this]() {
        //                 hideResults();
        //             });
        //         }
        //     }
        // }

        return QWidget::eventFilter(obj, event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        // if (event->key() == Qt::Key_Escape)
        // {
        //     // Always hide search results first
        //     hideResults();

        //     // Check if search box has content
        //     if (!m_searchLineEdit->text().isEmpty())
        //     {
        //         // If content is not empty, clear it
        //         m_searchLineEdit->clear();
        //     }
        //     else
        //     {
        //         // If content is empty, hide main window
        //         emit escapePressed();
        //     }
        //     return;
        // }

        // // Forward keyboard events to result window (if it's visible)
        // if (m_resultWindow->isVisible())
        // {
        //     QApplication::sendEvent(m_resultWindow, event);
        //     return;
        // }

        QWidget::keyPressEvent(event);
    }

    void focusOutEvent(QFocusEvent *event) override
    {
        // // Delay hiding to allow clicking on result window
        // QTimer::singleShot(100, this, [this]() {
        //     // Check if focus is on search box or result window
        //     QWidget *focusWidget = QApplication::focusWidget();
        //     bool shouldHide = true;

        //     if (focusWidget)
        //     {
        //         // Check if focus is on search line edit
        //         if (focusWidget == m_searchLineEdit)
        //         {
        //             shouldHide = false;
        //         }
        //         // Check if focus is on result window or its children
        //         else if (m_resultWindow->isVisible())
        //         {
        //             QWidget *widget = focusWidget;
        //             while (widget)
        //             {
        //                 if (widget == m_resultWindow)
        //                 {
        //                     shouldHide = false;
        //                     break;
        //                 }
        //                 widget = widget->parentWidget();
        //             }
        //         }
        //     }

        //     if (shouldHide)
        //     {
        //         hideResults();
        //     }
        // });

        QWidget::focusOutEvent(event);
    }

private:
    void setupUI()
    {
    }

    void showResults(const QList<StockSearchResult> &results)
    {
        // // Calculate search box position on screen
        // QPoint globalPos = m_searchLineEdit->mapToGlobal(QPoint(0, m_searchLineEdit->height()));

        // // Get search box width to match result window width
        // int searchBoxWidth = m_searchLineEdit->width();

        // // Show search result window with matching width
        // m_resultWindow->showResults(results, globalPos, searchBoxWidth);

        // // Ensure search box keeps focus after showing results
        // m_searchLineEdit->setFocus();
    }

    void hideResults()
    {
        // m_resultWindow->hideResults();
    }

    QTimer *m_searchTimer;
    QVBoxLayout *m_mainLayout;

    bool m_isSearching;
    QStringList m_existingStockCodes;

    static const int SEARCH_DELAY = 300;
};
