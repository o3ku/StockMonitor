#pragma once

#include "Logger.hpp"
#include "StockInfo.hpp"
#include <QAbstractTableModel>
#include <QDataStream>
#include <QMap>
#include <QMimeData>
#include <QSet>
#include <QStringList>

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
        // if (!stocks_.contains(code)) return {};
        const StockInfo &stock = stocks_.value(code);

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
            case Qt::DisplayRole: {

                switch (section)
                {
                case NameColumn:
                    return "Name";
                case CurrentPriceColumn:
                    return "Price";
                case ChangePercentColumn:
                    return "Change";
                }
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
        const QList<StockInfo> updates = filterTrackedStocks(arr);
        if (updates.isEmpty()) return;
        for (const StockInfo &it : updates)
        {
            stocks_[it.code()] = it;
        }
        if (rowCount() > 0 && columnCount() > 0)
        {
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                { Qt::DisplayRole, Qt::ForegroundRole, Qt::ToolTipRole });
        }
    }
    
    void addStock(const QString &code)
    {
        if (config_->stockCodes.contains(code)) return;
        beginInsertRows(QModelIndex(), 0, 0);
        config_->stockCodes.prepend(code);
        stocks_[code] = StockInfo(code);
        endInsertRows();
        config_->save();
    }

    void removeStock(int row)
    {
        if (row >= 0 && row < config_->stockCodes.size())
        {
            QString code = config_->stockCodes.at(row);
            beginRemoveRows(QModelIndex(), row, row);
            config_->stockCodes.removeAt(row);
            stocks_.remove(code);
            endRemoveRows();
            config_->save();
        }
    }

    void moveStock(int from, int to)
    {
        if (from >= 0 && from < config_->stockCodes.size() && to >= 0 && to < config_->stockCodes.size() && from != to)
        {
            // Calculate the correct destination index for beginMoveRows
            int destinationIndex = to;
            if (to > from) destinationIndex++;  // When moving down, destination is after the target row

            if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), destinationIndex))
            {
                QString code = config_->stockCodes.takeAt(from);
                config_->stockCodes.insert(to, code);
                endMoveRows();
                config_->save();
            }
            else
            {
                Logger::error(QString("Failed to begin move rows from %1 to %2").arg(from).arg(to));
            }
        }
    }

    QList<StockInfo> stockData() const
    {
        QList<StockInfo> result;
        for (const QString &code : config_->stockCodes)
        {
            if (stocks_.contains(code))
            {
                result.append(stocks_.value(code));
            }
        }
        return result;
    }

private:
    QList<StockInfo> filterTrackedStocks(const QList<StockInfo> &arr) const
    {
        QSet<QString> trackedCodes = config_->stockCodes.toSet();
        QList<StockInfo> updates;
        updates.reserve(arr.size());
        for (const StockInfo &it : arr)
        {
            if (trackedCodes.contains(it.code()))
            {
                updates.append(it);
            }
        }
        return updates;
    }

    std::shared_ptr<StockConfig> config_;
    QMap<QString, StockInfo> stocks_;
};
