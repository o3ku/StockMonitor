#pragma once

#include "../StockInfo.hpp"
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
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

    // Network proxy for all requests made through netmgr_ (all three
    // providers share this base class, so one call covers Sina, Tencent
    // and Eastmoney). Applies to future requests only, per QNAM docs.
    // "system": use the OS proxy settings. Anything else: direct, no proxy.
    void setNetworkProxy(const QString &mode)
    {
        if (mode.trimmed().toLower() == "system")
        {
            // DefaultProxy alone is not enough: it only resolves through
            // the application proxy factory, which is NoProxy unless the
            // system configuration is enabled.
            QNetworkProxyFactory::setUseSystemConfiguration(true);
            netmgr_->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        }
        else
        {
            netmgr_->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        }
    }

signals:
    void searchCompleted(quint64 requestId, const QString &keyword, const QList<StockInfo> &results);
    void stockInfosReceived(const QList<StockInfo> &stockInfos);
    void errorOccurred(const QString &error);

protected:
    QNetworkAccessManager *netmgr_;
};
