#ifndef STOCKMODEL_HPP
#define STOCKMODEL_HPP

#include <QAbstractTableModel>
#include <QColor>
#include <QDateTime>
#include <QMimeData>
#include <QStringList>
#include <QMap>
#include "StockInfo.hpp"
#include "Logger.hpp"

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

    explicit StockModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return m_stockCodes.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return ColumnCount;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= m_stockCodes.size())
            return QVariant();

        const QString &code = m_stockCodes.at(index.row());
        if (!m_stockData.contains(code))
            return QVariant();

        const StockInfo &stock = m_stockData.value(code);

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
                return getStockColor(stock);
            case NameColumn:
                return getDefaultTextColor();
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
            {

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
        if (!index.isValid())
            return Qt::NoItemFlags;

        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

    // Drag and drop support
    Qt::DropActions supportedDropActions() const override
    {
        return Qt::MoveAction;
    }

    QStringList mimeTypes() const override
    {
        QStringList types;
        types << "application/x-stockrow";
        return types;
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QMimeData *mimeData = new QMimeData();
        QByteArray encodedData;

        QDataStream stream(&encodedData, QIODevice::WriteOnly);

        foreach (const QModelIndex &index, indexes)
        {
            if (index.isValid() && index.column() == 0) // Only process first column to avoid duplicates
            {
                stream << index.row();
            }
        }

        mimeData->setData("application/x-stockrow", encodedData);
        return mimeData;
    }

    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override
    {
        Q_UNUSED(column)

        if (action == Qt::IgnoreAction)
            return true;

        if (!data->hasFormat("application/x-stockrow"))
            return false;

        int beginRow;
        if (row != -1)
            beginRow = row;
        else if (parent.isValid())
            beginRow = parent.row();
        else
            beginRow = rowCount(QModelIndex());

        QByteArray encodedData = data->data("application/x-stockrow");
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
            if (sourceRow >= 0 && sourceRow < m_stockCodes.size() && beginRow >= 0 && beginRow <= m_stockCodes.size())
            {
                moveStock(sourceRow, beginRow);
                return true;
            }
        }

        return false;
    }

    // Custom methods
    void addStock(const StockInfo &stock)
    {
        // Check if stock already exists
        if (m_stockData.contains(stock.code()))
        {
            Logger::info(QString("Stock %1 already exists!").arg(stock.code()));
            return;
        }

        beginInsertRows(QModelIndex(), m_stockCodes.size(), m_stockCodes.size());
        m_stockCodes.append(stock.code());
        m_stockData.insert(stock.code(), stock);
        endInsertRows();

        Logger::info(QString("Added stock: %1 - %2").arg(stock.code(), stock.name()));
        emit stockListChanged();
    }

    void updateStockData(const QList<StockInfo> &newData)
    {
        int updatedCount = 0;
        for (const StockInfo &newStock : newData)
        {
            if (m_stockData.contains(newStock.code()))
            {
                // Find the row index for this stock code
                int row = m_stockCodes.indexOf(newStock.code());
                if (row >= 0)
                {
                    m_stockData[newStock.code()] = newStock;
                    QModelIndex topLeft = index(row, 0);
                    QModelIndex bottomRight = index(row, ColumnCount - 1);
                    emit dataChanged(topLeft, bottomRight);
                    updatedCount++;
                    Logger::info(QString("Updated stock %1: %2 at price %3")
                                     .arg(newStock.code())
                                     .arg(newStock.name())
                                     .arg(QString::number(newStock.currentPrice(), 'f', 2)));
                }
            }
        }
        Logger::info(QString("StockModel: Updated %1 stocks, triggered dataChanged signals").arg(updatedCount));
    }

    void removeStock(int row)
    {
        if (row >= 0 && row < m_stockCodes.size())
        {
            QString code = m_stockCodes.at(row);
            beginRemoveRows(QModelIndex(), row, row);
            m_stockCodes.removeAt(row);
            m_stockData.remove(code);
            endRemoveRows();
            emit stockListChanged();
            Logger::info(QString("Deleted stock %1 at row %2").arg(code).arg(row));
        }
    }

    void moveStock(int from, int to)
    {
        if (from >= 0 && from < m_stockCodes.size() && to >= 0 && to < m_stockCodes.size() && from != to)
        {
            // Calculate the correct destination index for beginMoveRows
            int destinationIndex = to;
            if (to > from)
                destinationIndex++; // When moving down, destination is after the target row

            if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), destinationIndex))
            {
                QString code = m_stockCodes.takeAt(from);
                m_stockCodes.insert(to, code);
                endMoveRows();
                emit stockListChanged();
                Logger::info(QString("Moved stock from row %1 to row %2").arg(from).arg(to));
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
        for (const QString &code : m_stockCodes)
        {
            if (m_stockData.contains(code))
            {
                result.append(m_stockData.value(code));
            }
        }
        return result;
    }

    void setStockData(const QList<StockInfo> &stockData)
    {
        beginResetModel();
        m_stockCodes.clear();
        m_stockData.clear();
        for (const StockInfo &stock : stockData)
        {
            m_stockCodes.append(stock.code());
            m_stockData.insert(stock.code(), stock);
        }
        endResetModel();
    }

signals:
    void stockListChanged();

private:
    QString formatVolume(long long volume) const
    {
        if (volume >= 100000000) // 1亿
        {
            return QString("%1亿").arg(volume / 100000000.0, 0, 'f', 2);
        }
        else if (volume >= 10000) // 1万
        {
            return QString("%1万").arg(volume / 10000.0, 0, 'f', 1);
        }
        else
        {
            return QString::number(volume);
        }
    }

    QColor getStockColor(const StockInfo &stock) const
    {
        double change = stock.change();

        // Detect current theme for better color contrast
        QString styleSheet = qApp->styleSheet();
        bool isDarkTheme = styleSheet.contains("dark.css") || styleSheet.contains("#2b2b2b");

        if (change > 0)
        {
            // Red for up (涨红) - brighter red for dark theme
            return isDarkTheme ? QColor(255, 85, 85) : QColor(220, 20, 60);
        }
        else if (change < 0)
        {
            // Green for down (跌绿) - brighter green for dark theme
            return isDarkTheme ? QColor(85, 255, 85) : QColor(34, 139, 34);
        }
        else
        {
            // Gray for flat (平灰) - lighter gray for dark theme
            return isDarkTheme ? QColor(180, 180, 180) : QColor(128, 128, 128);
        }
    }

    QColor getDefaultTextColor() const
    {
        // Detect current theme by checking application stylesheet
        QString styleSheet = qApp->styleSheet();
        if (styleSheet.contains("dark.css") || styleSheet.contains("#2b2b2b"))
        {
            return QColor(255, 255, 255); // White for dark theme
        }
        else
        {
            return QColor(0, 0, 0); // Black for light theme
        }
    }

    QMap<QString, StockInfo> m_stockData; // Map for fast lookup by code
    QStringList m_stockCodes;             // List to maintain display order
};

#endif // STOCKMODEL_HPP
