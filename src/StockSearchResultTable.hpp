#pragma once

#include <QHeaderView>
#include <QStandardItemModel>
#include <QTableView>
#include "StockInfo.hpp"

class StockSearchResultTable : public QTableView
{
    Q_OBJECT

public:
    explicit StockSearchResultTable(const std::shared_ptr<StockConfig> &config, QWidget *parent = nullptr)
        : QTableView(parent)
        , config_(config)
        , model_(new QStandardItemModel(this))
    {
        hide();
        model_->setColumnCount(2);
        this->setObjectName("searchResultsList");
        this->horizontalHeader()->hide();
        this->verticalHeader()->hide();
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setSelectionBehavior(QTableView::SelectRows);
        this->setSelectionMode(QTableView::SingleSelection);
        this->setEditTriggers(QTableView::NoEditTriggers);
        this->setShowGrid(false);
        this->setModel(model_);

        connect(this, &QTableView::activated, this, &StockSearchResultTable::chooseCurrentRow);
        connect(this, &QTableView::clicked, this, &StockSearchResultTable::chooseCurrentRow);
    }

    void selectNextRow()
    {
        selectRelativeRow(1);
    }
    void selectPrevRow()
    {
        selectRelativeRow(-1);
    }
    void chooseCurrentRow(const QModelIndex &index)
    {
        int row = index.isValid() ? index.row() : this->currentIndex().row();
        if (row >= 0 && row < model_->rowCount())
        {
            auto code = model_->index(row, 0).data(Qt::DisplayRole).toString();
            hide();
            emit stockSelected(code);
        }
    }

    bool showResults(const QList<StockInfo> &results)
    {
        model_->clear();
        model_->setColumnCount(2);
        for (auto &&it : results)
        {
            auto items = { new QStandardItem(it.code()), new QStandardItem(it.name()) };
            if (config_->stockCodes.contains(it.code()))
            {
                // for (auto item : items)
                continue;  // already in watchlist (gray-out variant abandoned)
            }
            model_->appendRow(items);
        }

        if (model_->rowCount() == 0)
        {
            hide();
            return false;
        }

        setColumnWidth(0, qMin(92, qMax(64, viewport()->width() / 3)));
        horizontalHeader()->setStretchLastSection(true);
        selectRow(0);
        raise();
        show();
        return true;
    }

signals:
    void stockSelected(const QString &code);

private:
    void selectRelativeRow(int delta)
    {
        int rows = model_->rowCount();
        if (rows == 0) return;

        int nextRow = (currentIndex().row() + delta + rows) % rows;
        setCurrentIndex(model_->index(nextRow, 0));
        selectRow(nextRow);
    }

    std::shared_ptr<StockConfig> config_;
    QStandardItemModel *model_;
};
