#pragma once

#include "Logger.hpp"
#include "StockInfo.hpp"
#include "StockModel.hpp"
#include <QAbstractItemView>
#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTableView>

// Custom delegate to preserve text colors in selected state
class ColorPreservingDelegate : public QStyledItemDelegate
{
public:
    explicit ColorPreservingDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;

        // Get the color from the model's ForegroundRole
        QVariant colorVariant = index.data(Qt::ForegroundRole);
        if (colorVariant.isValid())
        {
            QColor textColor = colorVariant.value<QColor>();
            // Force the text color even when selected
            opt.palette.setColor(QPalette::Text, textColor);
            opt.palette.setColor(QPalette::HighlightedText, textColor);
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }
};

class StockTableView : public QTableView
{
    Q_OBJECT

public:
    explicit StockTableView(QWidget *parent = nullptr)
        : QTableView(parent)
        , m_stockModel(new StockModel(this))
        , m_dropIndicatorRow(-1)
    {
        setupView();
        setupContextMenu();
        setModel(m_stockModel);
        setColumnWidth(StockModel::NameColumn, 90);
        setColumnWidth(StockModel::CurrentPriceColumn, 80);
        setColumnWidth(StockModel::ChangePercentColumn, 70);

        // Connect model signals
        connect(m_stockModel, &StockModel::stockListChanged, this, &StockTableView::stockListChanged);
    }

    void addStock(const StockInfo &stock)
    {
        // Check if stock already exists
        QList<StockInfo> existingStocks = m_stockModel->stockData();
        for (const StockInfo &existingStock : existingStocks)
        {
            if (existingStock.code() == stock.code())
            {
                QMessageBox::information(this, "Information", QString("Stock %1 already exists!").arg(stock.code()));
                return;
            }
        }

        m_stockModel->addStock(stock);
    }

    void updateStockData(const QList<StockInfo> &newData)
    {
        m_stockModel->updateStockData(newData);
    }

    QList<StockInfo> stockData() const
    {
        return m_stockModel->stockData();
    }

    void setStockData(const QList<StockInfo> &stockData)
    {
        m_stockModel->setStockData(stockData);
    }

signals:
    void stockListChanged();

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTableView::paintEvent(event);

        // Draw custom drop indicator for full row
        if (m_dropIndicatorRow >= 0)
        {
            QPainter painter(viewport());
            painter.setPen(QPen(QColor(0, 120, 215), 2));  // Blue line

            int y;
            if (m_dropIndicatorRow >= model()->rowCount())
            {
                // Drop at the end
                QRect lastRowRect = visualRect(model()->index(model()->rowCount() - 1, 0));
                y = lastRowRect.bottom() + 1;
            }
            else
            {
                // Drop before the specified row
                QRect rowRect = visualRect(model()->index(m_dropIndicatorRow, 0));
                y = rowRect.top() - 1;
            }

            painter.drawLine(0, y, viewport()->width(), y);
        }
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() == this && event->mimeData()->hasFormat(StockConstants::STOCK_ROW_MIME_TYPE))
        {
            event->acceptProposedAction();
            m_dropIndicatorRow = -1;
            viewport()->update();
        }
        else
        {
            QTableView::dragEnterEvent(event);
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->source() == this && event->mimeData()->hasFormat(StockConstants::STOCK_ROW_MIME_TYPE))
        {
            QModelIndex index = indexAt(event->pos());
            int newDropRow = -1;

            if (index.isValid())
            {
                QRect itemRect = visualRect(index);
                if (event->pos().y() < itemRect.center().y())
                {
                    newDropRow = index.row();
                }
                else
                {
                    newDropRow = index.row() + 1;
                }
            }
            else
            {
                // Drop at the end if not over any item
                newDropRow = model()->rowCount();
            }

            if (newDropRow != m_dropIndicatorRow)
            {
                m_dropIndicatorRow = newDropRow;
                viewport()->update();
            }

            event->acceptProposedAction();
        }
        else
        {
            QTableView::dragMoveEvent(event);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        m_dropIndicatorRow = -1;
        viewport()->update();
        QTableView::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        m_dropIndicatorRow = -1;
        viewport()->update();
        QTableView::dropEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid())
        {
            setCurrentIndex(index);

            // Update action states
            int row = index.row();
            int rowCount = m_stockModel->rowCount();

            // Enable/disable move actions based on current position
            m_moveUpAction->setEnabled(row > 0);
            m_moveDownAction->setEnabled(row < rowCount - 1);
            m_moveToTopAction->setEnabled(row > 0);
            m_moveToBottomAction->setEnabled(row < rowCount - 1);

            m_contextMenu->exec(event->globalPos());
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete)
        {
            QModelIndex currentIndex = this->currentIndex();
            if (currentIndex.isValid())
            {
                onDeleteStock();
            }
            event->accept();
        }
        else
        {
            QTableView::keyPressEvent(event);
        }
    }

private slots:
    void onDeleteStock()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid())
        {
            m_stockModel->removeStock(currentIndex.row());
        }
    }

    void onMoveUp()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            int currentRow = currentIndex.row();
            m_stockModel->moveStock(currentRow, currentRow - 1);
            setCurrentIndex(m_stockModel->index(currentRow - 1, 0));
        }
    }

    void onMoveDown()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() < m_stockModel->rowCount() - 1)
        {
            int currentRow = currentIndex.row();
            m_stockModel->moveStock(currentRow, currentRow + 1);
            setCurrentIndex(m_stockModel->index(currentRow + 1, 0));
        }
    }

    void onMoveToTop()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            int currentRow = currentIndex.row();
            m_stockModel->moveStock(currentRow, 0);
            setCurrentIndex(m_stockModel->index(0, 0));
        }
    }

    void onMoveToBottom()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() < m_stockModel->rowCount() - 1)
        {
            int currentRow = currentIndex.row();
            int lastRow = m_stockModel->rowCount() - 1;
            m_stockModel->moveStock(currentRow, lastRow);
            setCurrentIndex(m_stockModel->index(lastRow, 0));
        }
    }

private:
    void setupView()
    {
        // Set selection behavior
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);

        // Enable drag and drop
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDragDropOverwriteMode(false);

        // Set alternating row colors for subtle visual separation
        setAlternatingRowColors(true);

        // Disable editing
        setEditTriggers(QAbstractItemView::NoEditTriggers);

        // Configure headers
        horizontalHeader()->setStretchLastSection(true);
        horizontalHeader()->setHighlightSections(false);
        horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        verticalHeader()->setVisible(false);

        // Minimalist grid style - no visible grid lines
        setShowGrid(false);

        // Enable keyboard focus but remove focus rectangle
        setFocusPolicy(Qt::StrongFocus);

        // Set sorting
        setSortingEnabled(false);  // Disable sorting to maintain user order

        // Set compact row height

        // Create a custom item delegate to preserve colors in selected state
        setItemDelegate(new ColorPreservingDelegate(this));
    }

    void setupContextMenu()
    {
        setContextMenuPolicy(Qt::DefaultContextMenu);

        m_contextMenu = new QMenu(this);

        // Add move actions first
        m_moveUpAction = m_contextMenu->addAction("Move Up");
        m_moveDownAction = m_contextMenu->addAction("Move Down");
        m_moveToTopAction = m_contextMenu->addAction("Move to Top");
        m_moveToBottomAction = m_contextMenu->addAction("Move to Bottom");

        // Add separator
        m_contextMenu->addSeparator();

        // Add delete action
        m_deleteAction = m_contextMenu->addAction("Delete");

        // Connect signals
        connect(m_moveUpAction, &QAction::triggered, this, &StockTableView::onMoveUp);
        connect(m_moveDownAction, &QAction::triggered, this, &StockTableView::onMoveDown);
        connect(m_moveToTopAction, &QAction::triggered, this, &StockTableView::onMoveToTop);
        connect(m_moveToBottomAction, &QAction::triggered, this, &StockTableView::onMoveToBottom);
        connect(m_deleteAction, &QAction::triggered, this, &StockTableView::onDeleteStock);
    }

    StockModel *m_stockModel;
    QMenu *m_contextMenu;
    QAction *m_deleteAction;
    QAction *m_moveUpAction;
    QAction *m_moveDownAction;
    QAction *m_moveToTopAction;
    QAction *m_moveToBottomAction;

    // Drag and drop indicator
    int m_dropIndicatorRow;
};
