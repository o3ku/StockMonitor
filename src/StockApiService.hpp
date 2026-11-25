#pragma once

#include "api/EastMoneyStockApiProvider.hpp"
#include "api/IStockApiProvider.hpp"
#include "api/SinaStockApiProvider.hpp"
#include "api/TencentStockApiProvider.hpp"
#include <QMap>
#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QStringList>
#include <functional>

// API Manager class - manages different API providers
class StockApiService : public QObject
{
    Q_OBJECT

public:
    static StockApiService &instance()
    {
        static StockApiService inst;
        return inst;
    }

    IStockApiProvider *activeProvider() const
    {
        return activeProvider_.data();
    }

    QString activeProviderName() const
    {
        return activeProvider_ ? activeProvider_->name() : QString();
    }

    bool setActiveProvider(const QString &name)
    {
        const QString normalized = normalizeName(name);
        const QString resolved = factories_.contains(normalized) ? normalized : QString("sina");
        const bool exactMatch = resolved == normalized;

        if (activeProvider_ && activeProviderName_ == resolved)
        {
            return exactMatch;
        }

        ProviderFactory factory = factories_.value(resolved);
        if (!factory)
        {
            emit errorOccurred(QString("No API provider for name: %1").arg(name));
            return false;
        }

        if (activeProvider_) disconnectProvider(activeProvider_.data());
        activeProvider_.reset(factory(this));
        activeProviderName_ = resolved;
        activeProvider_->setNetworkProxy(networkProxy_);
        connectProvider(activeProvider_.data());
        emit providerChanged(activeProvider_->name());

        return exactMatch;
    }

    void switchProvider(const QString &name)
    {
        setActiveProvider(name);
    }

    // Network proxy: "off" (default, direct) or "system" (OS settings).
    // Stored so a provider created later (e.g. on source switch) gets
    // the same mode, and applied to the active provider immediately.
    void setNetworkProxy(const QString &mode)
    {
        const QString normalized = mode.trimmed().toLower();
        networkProxy_ = normalized == "system" ? "system" : "off";
        if (activeProvider_)
        {
            activeProvider_->setNetworkProxy(networkProxy_);
        }
    }

    quint64 searchStocksAsync(const QString &keyword)
    {
        ensureActiveProvider();
        if (!activeProvider_)
        {
            emit errorOccurred("No active API provider");
            return 0;
        }
        const quint64 requestId = ++nextSearchRequestId_;
        activeProvider_->searchStocksAsync(keyword, requestId);
        return requestId;
    }

    void getStockInfosAsync(const QStringList &codes)
    {
        ensureActiveProvider();
        if (!activeProvider_)
        {
            emit errorOccurred("No active API provider");
            return;
        }
        activeProvider_->getStockInfosAsync(codes);
    }

signals:
    void searchCompleted(quint64 requestId, const QString &keyword, const QList<StockInfo> &results);
    void stockInfosReceived(const QList<StockInfo> &stockInfos);
    void errorOccurred(const QString &error);
    void providerChanged(const QString &name);

private:
    using ProviderFactory = std::function<IStockApiProvider *(QObject *)>;

    StockApiService()
        : QObject(nullptr)
        , factories_({
              { "sina", [](QObject *parent) { return new SinaStockApiProvider(parent); } },
              { "tencent", [](QObject *parent) { return new TencentStockApiProvider(parent); } },
              { "eastmoney", [](QObject *parent) { return new EastMoneyStockApiProvider(parent); } },
          })
    {
    }

    void ensureActiveProvider()
    {
        if (!activeProvider_)
        {
            setActiveProvider("sina");
        }
    }

    void connectProvider(IStockApiProvider *provider)
    {
        if (!provider) return;
        connect(provider, &IStockApiProvider::searchCompleted, this, &StockApiService::searchCompleted, Qt::UniqueConnection);
        connect(provider, &IStockApiProvider::stockInfosReceived, this, &StockApiService::stockInfosReceived, Qt::UniqueConnection);
        connect(provider, &IStockApiProvider::errorOccurred, this, &StockApiService::errorOccurred, Qt::UniqueConnection);
    }

    void disconnectProvider(IStockApiProvider *provider)
    {
        if (!provider) return;
        disconnect(provider, nullptr, this, nullptr);
    }

    QString normalizeName(const QString &name) const
    {
        QString normalized = name.trimmed().toLower();
        if (normalized == "easymoney")
        {
            normalized = "eastmoney";
        }
        return normalized;
    }

    QMap<QString, ProviderFactory> factories_;
    QScopedPointer<IStockApiProvider> activeProvider_;
    QString activeProviderName_;
    QString networkProxy_{ "off" };
    quint64 nextSearchRequestId_{ 0 };
};
