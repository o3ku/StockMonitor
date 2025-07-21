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
    {
    }

    QString name() const override
    {
        return "netease";
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("https://quotes.money.163.com/stocksearch/json.do?count=20&type=&word=%1").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://quotes.money.163.com");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

        QNetworkReply *reply = netmgr_->get(request);
        connect(reply, &QNetworkReply::finished, this, &NeteaseStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QString url = QString("https://api.money.126.net/data/feed/%1,money.api").arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://quotes.money.163.com");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

        QNetworkReply *reply = netmgr_->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
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
        QList<StockInfo> results = parseNeteaseSearchData(response);
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
            stockInfos = parseNeteaseBatchStockData(stockCodes, formattedCodes, response);
        }
        else
        {
            emit errorOccurred(QString("Netease API Error: %1").arg(reply->errorString()));
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

    QList<StockInfo> parseNeteaseSearchData(const QString &response)
    {
        QList<StockInfo> results;

        // 网易搜索返回格式是JSONP格式，需要先提取JSON部分
        QRegularExpression jsonpPattern("^[^\\(]*\\((.*)\\)[^\\)]*$");
        QRegularExpressionMatch jsonpMatch = jsonpPattern.match(response);
        
        QString jsonStr;
        if (jsonpMatch.hasMatch()) {
            jsonStr = jsonpMatch.captured(1);
        } else {
            jsonStr = response; // 尝试直接解析
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            emit errorOccurred(QString("Netease search parse error: %1").arg(error.errorString()));
            return results;
        }

        if (doc.isArray())
        {
            QJsonArray resultArray = doc.array();

            for (const QJsonValue &value : resultArray)
            {
                QJsonObject stockObj = value.toObject();
                QString code = stockObj["code"].toString();
                QString name = stockObj["name"].toString();
                QString market = stockObj["type"].toString();
                
                // 构建标准格式的代码
                QString standardCode;
                if (market == "sh") {
                    standardCode = "SH" + code;
                } else if (market == "sz") {
                    standardCode = "SZ" + code;
                } else {
                    standardCode = code;
                }

                if (!code.isEmpty() && !name.isEmpty())
                {
                    StockInfo result(standardCode, name);
                    results.append(result);
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
            emit errorOccurred(QString("Netease data parse error: %1").arg(error.errorString()));
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
        stockInfo.setChangePercent(stockObj["percent"].toDouble() * 100); // 网易返回的是小数，需要转换为百分比
        stockInfo.setHigh(stockObj["high"].toDouble());
        stockInfo.setLow(stockObj["low"].toDouble());
        stockInfo.setOpenPrice(stockObj["open"].toDouble());
        stockInfo.setYesterday(stockObj["yestclose"].toDouble());
        stockInfo.setVolume(stockObj["volume"].toVariant().toLongLong());
        stockInfo.setTurnover(stockObj["turnover"].toDouble());
        
        // 买卖盘数据
        if (stockObj.contains("bid1")) {
            stockInfo.setBid1(stockObj["bid1"].toDouble());
            stockInfo.setAsk1(stockObj["ask1"].toDouble());
            stockInfo.setBid1Volume(stockObj["bidvol1"].toVariant().toLongLong() * 100);
            stockInfo.setAsk1Volume(stockObj["askvol1"].toVariant().toLongLong() * 100);
        } else {
            stockInfo.setBid1(0.0);
            stockInfo.setAsk1(0.0);
            stockInfo.setBid1Volume(0);
            stockInfo.setAsk1Volume(0);
        }
        
        // 日期和时间
        if (stockObj.contains("time")) {
            QString timeStr = stockObj["time"].toString();
            QStringList timeParts = timeStr.split(" ");
            if (timeParts.size() >= 2) {
                stockInfo.setTradeDate(timeParts[0]);
                stockInfo.setTradeTime(timeParts[1]);
            } else {
                QDateTime now = QDateTime::currentDateTime();
                stockInfo.setTradeDate(now.toString("yyyy-MM-dd"));
                stockInfo.setTradeTime(now.toString("HH:mm:ss"));
            }
        } else {
            QDateTime now = QDateTime::currentDateTime();
            stockInfo.setTradeDate(now.toString("yyyy-MM-dd"));
            stockInfo.setTradeTime(now.toString("HH:mm:ss"));
        }
        
        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }
};