#pragma once

#include "api/EasyMoneyStockApiProvider.hpp"
#include "api/IStockApiProvider.hpp"
#include "api/NeteaseStockApiProvider.hpp"
#include "api/SinaStockApiProvider.hpp"
#include "api/TencentStockApiProvider.hpp"
#include "api/XueqiuStockApiProvider.hpp"
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

    IStockApiProvider *get(const QString &src)
    {
        Q_ASSERT(apis_.contains(src));
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
        { "netease",
            []() {
                return new NeteaseStockApiProvider();
            } },
        { "easymoney",
            []() {
                return new EasyMoneyStockApiProvider();
            } },
        { "xueqiu",
            []() {
                return new XueqiuStockApiProvider();
            } },
    };
};