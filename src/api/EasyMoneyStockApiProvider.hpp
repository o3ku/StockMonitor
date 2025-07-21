#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "IStockApiProvider.hpp"

class EasyMoneyStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit EasyMoneyStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
    {
    }

    QString name() const override
    {
        return "easymoney";
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
        connect(reply, &QNetworkReply::finished, this, &EasyMoneyStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QString url = QString(
            "http://push2.eastmoney.com/api/qt/ulist.np/"
            "get?fields=f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f20,f21,f23,f24,f25,f22,f33,f11,f62,f128,f136,f115,f152&secids=%1")
                          .arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "http://quote.eastmoney.com");

        QNetworkReply *reply = netmgr_->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        connect(reply, &QNetworkReply::finished, this, &EasyMoneyStockApiProvider::onStockInfosFinished);
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
                if (quotationObj.contains("Data"))
                {
                    QJsonArray dataArray = quotationObj["Data"].toArray();

                    for (const QJsonValue &value : dataArray)
                    {
                        QJsonObject stockObj = value.toObject();
                        QString code = stockObj["Code"].toString();
                        QString name = stockObj["Name"].toString();
                        QString market = stockObj["MktNum"].toString() == "1" ? "SH" : "SZ";
                        
                        // 构建标准格式的代码
                        QString standardCode = market + code;

                        if (!code.isEmpty() && !name.isEmpty())
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
                if (dataObj.contains("diff") && dataObj["diff"].isObject())
                {
                    QJsonObject diffObj = dataObj["diff"].toObject();
                    
                    for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
                    {
                        const QString &originalCode = originalCodes[i];
                        const QString &formattedCode = formattedCodes[i];
                        
                        // 在diff对象中查找对应的股票数据
                        for (const QString &key : diffObj.keys())
                        {
                            QJsonObject stockObj = diffObj[key].toObject();
                            
                            // 检查是否匹配当前股票代码
                            QString secid = stockObj["f12"].toString();
                            if (!secid.isEmpty())
                            {
                                // 提取股票代码并构建标准格式
                                QString market = formattedCode.startsWith("0.") ? "SZ" : "SH";
                                QString code = secid;
                                QString standardCode = market + code;
                                
                                if (standardCode == originalCode || code == originalCode.mid(2))
                                {
                                    StockInfo stockInfo = parseEasyMoneyStockData(originalCode, stockObj);
                                    if (!stockInfo.code().isEmpty())
                                    {
                                        stockInfos.append(stockInfo);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        return stockInfos;
    }

    StockInfo parseEasyMoneyStockData(const QString &code, const QJsonObject &stockObj)
    {
        StockInfo stockInfo;

        stockInfo.setCode(code);
        stockInfo.setName(stockObj["f14"].toString());
        stockInfo.setCurrentPrice(stockObj["f2"].toDouble() / 100.0);  // 价格可能需要除以100
        stockInfo.setChange(stockObj["f4"].toDouble() / 100.0);        // 涨跌额可能需要除以100
        stockInfo.setChangePercent(stockObj["f3"].toDouble() / 100.0); // 涨跌幅可能需要除以100
        stockInfo.setHigh(stockObj["f15"].toDouble() / 100.0);
        stockInfo.setLow(stockObj["f16"].toDouble() / 100.0);
        stockInfo.setOpenPrice(stockObj["f17"].toDouble() / 100.0);
        stockInfo.setYesterday(stockObj["f18"].toDouble() / 100.0);
        stockInfo.setVolume(stockObj["f5"].toVariant().toLongLong());
        stockInfo.setTurnover(stockObj["f6"].toDouble());
        
        // 买卖盘数据可能不存在，设置默认值
        stockInfo.setBid1(stockObj.contains("f19") ? stockObj["f19"].toDouble() / 100.0 : 0.0);
        stockInfo.setAsk1(stockObj.contains("f20") ? stockObj["f20"].toDouble() / 100.0 : 0.0);
        stockInfo.setBid1Volume(stockObj.contains("f21") ? stockObj["f21"].toVariant().toLongLong() * 100 : 0);
        stockInfo.setAsk1Volume(stockObj.contains("f22") ? stockObj["f22"].toVariant().toLongLong() * 100 : 0);

        // 东方财富时间格式处理
        QString timeStr = stockObj.contains("f86") ? stockObj["f86"].toString() : "";
        if (!timeStr.isEmpty())
        {
            QDateTime dateTime = QDateTime::fromString(timeStr, "yyyyMMddHHmmss");
            if (dateTime.isValid())
            {
                stockInfo.setTradeDate(dateTime.toString("yyyy-MM-dd"));
                stockInfo.setTradeTime(dateTime.toString("HH:mm:ss"));
            }
        }
        else
        {
            // 如果没有时间字段，使用当前时间
            QDateTime now = QDateTime::currentDateTime();
            stockInfo.setTradeDate(now.toString("yyyy-MM-dd"));
            stockInfo.setTradeTime(now.toString("HH:mm:ss"));
        }

        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }
};