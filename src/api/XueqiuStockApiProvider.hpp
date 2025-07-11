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

class XueqiuStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit XueqiuStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
        , m_networkManager(new QNetworkAccessManager(this))
        , m_expectedReplies(0)
    {
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("https://xueqiu.com/stock/search.json?code=%1&size=20&key=%1&market=CN").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://xueqiu.com");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &XueqiuStockApiProvider::onSearchFinished);
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

        QString url = QString("https://stock.xueqiu.com/v5/stock/batch/quote.json?symbol=%1&extend=detail").arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://xueqiu.com");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        QNetworkReply *reply = m_networkManager->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        m_pendingReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, &XueqiuStockApiProvider::onStockInfosFinished);
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
        QList<StockSearchResult> results = parseXueqiuSearchData(response);
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
            QList<StockInfo> stockInfos = parseXueqiuBatchStockData(stockCodes, formattedCodes, response);
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

        if (upperCode.length() == 6)
        {
            if (upperCode.startsWith("0") || upperCode.startsWith("3"))
            {
                return QString("SZ%1").arg(upperCode);  // 深圳
            }
            else if (upperCode.startsWith("6"))
            {
                return QString("SH%1").arg(upperCode);  // 上海
            }
        }

        // 如果已经是格式化的代码
        if (upperCode.startsWith("SH") || upperCode.startsWith("SZ"))
        {
            return upperCode;
        }

        return upperCode;
    }

    QList<StockSearchResult> parseXueqiuSearchData(const QString &response)
    {
        QList<StockSearchResult> results;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            return results;
        }

        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("stocks"))
            {
                QJsonArray stocksArray = obj["stocks"].toArray();

                for (const QJsonValue &value : stocksArray)
                {
                    QJsonObject stockObj = value.toObject();
                    QString symbol = stockObj["symbol"].toString();
                    QString name = stockObj["name"].toString();

                    // 从symbol中提取代码和市场
                    QString code, market;
                    if (symbol.startsWith("SH"))
                    {
                        code = symbol.mid(2);
                        market = "SH";
                    }
                    else if (symbol.startsWith("SZ"))
                    {
                        code = symbol.mid(2);
                        market = "SZ";
                    }
                    else
                    {
                        code = symbol;
                        market = "Unknown";
                    }

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

    QList<StockInfo> parseXueqiuBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
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
                if (dataObj.contains("items"))
                {
                    QJsonArray itemsArray = dataObj["items"].toArray();

                    for (int i = 0; i < originalCodes.size(); ++i)
                    {
                        const QString &originalCode = originalCodes[i];
                        const QString &formattedCode = formattedCodes[i];

                        for (const QJsonValue &value : itemsArray)
                        {
                            QJsonObject stockObj = value.toObject();
                            if (stockObj.contains("quote"))
                            {
                                QJsonObject quoteObj = stockObj["quote"].toObject();
                                QString symbol = quoteObj["symbol"].toString();

                                if (symbol == formattedCode)
                                {
                                    StockInfo stockInfo = parseXueqiuStockData(originalCode, quoteObj);
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

    StockInfo parseXueqiuStockData(const QString &code, const QJsonObject &quoteObj)
    {
        StockInfo stockInfo;

        stockInfo.setCode(code);
        stockInfo.setName(quoteObj["name"].toString());
        stockInfo.setCurrentPrice(quoteObj["current"].toDouble());
        stockInfo.setChange(quoteObj["chg"].toDouble());
        stockInfo.setChangePercent(quoteObj["percent"].toDouble());
        stockInfo.setHigh(quoteObj["high"].toDouble());
        stockInfo.setLow(quoteObj["low"].toDouble());
        stockInfo.setOpenPrice(quoteObj["open"].toDouble());
        stockInfo.setYesterday(quoteObj["last_close"].toDouble());
        stockInfo.setVolume(quoteObj["volume"].toVariant().toLongLong());
        stockInfo.setTurnover(quoteObj["amount"].toDouble());

        // 雪球可能没有买卖盘数据，设置为0
        stockInfo.setBid1(0.0);
        stockInfo.setAsk1(0.0);
        stockInfo.setBid1Volume(0);
        stockInfo.setAsk1Volume(0);

        // 时间戳转换
        qint64 timestamp = quoteObj["timestamp"].toVariant().toLongLong();
        if (timestamp > 0)
        {
            QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
            stockInfo.setTradeDate(dateTime.toString("yyyy-MM-dd"));
            stockInfo.setTradeTime(dateTime.toString("HH:mm:ss"));
        }

        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }

    QNetworkAccessManager *m_networkManager;
    QList<QNetworkReply *> m_pendingReplies;
    QStringList m_pendingCodes;
    QList<StockInfo> m_pendingStockInfos;
    int m_expectedReplies;
};
