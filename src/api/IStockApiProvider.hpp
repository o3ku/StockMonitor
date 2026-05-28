#pragma once

#include "../StockInfo.hpp"
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

// Abstract interface for stock API providers
class IStockApiProvider : public QObject
{
    Q_OBJECT

public:
    explicit IStockApiProvider(QObject *parent = nullptr)
        : QObject(parent)
        , netmgr_(new QNetworkAccessManager(this))
    {
    }
    virtual ~IStockApiProvider() = default;
    virtual QString name() const = 0;
    virtual void searchStocksAsync(const QString &keyword, quint64 requestId) = 0;
    virtual void getStockInfosAsync(const QStringList &codes) = 0;

signals:
    void searchCompleted(quint64 requestId, const QString &keyword, const QList<StockInfo> &results);
    void stockInfosReceived(const QList<StockInfo> &stockInfos);
    void errorOccurred(const QString &error);

protected:
    QNetworkAccessManager *netmgr_;
};
