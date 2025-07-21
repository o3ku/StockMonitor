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
    {
        // 雪球API需要先获取cookie
        initCookies();
    }

    QString name() const override
    {
        return "xueqiu";
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("https://xueqiu.com/query/v1/search/stock.json?q=%1&size=20&page=1").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://xueqiu.com");
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        
        // 添加cookie
        QMapIterator<QByteArray, QByteArray> it(cookies_);
        while (it.hasNext()) {
            it.next();
            request.setRawHeader(it.key(), it.value());
        }

        QNetworkReply *reply = netmgr_->get(request);
        connect(reply, &QNetworkReply::finished, this, &XueqiuStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
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
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        
        // 添加cookie
        QMapIterator<QByteArray, QByteArray> it(cookies_);
        while (it.hasNext()) {
            it.next();
            request.setRawHeader(it.key(), it.value());
        }

        QNetworkReply *reply = netmgr_->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
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

        // 保存cookie
        saveCookies(reply);

        QString response = QString::fromUtf8(reply->readAll());
        QList<StockInfo> results = parseXueqiuSearchData(response);
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
            // 保存cookie
            saveCookies(reply);
            
            QString response = QString::fromUtf8(reply->readAll());
            stockInfos = parseXueqiuBatchStockData(stockCodes, formattedCodes, response);
        }
        else
        {
            emit errorOccurred(QString("Xueqiu API Error: %1").arg(reply->errorString()));
            
            // 如果是认证错误，尝试重新获取cookie
            if (reply->error() == QNetworkReply::AuthenticationRequiredError ||
                reply->error() == QNetworkReply::ContentAccessDenied ||
                reply->error() == QNetworkReply::ContentOperationNotPermittedError) {
                initCookies();
            }
        }
        
        reply->deleteLater();
        emit stockInfosReceived(stockInfos);
    }
    
    void onInitCookiesFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;
        
        saveCookies(reply);
        reply->deleteLater();
    }

private:
    void initCookies()
    {
        // 访问雪球首页获取cookie
        QNetworkRequest request;
        request.setUrl(QUrl("https://xueqiu.com"));
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        
        QNetworkReply *reply = netmgr_->get(request);
        connect(reply, &QNetworkReply::finished, this, &XueqiuStockApiProvider::onInitCookiesFinished);
    }
    
    void saveCookies(QNetworkReply *reply)
    {
        QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();
        for (const auto& header : headers) {
            if (QString(header.first).toLower() == "set-cookie") {
                QByteArray cookieValue = header.second;
                int endPos = cookieValue.indexOf(';');
                if (endPos > 0) {
                    QByteArray cookiePair = cookieValue.left(endPos);
                    int eqPos = cookiePair.indexOf('=');
                    if (eqPos > 0) {
                        QByteArray cookieName = cookiePair.left(eqPos);
                        QByteArray cookieVal = cookiePair.mid(eqPos + 1);
                        cookies_[cookieName] = cookieVal;
                    }
                }
            }
        }
    }

    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();

        if (upperCode.startsWith("SZ") || upperCode.startsWith("SH"))
        {
            // 已经是带市场前缀的代码，转换为雪球格式
            if (upperCode.startsWith("SZ"))
            {
                return QString("SZ%1").arg(upperCode.mid(2));
            }
            else if (upperCode.startsWith("SH"))
            {
                return QString("SH%1").arg(upperCode.mid(2));
            }
        }

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

        return upperCode;
    }

    QList<StockInfo> parseXueqiuSearchData(const QString &response)
    {
        QList<StockInfo> results;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError)
        {
            emit errorOccurred(QString("Xueqiu search parse error: %1").arg(error.errorString()));
            return results;
        }

        if (doc.isObject())
        {
            QJsonObject obj = doc.object();
            if (obj.contains("data") && obj["data"].isObject())
            {
                QJsonObject dataObj = obj["data"].toObject();
                if (dataObj.contains("stocks") && dataObj["stocks"].isArray())
                {
                    QJsonArray stocksArray = dataObj["stocks"].toArray();

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
                            QString standardCode = market + code;
                            StockInfo result(standardCode, name);
                            results.append(result);
                        }
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
            emit errorOccurred(QString("Xueqiu data parse error: %1").arg(error.errorString()));
            return stockInfos;
        }

        if (doc.isObject())
        {
            QJsonObject rootObj = doc.object();
            if (rootObj.contains("data") && rootObj["data"].isObject())
            {
                QJsonObject dataObj = rootObj["data"].toObject();
                if (dataObj.contains("items") && dataObj["items"].isArray())
                {
                    QJsonArray itemsArray = dataObj["items"].toArray();

                    for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
                    {
                        const QString &originalCode = originalCodes[i];
                        const QString &formattedCode = formattedCodes[i];

                        for (const QJsonValue &value : itemsArray)
                        {
                            QJsonObject stockObj = value.toObject();
                            if (stockObj.contains("quote") && stockObj["quote"].isObject())
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
        stockInfo.setChangePercent(quoteObj["percent"].toDouble() * 100); // 雪球返回的是小数，需要转换为百分比
        stockInfo.setHigh(quoteObj["high"].toDouble());
        stockInfo.setLow(quoteObj["low"].toDouble());
        stockInfo.setOpenPrice(quoteObj["open"].toDouble());
        stockInfo.setYesterday(quoteObj["last_close"].toDouble());
        stockInfo.setVolume(quoteObj["volume"].toVariant().toLongLong());
        stockInfo.setTurnover(quoteObj["amount"].toDouble());

        // 雪球可能没有买卖盘数据，设置为0或从其他字段获取
        if (quoteObj.contains("bid") && quoteObj.contains("ask")) {
            stockInfo.setBid1(quoteObj["bid"].toDouble());
            stockInfo.setAsk1(quoteObj["ask"].toDouble());
            
            // 尝试获取买卖盘量
            if (quoteObj.contains("bid_volume") && quoteObj.contains("ask_volume")) {
                stockInfo.setBid1Volume(quoteObj["bid_volume"].toVariant().toLongLong());
                stockInfo.setAsk1Volume(quoteObj["ask_volume"].toVariant().toLongLong());
            } else {
                stockInfo.setBid1Volume(0);
                stockInfo.setAsk1Volume(0);
            }
        } else {
            stockInfo.setBid1(0.0);
            stockInfo.setAsk1(0.0);
            stockInfo.setBid1Volume(0);
            stockInfo.setAsk1Volume(0);
        }

        // 时间戳转换
        qint64 timestamp = quoteObj["timestamp"].toVariant().toLongLong();
        if (timestamp > 0)
        {
            QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(timestamp);
            stockInfo.setTradeDate(dateTime.toString("yyyy-MM-dd"));
            stockInfo.setTradeTime(dateTime.toString("HH:mm:ss"));
        }
        else
        {
            QDateTime now = QDateTime::currentDateTime();
            stockInfo.setTradeDate(now.toString("yyyy-MM-dd"));
            stockInfo.setTradeTime(now.toString("HH:mm:ss"));
        }

        stockInfo.setLastUpdate(QDateTime::currentDateTime());

        return stockInfo;
    }
    
private:
    QMap<QByteArray, QByteArray> cookies_;
};