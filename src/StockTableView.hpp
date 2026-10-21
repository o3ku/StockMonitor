#pragma once

#include "Logger.hpp"
#include "StockInfo.hpp"
#include "StockModel.hpp"
#include <QAbstractItemView>
#include <QAction>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QMenu>
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
        , config_(config)
        , model_(new StockModel(config, this))
        , dopIndicatorRow_(-1)
    {
        setupView(config->rowHeight);
        setupContextMenu();

        setModel(model_);
        for (auto i = 0; i < model_->columnCount(); ++i)
        {
            setColumnWidth(i, model_->width(i));
        }

        connect(this, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos) {
            QModelIndex index = indexAt(pos);
            if (index.isValid())
            {
                setCurrentIndex(index);

                updateMoveActions(index.row(), model_->rowCount());

                emit contextMenuOpened();
                menu_->exec(mapToGlobal(pos));
                emit contextMenuClosed();
            }
        });
    }

    void applyRowHeight(int rowHeight)
    {
        verticalHeader()->setDefaultSectionSize(rowHeight);
        int rows = model_->rowCount();
        for (int row = 0; row < rows; ++row)
        {
            setRowHeight(row, rowHeight);
        }
        viewport()->update();
    }

    void addStock(const QString &stock)
    {
        int row0 = model_->addStock(stock);
        if (row0 < 0) row0 = 0;
        setCurrentIndex(model_->index(row0, 0));
        scrollTo(model_->index(row0, 0));
        selectRow(row0);
    }

    void removeCurrent()
    {
        onDeleteStock();
    }

    void updateStocksDetail(const QList<StockInfo> &arr)
    {
        model_->updateStocksDetail(arr);
    }

    void selectNextRow()
    {
        selectRelativeRow(1);
    }
    void selectPrevRow()
    {
        selectRelativeRow(-1);
    }

signals:
    void contextMenuOpened();
    void contextMenuClosed();

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTableView::paintEvent(event);

        // Draw custom drop indicator for full row
        if (dopIndicatorRow_ >= 0)
        {
            QPainter painter(viewport());
            QColor indicatorColor = config_->isDarkTheme() ? QColor(0, 160, 255) : QColor(0, 120, 215);
            painter.setPen(QPen(indicatorColor, 2));

            int y;
            if (dopIndicatorRow_ >= model()->rowCount())
            {
                // Drop at the end
                QRect lastRowRect = visualRect(model()->index(model()->rowCount() - 1, 0));
                y = lastRowRect.bottom() + 1;
            }
            else
            {
                // Drop before the specified row
                QRect rowRect = visualRect(model()->index(dopIndicatorRow_, 0));
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
            dopIndicatorRow_ = -1;
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

            if (newDropRow != dopIndicatorRow_)
            {
                dopIndicatorRow_ = newDropRow;
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
        dopIndicatorRow_ = -1;
        viewport()->update();
        QTableView::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        dopIndicatorRow_ = -1;
        viewport()->update();
        QTableView::dropEvent(event);
    }

private slots:
    void onDeleteStock()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid())
        {
            model_->removeStock(currentIndex.row());
        }
    }

    void onMoveUp()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            int currentRow = currentIndex.row();
            int row = model_->moveStock(currentRow, currentRow - 1);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        }
    }

    void onMoveDown()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() < model_->rowCount() - 1)
        {
            int currentRow = currentIndex.row();
            int row = model_->moveStock(currentRow, currentRow + 1);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        }
    }

    void onMoveToTop()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() > 0)
        {
            int currentRow = currentIndex.row();
            int row = model_->moveStock(currentRow, 0);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        }
    }

    void onMoveToBottom()
    {
        QModelIndex currentIndex = this->currentIndex();
        if (currentIndex.isValid() && currentIndex.row() < model_->rowCount() - 1)
        {
            int currentRow = currentIndex.row();
            int row = model_->moveStock(currentRow, model_->rowCount() - 1);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        }
    }

private:
    void selectRelativeRow(int delta)
    {
        int rows = model_->rowCount();
        if (rows == 0) return;

        int nextRow = (currentIndex().row() + delta + rows) % rows;
        setCurrentIndex(model_->index(nextRow, 0));
        selectRow(nextRow);
    }

    void updateMoveActions(int row, int rowCount)
    {
        // Moves are clamped inside the row's own block (pinned on top),
        // so disable actions that would cross the boundary or no-op.
        int pinnedCount = model_->pinnedCount();
        bool inPinnedBlock = row < pinnedCount;
        actUp_->setEnabled(row > (inPinnedBlock ? 0 : pinnedCount));
        actDown_->setEnabled(row < (inPinnedBlock ? pinnedCount - 1 : rowCount - 1));
        actTop_->setEnabled(actUp_->isEnabled());
        actBottom_->setEnabled(actDown_->isEnabled());

        bool pinned = model_->isPinned(row);

        actPin_->setVisible(!pinned);
        actUnpin_->setVisible(pinned);
    }

    void setupView(int rowHeight)
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
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        horizontalHeader()->setStretchLastSection(true);
        horizontalHeader()->setHighlightSections(false);
        horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        horizontalHeader()->hide();
        verticalHeader()->hide();
        verticalHeader()->setDefaultSectionSize(rowHeight);

        setItemDelegate(new ColorPreservingDelegate(this));
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
        actPin_ = menu_->addAction("Pin to Top", this, [this] {
            int row = model_->setPinned(currentIndex().row(), true);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        });
        actUnpin_ = menu_->addAction("Unpin", this, [this] {
            int row = model_->setPinned(currentIndex().row(), false);
            if (row >= 0) setCurrentIndex(model_->index(row, 0));
        });
        menu_->addSeparator();
        actDelete_ = menu_->addAction("Delete", this, &StockTableView::onDeleteStock);
    }

private:
    std::shared_ptr<StockConfig> config_;
    StockModel *model_;
    QMenu *menu_;
    QAction *actDelete_;
    QAction *actUp_;
    QAction *actDown_;
    QAction *actTop_;
    QAction *actBottom_;
    QAction *actPin_;
    QAction *actUnpin_;
    int dopIndicatorRow_;
};
