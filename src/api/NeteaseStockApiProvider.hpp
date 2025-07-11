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

class NeteaseStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit NeteaseStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
        , m_networkManager(new QNetworkAccessManager(this))
        , m_expectedReplies(0)
    {
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("http://quotes.money.163.com/stocksearch/json.do?count=20&type=&word=%1").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "http://quotes.money.163.com");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &NeteaseStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
        m_pendingCodes = codes;
        m_pendingStockInfos.clear();
        m_pendingReplies.clear();
        m_expectedReplies = codes.size();

        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QString url = QString("http://api.money.126.net/data/feed/%1").arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "http://quotes.money.163.com");

        QNetworkReply *reply = m_networkManager->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        m_pendingReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, &NeteaseStockApiProvider::onStockInfosFinished);
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
        QList<StockSearchResult> results = parseNeteaseSearchData(response);
        emit searchCompleted(results);
    }

    void onStockInfosFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;

        QStringList stockCodes = reply->property("stockCodes").toStringList();
        QStringList formattedCodes = reply->property("formattedCodes").toStringList();
        m_pendingReplies.removeOne(reply);

        if (reply->error() == QNetworkReply::NoError)
        {
            QString response = QString::fromUtf8(reply->readAll());
            QList<StockInfo> stockInfos = parseNeteaseBatchStockData(stockCodes, formattedCodes, response);
            m_pendingStockInfos.append(stockInfos);
        }

        reply->deleteLater();

        if (m_pendingReplies.isEmpty())
        {
            emit stockInfosReceived(m_pendingStockInfos);
            m_pendingStockInfos.clear();
        }
    }

private:
    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();
        if (upperCode.startsWith("SZ") || upperCode.startsWith("SH"))
        {
            return upperCode;
        }

        if (upperCode.length() == 6)
        {
            if (upperCode.startsWith("0") || upperCode.startsWith("3"))
            {
                return QString("SZ%1").arg(upperCode);
            }
            else if (upperCode.startsWith("6"))
            {
                return QString("SH%1").arg(upperCode);
            }
        }

        return upperCode;
    }

    QList<StockSearchResult> parseNeteaseSearchData(const QString &response)
    {
        QList<StockSearchResult> results;

        // 网易搜索返回JSON格式
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            return results;
        }

        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("result"))
            {
                QJsonArray resultArray = obj["result"].toArray();

                for (const QJsonValue &value : resultArray)
                {
                    QJsonObject stockObj = value.toObject();
                    QString code = stockObj["code"].toString();
                    QString name = stockObj["name"].toString();
                    QString market = stockObj["type"].toString();

                    if (!code.isEmpty() && !name.isEmpty())
                    {
                        StockSearchResult result(code, name, market, "stock");
                        results.append(result);
                    }
                }
            }
        }

        return results;
    }

    QList<StockInfo> parseNeteaseBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
    {
        QList<StockInfo> stockInfos;

        // 网易数据格式: _ntes_quote_callback({"SH000001": {...}});
        QString jsonData = response;

        // 移除JSONP包装
        QRegularExpression jsonpPattern("_ntes_quote_callback\\((.*)\\);?");
        QRegularExpressionMatch match = jsonpPattern.match(jsonData);
        if (match.hasMatch())
        {
            jsonData = match.captured(1);
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            return stockInfos;
        }

        if (doc.isObject())
        {
            QJsonObject rootObj = doc.object();

            for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
            {
                const QString &originalCode = originalCodes[i];
                const QString &formattedCode = formattedCodes[i];

                if (rootObj.contains(formattedCode))
                {
                    QJsonObject stockObj = rootObj[formattedCode].toObject();
                    StockInfo stockInfo = parseNeteaseStockData(originalCode, stockObj);
                    if (!stockInfo.code().isEmpty())
                    {
                        stockInfos.append(stockInfo);
                    }
                }
            }
        }

        return stockInfos;
    }

    StockInfo parseNeteaseStockData(const QString &code, const QJsonObject &stockObj)
    {
        StockInfo stockInfo;

        stockInfo.setCode(code);
        stockInfo.setName(stockObj["name"].toString());
        stockInfo.setCurrentPrice(stockObj["price"].toDouble());
        stockInfo.setChange(stockObj["updown"].toDouble());
        stockInfo.setChangePercent(stockObj["percent"].toDouble());
        stockInfo.setHigh(stockObj["high"].toDouble());
        stockInfo.setLow(stockObj["low"].toDouble());
        stockInfo.setOpenPrice(stockObj["open"].toDouble());
        stockInfo.setYesterday(stockObj["yestclose"].toDouble());
        stockInfo.setVolume(stockObj["volume"].toVariant().toLongLong());
        stockInfo.setTurnover(stockObj["turnover"].toDouble());
        stockInfo.setBid1(stockObj["bid1"].toDouble());
        stockInfo.setAsk1(stockObj["ask1"].toDouble());
        stockInfo.setBid1Volume(stockObj["bidvol1"].toVariant().toLongLong() * 100);
        stockInfo.setAsk1Volume(stockObj["askvol1"].toVariant().toLongLong() * 100);
        stockInfo.setTradeDate(stockObj["time"].toString().split(" ")[0]);
        stockInfo.setTradeTime(stockObj["time"].toString().split(" ")[1]);
        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }

    QNetworkAccessManager *m_networkManager;
    QList<QNetworkReply *> m_pendingReplies;
    QStringList m_pendingCodes;
    QList<StockInfo> m_pendingStockInfos;
    int m_expectedReplies;
};
