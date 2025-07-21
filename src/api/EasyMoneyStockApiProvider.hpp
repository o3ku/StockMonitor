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

class EastMoneyStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit EastMoneyStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
        , netmgr_(new QNetworkAccessManager(this))
        , m_expectedReplies(0)
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
        m_pendingCodes = codes;
        m_pendingStockInfos.clear();
        m_expectedReplies = codes.size();

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

        if (reply->error() == QNetworkReply::NoError)
        {
            QString response = QString::fromUtf8(reply->readAll());
            QList<StockInfo> stockInfos = parseEasyMoneyBatchStockData(stockCodes, formattedCodes, response);
            m_pendingStockInfos.append(stockInfos);
        }

        reply->deleteLater();
    }

private:
    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();

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

                        if (!code.isEmpty() && !name.isEmpty())
                        {
                            StockInfo result(code, name);
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
            return stockInfos;
        }

        if (doc.isObject())
        {
            QJsonObject rootObj = doc.object();
            if (rootObj.contains("data") && rootObj["data"].isObject())
            {
                QJsonObject dataObj = rootObj["data"].toObject();
                if (dataObj.contains("diff"))
                {
                    QJsonArray diffArray = dataObj["diff"].toArray();

                    for (int i = 0; i < originalCodes.size(); ++i)
                    {
                        const QString &originalCode = originalCodes[i];

                        for (const QJsonValue &value : diffArray)
                        {
                            QJsonObject stockObj = value.toObject();
                            QString stockCode = stockObj["f12"].toString();

                            if (stockCode == originalCode)
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

        return stockInfos;
    }

    StockInfo parseEasyMoneyStockData(const QString &code, const QJsonObject &stockObj)
    {
        StockInfo stockInfo;

        stockInfo.setCode(code);
        stockInfo.setName(stockObj["f14"].toString());
        stockInfo.setCurrentPrice(stockObj["f2"].toDouble());
        stockInfo.setChange(stockObj["f4"].toDouble());
        stockInfo.setChangePercent(stockObj["f3"].toDouble());
        stockInfo.setHigh(stockObj["f15"].toDouble());
        stockInfo.setLow(stockObj["f16"].toDouble());
        stockInfo.setOpenPrice(stockObj["f17"].toDouble());
        stockInfo.setYesterday(stockObj["f18"].toDouble());
        stockInfo.setVolume(stockObj["f5"].toVariant().toLongLong());
        stockInfo.setTurnover(stockObj["f6"].toDouble());
        stockInfo.setBid1(stockObj["f19"].toDouble());
        stockInfo.setAsk1(stockObj["f20"].toDouble());
        stockInfo.setBid1Volume(stockObj["f21"].toVariant().toLongLong() * 100);
        stockInfo.setAsk1Volume(stockObj["f22"].toVariant().toLongLong() * 100);

        // 东方财富时间格式处理
        QString timeStr = stockObj["f86"].toString();
        if (!timeStr.isEmpty())
        {
            QDateTime dateTime = QDateTime::fromString(timeStr, "yyyyMMddHHmmss");
            if (dateTime.isValid())
            {
                stockInfo.setTradeDate(dateTime.toString("yyyy-MM-dd"));
                stockInfo.setTradeTime(dateTime.toString("HH:mm:ss"));
            }
        }

        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }

    QNetworkAccessManager *netmgr_;
    QStringList m_pendingCodes;
    QList<StockInfo> m_pendingStockInfos;
    int m_expectedReplies;
};
