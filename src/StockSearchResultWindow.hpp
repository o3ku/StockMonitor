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

class StockSearchWidget : public QTableView
{
    Q_OBJECT

public:
    explicit StockSearchWidget(const std::shared_ptr<StockConfig> &config, IStockApiProvider *api, QWidget *parent = nullptr)
        : QTableView(parent)
        , config_(config)
        , model_(new QStandardItemModel(this))
        , api_(api)
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
        this->setModel(model_);

        connect(this, &QTableView::activated, this, &StockSearchWidget::chooseCurrentRow);
        // connect(result_, &QTableView::itemClicked, this, [this](QListWidgetItem *item) {
        //     int row = result_->row(item);
        //     auto stock = stocks_.value(row);
        //     config_->stockCodes << stock.code();
        //     input_->clear();
        //     showInputMode();
        // });
    }

    void selectNextRow()
    {
        auto rows = model_->rowCount();
        this->selectRow((this->currentIndex().row() + 1 + rows) % rows);
    }
    void selectPrevRow()
    {
        auto rows = model_->rowCount();
        this->selectRow((this->currentIndex().row() - 1 + rows) % rows);
    }
    void chooseCurrentRow(const QModelIndex &index)
    {
        auto code = model_->index(this->currentIndex().row(), 0).data(Qt::DisplayRole).toString();
        hide();
        emit stockSelected(code);
    }

    void showResults(const QList<StockInfo> &results)
    {
        // Sort results according to market priority
        // stocks_ = sortResultsByMarket(results);
        stocks_ = results;
        // result_->clear();
        selectedIndex_ = -1;

        model_->clear();
        model_->setColumnCount(2);
        for (auto &&it : results)
        {
            model_->appendRow({ new QStandardItem(it.code()), new QStandardItem(it.name()) });
        }
        show();
    }

    void hideResults()
    {
        this->hide();
    }

signals:
    void stockSelected(const QString &code);

private:
    void onItemClicked(int row)
    {
        // if (row >= 0 && row < stocks_.size())
        // {
        //     emit stockSelected(stocks_[row]);
        // }
    }

private:
    QWidget *createResultItem(const StockInfo &result, int index)
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
        // if (index >= 0 && index < result_->count())
        //{
        //    selectedIndex_ = index;
        //    result_->setCurrentRow(index);
        //}
    }

    QList<StockInfo> sortResultsByMarket(const QList<StockInfo> &results) const
    {
        QList<StockInfo> sortedResults = results;

        // Sort by market priority
        std::sort(sortedResults.begin(), sortedResults.end(), [this](const StockInfo &a, const StockInfo &b) {
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

private:
    std::shared_ptr<StockConfig> config_;
    QStandardItemModel *model_;
    IStockApiProvider *api_{ nullptr };

    QList<StockInfo> stocks_;
    int selectedIndex_;
};
