#pragma once

#include "api/EastMoneyStockApiProvider.hpp"
#include "api/IStockApiProvider.hpp"
#include "api/SinaStockApiProvider.hpp"
#include "api/TencentStockApiProvider.hpp"
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

    IStockApiProvider *get(QString src)
    {
        if (!apis_.contains(src))
        {
            src = "sina";
        }
        return apis_.value(src)();
    }

private:
    StockApiService() = default;
    ~StockApiService() = default;

private:
    QMap<QString, std::function<IStockApiProvider *()>> apis_{
        { "sina",
            []() {
                return new SinaStockApiProvider();
            } },
        { "tencent",
            []() {
                return new TencentStockApiProvider();
            } },
        { "eastmoney",
            []() {
                return new EastMoneyStockApiProvider();
            } },
    };
};