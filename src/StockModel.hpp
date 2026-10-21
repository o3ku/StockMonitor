#pragma once

#include "Logger.hpp"
#include "StockInfo.hpp"
#include <QAbstractTableModel>
#include <QColor>
#include <QDataStream>
#include <QFont>
#include <QMap>
#include <QMimeData>
#include <QSet>
#include <QStringList>
#include <algorithm>

// MIME type constants for drag and drop
namespace StockConstants
{
    constexpr const char *STOCK_ROW_MIME_TYPE = "application/x-stockrow";
}

class StockModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns
    {
        NameColumn = 0,
        CurrentPriceColumn,
        ChangePercentColumn,
        ColumnCount
    };

    explicit StockModel(const std::shared_ptr<StockConfig> &config, QObject *parent = nullptr)
        : QAbstractTableModel(parent)
        , config_(config)
    {
        // Keep only codes that are still in the list; drop order duplicates.
        QSet<QString> valid(config_->stockCodes.cbegin(), config_->stockCodes.cend());
        config_->pinnedStocks.erase(std::remove_if(config_->pinnedStocks.begin(), config_->pinnedStocks.end(),
            [&](const QString &c) { return !valid.contains(c); }),
            config_->pinnedStocks.end());
        config_->pinnedStocks.erase(std::unique(config_->pinnedStocks.begin(), config_->pinnedStocks.end()),
            config_->pinnedStocks.end());

        for (auto &&it : config_->stockCodes)
        {
            stocks_[it] = StockInfo(it);
        }
    }

public:
    int width(int col) const
    {
        static QList<int> widths{ 90, 70, 70 };
        return widths.value(col);
    }

    // Rows [0, pinnedCount) are pinned and always stay on top.
    int pinnedCount() const
    {
        return config_->pinnedStocks.size();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return stocks_.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return ColumnCount;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= stocks_.size()) return QVariant();

        const QString &code = config_->stockCodes.at(index.row());
        const auto stockIt = stocks_.constFind(code);
        if (stockIt == stocks_.constEnd()) return QVariant();
        const StockInfo &stock = stockIt.value();

        switch (role)
        {
        case Qt::DisplayRole:
            switch (index.column())
            {
            case NameColumn:
                return stock.name();
            case CurrentPriceColumn:
                return QString::number(stock.currentPrice(), 'f', 2);
            case ChangePercentColumn:
                return stock.changePercentText();
            }
            break;

        case Qt::TextAlignmentRole:
            switch (index.column())
            {
            case CurrentPriceColumn:
            case ChangePercentColumn:
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            }

        case Qt::ForegroundRole:
            switch (index.column())
            {
            case CurrentPriceColumn:
            case ChangePercentColumn:
                return config_->getPriceColor(stock.isUp());
            case NameColumn:
                return config_->getTextColor();
            }
            break;

        case Qt::BackgroundRole:
            if (index.row() < pinnedCount())
            {
                return config_->isDarkTheme() ? QColor(56, 56, 62) : QColor(255, 244, 189);
            }
            break;

        case Qt::FontRole:
            if (index.row() < pinnedCount())
            {
                QFont font;
                font.setBold(true);
                return font; // shortcut: bold+bg marks pinned rows; no badge column
            }
            break;

        case Qt::ToolTipRole:
            return stock.detailedTooltip();
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal)
        {
            switch (role)
            {
            case Qt::DisplayRole:
                switch (section)
                {
                case NameColumn:
                    return "Name";
                case CurrentPriceColumn:
                    return "Price";
                case ChangePercentColumn:
                    return "Change";
                }
            case Qt::TextAlignmentRole:
                return section == 0 ? Qt::AlignLeft : Qt::AlignRight;
            }
        }
        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid()) return Qt::NoItemFlags;

        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

    // Drag and drop support
    Qt::DropActions supportedDropActions() const override
    {
        return Qt::MoveAction;
    }

    QStringList mimeTypes() const override
    {
        return { StockConstants::STOCK_ROW_MIME_TYPE };
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QMimeData *mimeData = new QMimeData();
        QByteArray encodedData;

        QDataStream stream(&encodedData, QIODevice::WriteOnly);

        for (const QModelIndex &index : indexes)
        {
            if (index.isValid() && index.column() == 0)  // Only process first column to avoid duplicates
            {
                stream << index.row();
            }
        }

        mimeData->setData(StockConstants::STOCK_ROW_MIME_TYPE, encodedData);
        return mimeData;
    }

    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override
    {
        Q_UNUSED(column)

        if (action == Qt::IgnoreAction) return true;

        if (!data->hasFormat(StockConstants::STOCK_ROW_MIME_TYPE)) return false;

        int beginRow;
        if (row != -1)
            beginRow = row;
        else if (parent.isValid())
            beginRow = parent.row();
        else
            beginRow = rowCount(QModelIndex());

        QByteArray encodedData = data->data(StockConstants::STOCK_ROW_MIME_TYPE);
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QList<int> sourceRows;

        while (!stream.atEnd())
        {
            int sourceRow;
            stream >> sourceRow;
            sourceRows << sourceRow;
        }

        if (sourceRows.size() == 1)
        {
            int sourceRow = sourceRows.first();
            if (sourceRow >= 0 && sourceRow < config_->stockCodes.size() && beginRow >= 0 && beginRow <= config_->stockCodes.size())
            {
                moveStock(sourceRow, beginRow);
                return true;
            }
        }

        return false;
    }

    void updateStocksDetail(const QList<StockInfo> &arr)
    {
        bool changed = false;
        for (const StockInfo &it : arr)
        {
            auto stockIt = stocks_.find(it.code());
            if (stockIt == stocks_.end()) continue;
            stockIt.value() = it;
            changed = true;
        }
        if (changed && rowCount() > 0 && columnCount() > 0)
        {
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                { Qt::DisplayRole, Qt::ForegroundRole, Qt::ToolTipRole });
        }
    }
    
    // Returns the row the stock was inserted at, or -1 if it already exists.
    int addStock(const QString &code)
    {
        if (config_->stockCodes.contains(code)) return -1;
        // Insert below the pinned block so pinned rows stay on top
        int row = config_->pinnedStocks.size();
        beginInsertRows(QModelIndex(), row, row);
        config_->stockCodes.insert(row, code);
        stocks_[code] = StockInfo(code);
        endInsertRows();
        config_->save();
        return row;
    }
    void removeStock(int row)
    {
        if (row >= 0 && row < config_->stockCodes.size())
        {
            QString code = config_->stockCodes.at(row);
            beginRemoveRows(QModelIndex(), row, row);
            config_->stockCodes.removeAt(row);
            config_->pinnedStocks.removeAll(code);
            stocks_.remove(code);
            endRemoveRows();
            config_->save();
        }
    }

    // Moves within the source row's block (pinned rows stay on top);
    // returns the actual row landed on, or -1 if no move happened.
    int moveStock(int from, int to)
    {
        if (from >= 0 && from < config_->stockCodes.size() && to >= 0 && to < config_->stockCodes.size() && from != to)
        {
            // Pinned rows form the top block; clamp moves to the source row's own block
            if (from < config_->pinnedStocks.size())
                to = qMin(to, config_->pinnedStocks.size() - 1);
            else if (to < config_->pinnedStocks.size())
                to = config_->pinnedStocks.size();
            if (from == to) return -1;

            // Calculate the correct destination index for beginMoveRows
            int destinationIndex = to;
            if (to > from) destinationIndex++;  // When moving down, destination is after the target row

            if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), destinationIndex))
            {
                QString code = config_->stockCodes.takeAt(from);
                config_->stockCodes.insert(to, code);
                endMoveRows();
                config_->save();
                return to;
            }
            else
            {
                Logger::error(QString("Failed to begin move rows from %1 to %2").arg(from).arg(to));
            }
        }
        return -1;
    }

    bool isPinned(int row) const
    {
        return row >= 0 && row < config_->stockCodes.size()
            && config_->pinnedStocks.contains(config_->stockCodes.at(row));
    }

    // Pin: move row to the bottom of the pinned block. Unpin: move it to the
    // top of the normal block. Pinned block always stays on top.
    int setPinned(int row, bool pinned)
    {
        if (row < 0 || row >= config_->stockCodes.size()) return -1;

        QString code = config_->stockCodes.at(row);
        int pinnedCount = config_->pinnedStocks.size();
        if (pinned == config_->pinnedStocks.contains(code)) return row;
        int target = pinned ? pinnedCount : pinnedCount - 1;
        moveStock(row, target);
        if (pinned)
            config_->pinnedStocks.append(code);
        else
            config_->pinnedStocks.removeAll(code);

        // Rows shifted between blocks need restyle (bold/background)
        int lo = qMin(row, target);
        int hi = qMax(row, target);
        emit dataChanged(index(lo, 0), index(hi, columnCount() - 1),
            { Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole, Qt::FontRole });
        config_->save();
        return target;
    }

    QList<StockInfo> stockData() const
    {
        QList<StockInfo> result;
        result.reserve(config_->stockCodes.size());
        for (const QString &code : config_->stockCodes)
        {
            const auto stockIt = stocks_.constFind(code);
            if (stockIt != stocks_.constEnd())
            {
                result.append(stockIt.value());
            }
        }
        return result;
    }

private:
    std::shared_ptr<StockConfig> config_;
    QMap<QString, StockInfo> stocks_;
};
