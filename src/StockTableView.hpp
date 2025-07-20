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
#include <QMimeData>
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
    explicit StockTableView(const std::shared_ptr<StockConfig> &config, QWidget *parent = nullptr)
        : QTableView(parent)
        , model_(new StockModel(config, this))
        , m_dropIndicatorRow(-1)
    {
        setupView();
        setupContextMenu();

        setModel(model_);
        for (auto i = 0; i < model_->columnCount(); ++i)
        {
            setColumnWidth(i, model_->width(i));
        }

        connect(this, &QTableView::customContextMenuRequested, this, [this](auto &&pos) {
            menu_->exec(QCursor::pos());
        });
    }

    void addStock(const QString &stock)
    {
        model_->addStock(stock);
    }

    void updateStocksDetail(const QList<StockInfo> &arr)
    {
        model_->updateStocksDetail(arr);
    }

    QList<StockInfo> stockData() const
    {
        // return m_stockModel->stockData();
        return {};
    }

    void setStockData(const QList<StockInfo> &stockData)
    {
        // m_stockModel->setStockData(stockData);
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
            // setCurrentIndex(index);

            // // Update action states
            // int row = index.row();
            // int rowCount = m_stockModel->rowCount();

            // // Enable/disable move actions based on current position
            // m_moveUpAction->setEnabled(row > 0);
            // m_moveDownAction->setEnabled(row < rowCount - 1);
            // m_moveToTopAction->setEnabled(row > 0);
            // m_moveToBottomAction->setEnabled(row < rowCount - 1);

            // m_contextMenu->exec(event->globalPos());
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
            // m_stockModel->removeStock(currentIndex.row());
        }
    }

    void onMoveUp()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            int currentRow = currentIndex.row();
            model_->moveStock(currentRow, currentRow - 1);
            setCurrentIndex(model_->index(currentRow - 1, 0));
        }
    }

    void onMoveDown()
    {
        // QModelIndex currentIndex = this->currentIndex();
        // if (currentIndex.isValid() && currentIndex.row() < m_stockModel->rowCount() - 1)
        // {
        //     int currentRow = currentIndex.row();
        //     m_stockModel->moveStock(currentRow, currentRow + 1);
        //     setCurrentIndex(m_stockModel->index(currentRow + 1, 0));
        // }
    }

    void onMoveToTop()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            // int currentRow = currentIndex.row();
            // m_stockModel->moveStock(currentRow, 0);
            // setCurrentIndex(m_stockModel->index(0, 0));
        }
    }

    void onMoveToBottom()
    {
        // QModelIndex currentIndex = this->currentIndex();
        // if (currentIndex.isValid() && currentIndex.row() < m_stockModel->rowCount() - 1)
        // {
        //     int currentRow = currentIndex.row();
        //     int lastRow = m_stockModel->rowCount() - 1;
        //     m_stockModel->moveStock(currentRow, lastRow);
        //     setCurrentIndex(m_stockModel->index(lastRow, 0));
        // }
    }

private:
    void setupView()
    {
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);

        // Enable drag and drop
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDragDropOverwriteMode(false);

        setShowGrid(false);
        setAlternatingRowColors(true);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setFocusPolicy(Qt::StrongFocus);
        setSortingEnabled(false);

        horizontalHeader()->setStretchLastSection(true);
        horizontalHeader()->setHighlightSections(false);
        horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);

        verticalHeader()->setVisible(false);
    }

    void setupContextMenu()
    {
        setContextMenuPolicy(Qt::CustomContextMenu);

        menu_ = new QMenu(this);

        actUp_ = menu_->addAction("Move Up", this, &StockTableView::onMoveUp);
        actDown_ = menu_->addAction("Move Down", this, &StockTableView::onMoveDown);
        actTop_ = menu_->addAction("Move to Top", this, &StockTableView::onMoveToTop);
        actBottom_ = menu_->addAction("Move to Bottom", this, &StockTableView::onMoveToBottom);
        menu_->addSeparator();
        actDelete_ = menu_->addAction("Delete", this, &StockTableView::onDeleteStock);
    }

private:
    StockModel *model_;
    QMenu *menu_;
    QAction *actDelete_;
    QAction *actUp_;
    QAction *actDown_;
    QAction *actTop_;
    QAction *actBottom_;

    // Drag and drop indicator
    int m_dropIndicatorRow;
};
