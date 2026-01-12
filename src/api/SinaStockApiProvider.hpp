#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "IStockApiProvider.hpp"

class SinaStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit SinaStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
    {
    }

    QString name() const override
    {
        return "sina";
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("http://suggest3.sinajs.cn/suggest/type=2&key=%1").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://finance.sina.com.cn");

        QNetworkReply *reply = netmgr_->get(request);
        connect(reply, &QNetworkReply::finished, this, &SinaStockApiProvider::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {

        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QString url = QString("http://hq.sinajs.cn/list=%1").arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://finance.sina.com.cn");

        QNetworkReply *reply = netmgr_->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        connect(reply, &QNetworkReply::finished, this, &SinaStockApiProvider::onStockInfosFinished);
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

        QString response = QString::fromLocal8Bit(reply->readAll());
        QList<StockInfo> results = parseSinaSearchData(response);
        emit searchCompleted(results);
    }

    void onStockInfosFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;

        if (reply->error() != QNetworkReply::NoError)
        {
            emit errorOccurred(QString("Stock info failed: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        QStringList stockCodes = reply->property("stockCodes").toStringList();
        QStringList formattedCodes = reply->property("formattedCodes").toStringList();

        QString response = QString::fromLocal8Bit(reply->readAll());
        QList<StockInfo> stockInfos = parseSinaBatchStockData(stockCodes, formattedCodes, response);
        emit stockInfosReceived(stockInfos);

        reply->deleteLater();
    }

private:
    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();
        if (upperCode.startsWith("SZ") || upperCode.startsWith("SH"))
        {
            return upperCode.toLower();
        }
        if (upperCode.startsWith("HK"))
        {
            return QString("hk%1").arg(upperCode.mid(2));
        }
        if (upperCode.startsWith("US"))
        {
            return QString("gb_%1").arg(upperCode.mid(2).toLower());
        }
        if (isUsSymbol(upperCode))
        {
            return QString("gb_%1").arg(upperCode.toLower());
        }

        if (upperCode.length() == 6)
        {
            if (upperCode.startsWith("0") || upperCode.startsWith("3"))
            {
                return QString("sz%1").arg(upperCode);
            }
            else if (upperCode.startsWith("6"))
            {
                return QString("sh%1").arg(upperCode);
            }
        }

        return upperCode.toLower();
    }

    QList<StockInfo> parseSinaSearchData(const QString &response)
    {
        QList<StockInfo> results;

        QRegularExpression pattern("var suggestvalue=\"([^\"]+)\"");
        QRegularExpressionMatch match = pattern.match(response);

        if (match.hasMatch())
        {
            QString suggestData = match.captured(1);
            QStringList stockEntries = suggestData.split(';', QString::SkipEmptyParts);

            for (const QString &entry : stockEntries)
            {
                QStringList fields = entry.split(',');
                if (fields.size() >= 8)
                {
                    QString code = normalizeSinaCode(fields[3]);
                    QString name = fields[4];
                    QString market = fields[1];
                    QString type = fields[0];

                    if (!code.isEmpty() && !name.isEmpty())
                    {
                        StockInfo result(code, name);
                        results.append(result);
                    }
                }
            }
        }

        return results;
    }

    bool isUsSymbol(const QString &upperCode) const
    {
        for (const QChar ch : upperCode)
        {
            if (ch.isLetter()) return true;
        }
        return false;
    }

    QString normalizeSinaCode(const QString &rawCode) const
    {
        QString code = rawCode.trimmed();
        if (code.startsWith("hk", Qt::CaseInsensitive))
        {
            return QString("HK%1").arg(code.mid(2));
        }
        if (code.startsWith("us", Qt::CaseInsensitive))
        {
            return code.mid(2).toUpper();
        }
        if (code.startsWith("gb_", Qt::CaseInsensitive))
        {
            return code.mid(3).toUpper();
        }
        return code.toUpper();
    }

    QList<StockInfo> parseSinaBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
    {
        QList<StockInfo> stockInfos;
        QStringList lines = response.split("\n", Qt::SkipEmptyParts);

        for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
        {
            const QString &originalCode = originalCodes[i];
            const QString &formattedCode = formattedCodes[i];

            QString stockLine;
            for (const QString &line : lines)
            {
                if (line.contains(QString("hq_str_%1").arg(formattedCode)))
                {
                    stockLine = line;
                    break;
                }
            }

            if (!stockLine.isEmpty())
            {
                StockInfo stockInfo = parseSinaStockData(originalCode, stockLine);
                if (!stockInfo.code().isEmpty())
                {
                    stockInfos.append(stockInfo);
                }
            }
        }

        return stockInfos;
    }

    StockInfo parseSinaStockData(const QString &code, const QString &response)
    {
        StockInfo stockInfo;

        QRegularExpression pattern(QString("var hq_str_%1=\"([^\"]+)\"").arg(formatStockCode(code)));
        QRegularExpressionMatch match = pattern.match(response);

        if (match.hasMatch())
        {
            QString data = match.captured(1);
            QStringList fields = data.split(',');

            if (formatStockCode(code).startsWith("hk") && fields.size() >= 19)
            {
                stockInfo.setCode(code);
                stockInfo.setName(fields[1]);
                stockInfo.setOpenPrice(fields[2].toDouble());
                stockInfo.setYesterday(fields[3].toDouble());
                stockInfo.setHigh(fields[4].toDouble());
                stockInfo.setLow(fields[5].toDouble());
                stockInfo.setCurrentPrice(fields[6].toDouble());
                stockInfo.setChange(fields[7].toDouble());
                stockInfo.setChangePercent(fields[8].toDouble());
                stockInfo.setBid1(fields[9].toDouble());
                stockInfo.setAsk1(fields[10].toDouble());
                stockInfo.setTurnover(fields[11].toDouble());
                stockInfo.setVolume(fields[12].toLongLong());
                stockInfo.setTradeDate(fields[17]);
                stockInfo.setTradeTime(fields[18]);
                stockInfo.setLastUpdate(QDateTime::currentDateTime());
            }
            else if (fields.size() >= 32)
            {
                stockInfo.setCode(code);
                stockInfo.setName(fields[0]);
                stockInfo.setOpenPrice(fields[1].toDouble());
                stockInfo.setYesterday(fields[2].toDouble());
                stockInfo.setCurrentPrice(fields[3].toDouble());
                stockInfo.setHigh(fields[4].toDouble());
                stockInfo.setLow(fields[5].toDouble());
                stockInfo.setBid1(fields[6].toDouble());
                stockInfo.setAsk1(fields[7].toDouble());
                stockInfo.setVolume(fields[8].toLongLong());
                stockInfo.setTurnover(fields[9].toDouble());
                stockInfo.setBid1Volume(fields[10].toLongLong());
                stockInfo.setAsk1Volume(fields[20].toLongLong());
                stockInfo.setTradeDate(fields[30]);
                stockInfo.setTradeTime(fields[31]);

                if (stockInfo.yesterday() > 0 && stockInfo.currentPrice())
                {
                    double change = stockInfo.currentPrice() - stockInfo.yesterday();
                    double changePercent = stockInfo.yesterday() > 0 ? (change / stockInfo.yesterday()) * 100 : 0;
                    stockInfo.setChange(change);
                    stockInfo.setChangePercent(changePercent);
                }
                stockInfo.setLastUpdate(QDateTime::currentDateTime());
            }
        }
        return stockInfo;
    }
};
