#pragma once

#include <QApplication>
#include <QDesktopWidget>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QScreen>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#include "Logger.hpp"
#include "StockInfo.hpp"

class StockSearchResultTable : public QTableView
{
    Q_OBJECT

public:
    explicit StockSearchResultTable(const std::shared_ptr<StockConfig> &config, QWidget *parent = nullptr)
        : QTableView(parent)
        , config_(config)
        , model_(new QStandardItemModel(this))
        , selectedIndex_(-1)
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
        auto rows = model_->rowCount();
        if (rows == 0) return;

        int nextRow = (this->currentIndex().row() + 1 + rows) % rows;
        this->setCurrentIndex(model_->index(nextRow, 0));
        this->selectRow(nextRow);
    }
    void selectPrevRow()
    {
        auto rows = model_->rowCount();
        if (rows == 0) return;

        int prevRow = (this->currentIndex().row() - 1 + rows) % rows;
        this->setCurrentIndex(model_->index(prevRow, 0));
        this->selectRow(prevRow);
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

    void showResults(const QList<StockInfo> &results)
    {
        stocks_ = results;
        // result_->clear();
        selectedIndex_ = -1;

        model_->clear();
        model_->setColumnCount(2);
        for (auto &&it : results)
        {
            auto items = { new QStandardItem(it.code()), new QStandardItem(it.name()) };
            if (config_->stockCodes.contains(it.code()))
            {
                // for (auto item : items)
                // {
                // item->setForeground(Qt::gray);
                // QFont ft = item->font();
                // ft.setItalic(true);
                // item->setFont(ft);
                // }
                continue;
            }
            model_->appendRow(items);
        }
        show();
    }

signals:
    void stockSelected(const QString &code);

private:
    std::shared_ptr<StockConfig> config_;
    QStandardItemModel *model_;

    QList<StockInfo> stocks_;
    int selectedIndex_;
};
