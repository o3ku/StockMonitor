#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "IStockApiProvider.hpp"

/**
 * 东方财富股票API提供者
 * 
 * API字段说明：
 * f1:市场代码 f2:最新价 f3:涨跌幅 f4:涨跌额 f5:成交量 f6:成交额
 * f7:振幅 f8:换手率 f9:市盈率 f10:量比 f11:涨速
 * f12:股票代码 f13:市场(1:上海, 0:深圳) f14:股票名称 f15:最高 f16:最低 f17:开盘价 f18:昨收
 * f19:买一价 f20:卖一价 f21:买一量 f22:卖一量
 * 
 * 价格单位为分，需要除以100转换为元
 */
class EastMoneyStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit EastMoneyStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
    {
    }

    QString name() const override
    {
        return "eastmoney";
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("http://searchapi.eastmoney.com/api/suggest/get?input=%1&type=14&token=D43BF722C8E33BDC906FB84D85E326E8&markettype=&count=20")
                          .arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "http://quote.eastmoney.com");

        QNetworkReply *reply = netmgr_->get(request);
        connect(reply, &QNetworkReply::finished, this, &EastMoneyStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            if (isUsSymbol(code))
            {
                QString symbol = normalizeUsSymbol(code);
                formattedCodes.append(QString("105.%1").arg(symbol));
                formattedCodes.append(QString("106.%1").arg(symbol));
            }
            else
            {
                QString formattedCode = formatStockCode(code);
                formattedCodes.append(formattedCode);
            }
        }
        formattedCodes.removeDuplicates();

        // 东方财富API字段说明：
        // f1:市场代码 f2:最新价 f3:涨跌幅 f4:涨跌额 f5:成交量 f6:成交额
        // f7:振幅 f8:换手率 f9:市盈率 f10:量比 f11:涨速
        // f12:股票代码 f13:市场 f14:股票名称 f15:最高 f16:最低 f17:开盘价 f18:昨收
        // f20:总市值 f21:流通市值 f22:涨速 f23:市净率 f24:60日涨跌幅 f25:年初至今涨跌幅
        QString url = QString(
            "http://push2.eastmoney.com/api/qt/ulist.np/"
            "get?fields=f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24,f25,f33,f62,f128,f136,f115,f152&secids=%1")
                          .arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "http://quote.eastmoney.com");

        QNetworkReply *reply = netmgr_->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        connect(reply, &QNetworkReply::finished, this, &EastMoneyStockApiProvider::onStockInfosFinished);
    }

private slots:
    void onSearchFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
        {
            return;
        }

        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            emit errorOccurred(QString("Search failed: %1").arg(reply->errorString()));
            return;
        }

        // 使用UTF-8编码读取响应数据
        QString response = QString::fromUtf8(reply->readAll());
        QList<StockInfo> results = parseEasyMoneySearchData(response);
        emit searchCompleted(results);
    }

    void onStockInfosFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;

        QStringList stockCodes = reply->property("stockCodes").toStringList();
        QStringList formattedCodes = reply->property("formattedCodes").toStringList();
        
        QList<StockInfo> stockInfos;
        
        if (reply->error() == QNetworkReply::NoError)
        {
            QString response = QString::fromUtf8(reply->readAll());
            stockInfos = parseEasyMoneyBatchStockData(stockCodes, formattedCodes, response);
            
            // 如果没有获取到数据，记录错误
            if (stockInfos.isEmpty() && !stockCodes.isEmpty())
            {
                emit errorOccurred(QString("EasyMoney API: No stock data returned for codes: %1").arg(stockCodes.join(", ")));
            }
        }
        else
        {
            emit errorOccurred(QString("EasyMoney API Error: %1").arg(reply->errorString()));
        }
        
        reply->deleteLater();
        emit stockInfosReceived(stockInfos);
    }

private:
    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();

        if (upperCode.startsWith("SZ") || upperCode.startsWith("SH"))
        {
            // 已经是带市场前缀的代码，转换为东方财富格式
            if (upperCode.startsWith("SZ"))
            {
                return QString("0.%1").arg(upperCode.mid(2));
            }
            else if (upperCode.startsWith("SH"))
            {
                return QString("1.%1").arg(upperCode.mid(2));
            }
        }
        if (upperCode.startsWith("HK"))
        {
            return QString("116.%1").arg(upperCode.mid(2));
        }

        if (upperCode.length() == 6)
        {
            if (upperCode.startsWith("0") || upperCode.startsWith("3"))
            {
                return QString("0.%1").arg(upperCode);  // 深圳
            }
            else if (upperCode.startsWith("6"))
            {
                return QString("1.%1").arg(upperCode);  // 上海
            }
        }

        // 如果已经是格式化的代码
        if (upperCode.contains("."))
        {
            return upperCode;
        }

        return upperCode;
    }

    QList<StockInfo> parseEasyMoneySearchData(const QString &response)
    {
        QList<StockInfo> results;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            emit errorOccurred(QString("EasyMoney search parse error: %1").arg(error.errorString()));
            return results;
        }

        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("QuotationCodeTable") && obj["QuotationCodeTable"].isObject())
            {
                QJsonObject quotationObj = obj["QuotationCodeTable"].toObject();
                if (quotationObj.contains("Data") && quotationObj["Data"].isArray())
                {
                    QJsonArray dataArray = quotationObj["Data"].toArray();

                    for (const QJsonValue &value : dataArray)
                    {
                        QJsonObject stockObj = value.toObject();
                        QString code = stockObj["Code"].toString();
                        QString name = stockObj["Name"].toString();
                        
                        QString marketPrefix;
                        if (!marketPrefixForNum(stockObj["MktNum"].toString(), marketPrefix)) continue;
                        
                        // Build standard code.
                        QString standardCode = marketPrefix.isEmpty() ? code.toUpper() : marketPrefix + code;

                        if (!code.isEmpty())
                        {
                            StockInfo result(standardCode, name);
                            results.append(result);
                        }
                    }
                }
            }
        }

        return results;
    }

    QList<StockInfo> parseEasyMoneyBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
    {
        QList<StockInfo> stockInfos;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            emit errorOccurred(QString("EasyMoney data parse error: %1").arg(error.errorString()));
            return stockInfos;
        }

        if (doc.isObject())
        {
            QJsonObject rootObj = doc.object();
            if (rootObj.contains("data") && rootObj["data"].isObject())
            {
                QJsonObject dataObj = rootObj["data"].toObject();
                if (dataObj.contains("diff") && dataObj["diff"].isArray())
                {
                    // 实际上diff是一个数组，不是对象
                    QJsonArray diffArray = dataObj["diff"].toArray();
                    
                    // 创建一个映射，用于快速查找股票代码
                    QMap<QString, QString> codeMap;
                    for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
                    {
                        codeMap[formattedCodes[i]] = originalCodes[i];
                    }
                    
                    // 遍历数组中的每个股票数据
                    for (const QJsonValue &value : diffArray)
                    {
                        if (!value.isObject()) continue;
                        
                        QJsonObject stockObj = value.toObject();
                        QString code = stockObj["f12"].toString();
                        int market = stockObj["f13"].toInt();
                        
                        // 构建东方财富格式的代码
                        QString formattedCode = QString("%1.%2").arg(market).arg(code);
                        
                        // 查找对应的原始代码
                        if (codeMap.contains(formattedCode))
                        {
                            QString originalCode = codeMap[formattedCode];
                            StockInfo stockInfo = parseEasyMoneyStockData(originalCode, stockObj);
                            if (!stockInfo.code().isEmpty())
                            {
                                stockInfos.append(stockInfo);
                            }
                        }
                    }
                }
            }
        }

        return stockInfos;
    }

    bool isUsSymbol(const QString &code) const
    {
        if (code.startsWith("US", Qt::CaseInsensitive)) return true;
        if (code.startsWith("SH", Qt::CaseInsensitive) || code.startsWith("SZ", Qt::CaseInsensitive) || code.startsWith("HK", Qt::CaseInsensitive))
        {
            return false;
        }
        for (const QChar ch : code)
        {
            if (ch.isLetter()) return true;
        }
        return false;
    }

    QString normalizeUsSymbol(const QString &code) const
    {
        QString symbol = code.toUpper();
        if (symbol.startsWith("US"))
        {
            symbol = symbol.mid(2);
        }
        return symbol;
    }

    bool marketPrefixForNum(const QString &marketNum, QString &marketPrefix) const
    {
        if (marketNum == "1")
        {
            marketPrefix = "SH";
            return true;
        }
        if (marketNum == "0")
        {
            marketPrefix = "SZ";
            return true;
        }
        if (marketNum == "116")
        {
            marketPrefix = "HK";
            return true;
        }
        if (marketNum == "105" || marketNum == "106")
        {
            marketPrefix = QString();
            return true;
        }
        return false;
    }

    double priceDivisorForMarket(int marketId) const
    {
        return (marketId == 116 || marketId == 105 || marketId == 106) ? 1000.0 : 100.0;
    }

    StockInfo parseEasyMoneyStockData(const QString &code, const QJsonObject &stockObj)
    {
        StockInfo stockInfo;

        stockInfo.setCode(code);
        stockInfo.setName(stockObj["f14"].toString());

        int marketId = stockObj["f13"].toInt();
        double priceDivisor = priceDivisorForMarket(marketId);

        double currentPrice = stockObj["f2"].toDouble();
        if (currentPrice > 0)
        {
            currentPrice = currentPrice / priceDivisor;
        }
        stockInfo.setCurrentPrice(currentPrice);

        double change = stockObj["f4"].toDouble() / priceDivisor;
        double changePercent = stockObj["f3"].toDouble() / 100.0;
        stockInfo.setChange(change);
        stockInfo.setChangePercent(changePercent);

        stockInfo.setHigh(stockObj["f15"].toDouble() / priceDivisor);
        stockInfo.setLow(stockObj["f16"].toDouble() / priceDivisor);
        stockInfo.setOpenPrice(stockObj["f17"].toDouble() / priceDivisor);
        stockInfo.setYesterday(stockObj["f18"].toDouble() / priceDivisor);
        
        // 成交量和成交额
        stockInfo.setVolume(stockObj["f5"].toVariant().toLongLong());
        stockInfo.setTurnover(stockObj["f6"].toDouble());
        
        // 买卖盘数据
        if (stockObj.contains("f19")) {
            stockInfo.setBid1(stockObj["f19"].toDouble() / priceDivisor);
        }
        
        if (stockObj.contains("f20")) {
            stockInfo.setAsk1(stockObj["f20"].toDouble() / priceDivisor);
        }
        
        // 买卖盘量（单位：股，需要乘以100转换为股）
        if (stockObj.contains("f21")) {
            stockInfo.setBid1Volume(stockObj["f21"].toVariant().toLongLong() * 100);
        }
        
        if (stockObj.contains("f22")) {
            stockInfo.setAsk1Volume(stockObj["f22"].toVariant().toLongLong() * 100);
        }

        // 东方财富API没有直接返回时间字段，使用当前时间
        QDateTime now = QDateTime::currentDateTime();
        stockInfo.setTradeDate(now.toString("yyyy-MM-dd"));
        stockInfo.setTradeTime(now.toString("HH:mm:ss"));
        stockInfo.setLastUpdate(now);

        return stockInfo;
    }
};
