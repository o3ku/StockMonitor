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

class TencentStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit TencentStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
        , m_networkManager(new QNetworkAccessManager(this))
        , m_expectedReplies(0)
    {
    }

    void searchStocksAsync(const QString &keyword) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("http://smartbox.gtimg.cn/s3/?q=%1&t=gp").arg(encodedKeyword);

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://gu.qq.com");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &TencentStockApiProvider::onSearchFinished);
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

        QString url = QString("http://qt.gtimg.cn/q=%1").arg(formattedCodes.join(","));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://gu.qq.com");

        QNetworkReply *reply = m_networkManager->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        m_pendingReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, &TencentStockApiProvider::onStockInfosFinished);
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
        QList<StockSearchResult> results = parseTencentSearchData(response);
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
            QList<StockInfo> stockInfos = parseTencentBatchStockData(stockCodes, formattedCodes, response);
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
            return upperCode.toLower();
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

    QList<StockSearchResult> parseTencentSearchData(const QString &response)
    {
        QList<StockSearchResult> results;

        // 腾讯搜索返回格式: v_hint="sh000001~上证指数~000001~1~SZZS";
        QRegularExpression pattern("\"([^\"]+)\"");
        QRegularExpressionMatch match = pattern.match(response);

        if (match.hasMatch())
        {
            QString data = match.captured(1);
            QStringList stockEntries = data.split(';', Qt::SkipEmptyParts);

            for (const QString &entry : stockEntries)
            {
                QStringList fields = entry.split('~');
                if (fields.size() >= 4)
                {
                    QString fullCode = fields[0];
                    QString name = fields[1];
                    QString code = fields[2];
                    QString type = fields[3];

                    if (!code.isEmpty() && !name.isEmpty())
                    {
                        QString market = fullCode.startsWith("sh") ? "SH" : "SZ";
                        StockSearchResult result(code, name, market, type);
                        results.append(result);
                    }
                }
            }
        }

        return results;
    }

    QList<StockInfo> parseTencentBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
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
                if (line.contains(QString("v_%1=").arg(formattedCode)))
                {
                    stockLine = line;
                    break;
                }
            }

            if (!stockLine.isEmpty())
            {
                StockInfo stockInfo = parseTencentStockData(originalCode, stockLine);
                if (!stockInfo.code().isEmpty())
                {
                    stockInfos.append(stockInfo);
                }
            }
        }

        return stockInfos;
    }

    StockInfo parseTencentStockData(const QString &code, const QString &response)
    {
        StockInfo stockInfo;

        // 腾讯数据格式: v_sh000001="1~上证指数~000001~3154.32~-9.84~-0.31~158826~79513~79313~0.00~...";
        QRegularExpression pattern("\"([^\"]+)\"");
        QRegularExpressionMatch match = pattern.match(response);

        if (match.hasMatch())
        {
            QString data = match.captured(1);
            QStringList fields = data.split('~');

            if (fields.size() >= 50)
            {
                stockInfo.setCode(code);
                stockInfo.setName(fields[1]);
                stockInfo.setCurrentPrice(fields[3].toDouble());
                stockInfo.setChange(fields[4].toDouble());
                stockInfo.setChangePercent(fields[5].toDouble());
                stockInfo.setVolume(fields[6].toLongLong());
                stockInfo.setTurnover(fields[7].toDouble());
                stockInfo.setOpenPrice(fields[5].toDouble());
                stockInfo.setYesterday(
                    fields[4].toDouble() > 0 ? fields[3].toDouble() - fields[4].toDouble() : fields[3].toDouble() + qAbs(fields[4].toDouble()));
                stockInfo.setHigh(fields[33].toDouble());
                stockInfo.setLow(fields[34].toDouble());
                stockInfo.setBid1(fields[9].toDouble());
                stockInfo.setAsk1(fields[19].toDouble());
                stockInfo.setBid1Volume(fields[10].toLongLong() * 100);
                stockInfo.setAsk1Volume(fields[20].toLongLong() * 100);
                stockInfo.setTradeDate(fields[30]);
                stockInfo.setTradeTime(fields[31]);
                stockInfo.setLastUpdate(QDateTime::currentDateTime());
            }
        }

        return stockInfo;
    }

    QNetworkAccessManager *m_networkManager;
    QList<QNetworkReply *> m_pendingReplies;
    QStringList m_pendingCodes;
    QList<StockInfo> m_pendingStockInfos;
    int m_expectedReplies;
};
