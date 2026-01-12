#pragma once

#include "api/EastMoneyStockApiProvider.hpp"
#include "api/IStockApiProvider.hpp"
#include "api/SinaStockApiProvider.hpp"
#include "api/TencentStockApiProvider.hpp"
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

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
        return activeProvider_;
    }

    QString activeProviderName() const
    {
        return activeProvider_ ? activeProvider_->name() : QString();
    }

    bool setActiveProvider(const QString &name)
    {
        IStockApiProvider *provider = getOrCreateProvider(name);
        if (!provider)
        {
            emit errorOccurred(QString("No API provider for name: %1").arg(name));
            return false;
        }

        bool exactMatch = provider->name() == normalizeName(name);
        if (activeProvider_ != provider)
        {
            if (activeProvider_) disconnectProvider(activeProvider_);
            activeProvider_ = provider;
            connectProvider(activeProvider_);
            emit providerChanged(activeProvider_->name());
        }

        return exactMatch;
    }

    IStockApiProvider *getProvider(const QString &name)
    {
        return getOrCreateProvider(name);
    }

    void searchStocksAsync(const QString &keyword)
    {
        ensureActiveProvider();
        if (!activeProvider_)
        {
            emit errorOccurred("No active API provider");
            return;
        }
        activeProvider_->searchStocksAsync(keyword);
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
    void searchCompleted(const QList<StockInfo> &results);
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

    IStockApiProvider *getOrCreateProvider(const QString &name)
    {
        const QString normalized = normalizeName(name);
        const QString resolved = factories_.contains(normalized) ? normalized : QString("sina");

        QPointer<IStockApiProvider> existing = providers_.value(resolved);
        if (!existing)
        {
            ProviderFactory factory = factories_.value(resolved);
            if (!factory) return nullptr;
            existing = factory(this);
            providers_.insert(resolved, existing);
        }
        return existing;
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
    QMap<QString, QPointer<IStockApiProvider>> providers_;
    QPointer<IStockApiProvider> activeProvider_;
};
