#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "StockInfo.hpp"
#include "StockSearchResult.hpp"

// Abstract interface for stock API providers
class IStockApiProvider : public QObject
{
    Q_OBJECT

public:
    explicit IStockApiProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    virtual ~IStockApiProvider() = default;
    virtual void searchStocksAsync(const QString &keyword) = 0;
    virtual void getStockInfosAsync(const QStringList &codes) = 0;

signals:
    void searchCompleted(const QList<StockSearchResult> &results);
    void stockInfosReceived(const QList<StockInfo> &stockInfos);
    void errorOccurred(const QString &error);
};
