#ifndef STOCKAPISERVICE_HPP
#define STOCKAPISERVICE_HPP

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QList>
#include <memory>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include "StockInfo.hpp"
#include "StockSearchResult.hpp"
#include "Logger.hpp"

class StockApiService : public QObject
{
    Q_OBJECT

public:
    static StockApiService &instance()
    {
        static StockApiService instance;
        return instance;
    }

    void searchStocksAsync(const QString &keyword)
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QString url = QString("http://suggest3.sinajs.cn/suggest/type=2&key=%1").arg(encodedKeyword);

        Logger::info(QString("Starting search for keyword: '%1', URL: %2").arg(keyword, url));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        request.setRawHeader("Accept-Encoding", "gzip, deflate");
        request.setRawHeader("Connection", "keep-alive");
        request.setRawHeader("Referer", "https://finance.sina.com.cn");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &StockApiService::onSearchFinished);
    }

    void getStockInfosAsync(const QStringList &codes)
    {
        Logger::info(QString("getStockInfosAsync called with %1 codes: %2").arg(codes.size()).arg(codes.join(", ")));

        m_pendingCodes = codes;
        m_pendingStockInfos.clear();
        m_pendingReplies.clear();
        m_expectedReplies = codes.size();

        // Use Sina API with HTTP - simple and reliable
        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QString url = QString("http://hq.sinajs.cn/list=%1").arg(formattedCodes.join(","));
        Logger::info(QString("Requesting Sina API: %1").arg(url));
        Logger::info(QString("Original codes: %1").arg(codes.join(", ")));
        Logger::info(QString("Formatted codes: %1").arg(formattedCodes.join(", ")));

        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Referer", "https://finance.sina.com.cn");

        QNetworkReply *reply = m_networkManager->get(request);
        reply->setProperty("stockCodes", codes);
        reply->setProperty("formattedCodes", formattedCodes);
        m_pendingReplies.append(reply);
        connect(reply, &QNetworkReply::finished, this, &StockApiService::onStockInfosFinished);
    }

signals:
    void searchCompleted(const QList<StockSearchResult> &results);
    void stockInfoReceived(const StockInfo &stockInfo);
    void stockInfosReceived(const QList<StockInfo> &stockInfos);
    void errorOccurred(const QString &error);

private slots:
    void onSearchFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
        {
            Logger::error("Search finished but reply is null");
            return;
        }

        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            Logger::error(QString("Search failed with error: %1").arg(reply->errorString()));
            emit errorOccurred(QString("Search failed: %1").arg(reply->errorString()));
            return;
        }

        QString response = QString::fromLocal8Bit(reply->readAll());
        Logger::info(QString("Search response received, length: %1").arg(response.length()));
        Logger::info(QString("Response content: %1").arg(response.left(200))); // Log first 200 chars

        QList<StockSearchResult> results = parseSinaSearchData(response);
        Logger::info(QString("Parsed %1 search results").arg(results.size()));
        emit searchCompleted(results);
    }

    void onStockInfoFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
            return;

        QString code = reply->property("stockCode").toString();
        QString formattedCode = reply->property("formattedCode").toString();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            QString errorMsg = QString("Failed to get stock info for %1: %2").arg(code, reply->errorString());
            Logger::error(errorMsg);
            emit errorOccurred(errorMsg);
            return;
        }

        QString response = QString::fromLocal8Bit(reply->readAll());
        Logger::info(QString("Received response for %1: %2").arg(formattedCode, response.left(100)));

        StockInfo stockInfo = parseSinaStockData(code, response);

        if (!stockInfo.code().isEmpty())
        {
            Logger::info(QString("Successfully parsed stock info for %1").arg(code));
            emit stockInfoReceived(stockInfo);
        }
        else
        {
            Logger::info(QString("Failed to parse stock data for %1").arg(code));
            emit errorOccurred(QString("Failed to parse stock data for %1").arg(code));
        }
    }

    void onStockInfosFinished()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply)
            return;

        QStringList stockCodes = reply->property("stockCodes").toStringList();
        QStringList formattedCodes = reply->property("formattedCodes").toStringList();
        m_pendingReplies.removeOne(reply);

        if (reply->error() == QNetworkReply::NoError)
        {
            QString response = QString::fromLocal8Bit(reply->readAll());
            Logger::info(QString("Sina API Response: %1").arg(response.left(200)));

            QList<StockInfo> stockInfos = parseSinaBatchStockData(stockCodes, formattedCodes, response);
            m_pendingStockInfos.append(stockInfos);
        }
        else
        {
            Logger::error(QString("Sina API error: %1").arg(reply->errorString()));
        }

        reply->deleteLater();

        if (m_pendingReplies.isEmpty())
        {
            Logger::info(QString("Emitting stockInfosReceived with %1 stocks").arg(m_pendingStockInfos.size()));
            emit stockInfosReceived(m_pendingStockInfos);
            m_pendingStockInfos.clear();
            m_pendingCodes.clear();
            m_expectedReplies = 0;
        }
    }

private:
    explicit StockApiService(QObject *parent = nullptr)
        : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_expectedReplies(0)
    {
    }

    ~StockApiService() = default;

    StockApiService(const StockApiService &) = delete;
    StockApiService &operator=(const StockApiService &) = delete;

    QList<StockSearchResult> parseSinaSearchData(const QString &response)
    {
        QList<StockSearchResult> results;

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
                    QString code = fields[3];
                    QString name = fields[4];
                    QString market = fields[1];
                    QString type = fields[0];

                    if (!code.isEmpty() && !name.isEmpty())
                    {
                        StockSearchResult result(code, name, market, type);
                        results.append(result);
                    }
                }
            }
        }

        return results;
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

            if (fields.size() >= 32)
            {
                stockInfo.setCode(code.toUpper());
                stockInfo.setName(fields[0]);
                Logger::info(QString("API parsed stock: code=%1, name=%2, fields[0]=%3").arg(code, fields[0], fields[0]));

                // 基本价格信息
                stockInfo.setOpenPrice(parseDouble(fields[1]));    // 开盘价
                stockInfo.setYesterday(parseDouble(fields[2]));    // 昨收价
                stockInfo.setCurrentPrice(parseDouble(fields[3])); // 当前价
                stockInfo.setHigh(parseDouble(fields[4]));         // 最高价
                stockInfo.setLow(parseDouble(fields[5]));          // 最低价

                // 买卖盘信息
                stockInfo.setBid1(parseDouble(fields[6])); // 买一价
                stockInfo.setAsk1(parseDouble(fields[7])); // 卖一价

                // 成交信息
                stockInfo.setVolume(parseLongLong(fields[8])); // 成交量（股）
                stockInfo.setTurnover(parseDouble(fields[9])); // 成交额（元）

                // 买卖盘量信息
                stockInfo.setBid1Volume(parseLongLong(fields[10])); // 买一量
                stockInfo.setAsk1Volume(parseLongLong(fields[20])); // 卖一量

                // 交易时间信息
                if (fields.size() > 30)
                {
                    stockInfo.setTradeDate(fields[30]); // 交易日期
                }
                if (fields.size() > 31)
                {
                    stockInfo.setTradeTime(fields[31]); // 交易时间
                }

                stockInfo.setLastUpdate(QDateTime::currentDateTime());

                double change = stockInfo.currentPrice() - stockInfo.yesterday();
                stockInfo.setChange(change);

                double changePercent = stockInfo.yesterday() > 0
                                           ? (change / stockInfo.yesterday()) * 100
                                           : 0;
                stockInfo.setChangePercent(changePercent);
            }
        }

        return stockInfo;
    }

    double parseDouble(const QString &value)
    {
        bool ok;
        double result = value.toDouble(&ok);
        return ok ? result : 0.0;
    }

    long long parseLongLong(const QString &value)
    {
        bool ok;
        long long result = value.toLongLong(&ok);
        return ok ? result : 0;
    }

    QString formatStockCode(const QString &code)
    {
        QString upperCode = code.toUpper();

        if (upperCode.startsWith("SH") || upperCode.startsWith("SZ"))
        {
            return upperCode.toLower();
        }

        if (upperCode.startsWith("6"))
        {
            return QString("sh%1").arg(upperCode);
        }
        else if (upperCode.startsWith("0") || upperCode.startsWith("3"))
        {
            return QString("sz%1").arg(upperCode);
        }

        return upperCode.toLower();
    }

    QString formatStockCodeForTencent(const QString &code)
    {
        QString upperCode = code.toUpper();

        // Handle codes that already have exchange prefix
        if (upperCode.startsWith("SH"))
        {
            return "sh" + upperCode.mid(2); // SH000001 -> sh000001
        }
        else if (upperCode.startsWith("SZ"))
        {
            return "sz" + upperCode.mid(2); // SZ000002 -> sz000002
        }

        // Auto-detect exchange based on code
        if (upperCode.startsWith("0") || upperCode.startsWith("3"))
        {
            return "sz" + upperCode; // Shenzhen: 000002 -> sz000002
        }
        else if (upperCode.startsWith("6"))
        {
            return "sh" + upperCode; // Shanghai: 000001 -> sh000001
        }

        // Default format
        return "sz" + upperCode;
    }

    QList<StockInfo> parseSinaBatchStockData(const QStringList &originalCodes, const QStringList &formattedCodes, const QString &response)
    {
        QList<StockInfo> stockInfos;

        // Sina API returns data in format: var hq_str_sz000001="name,price,yesterday,change,...";
        QStringList lines = response.split("\n", Qt::SkipEmptyParts);

        for (int i = 0; i < originalCodes.size() && i < formattedCodes.size(); ++i)
        {
            QString originalCode = originalCodes[i];
            QString formattedCode = formattedCodes[i];

            // Find the line for this stock
            QString stockLine;
            for (const QString &line : lines)
            {
                if (line.contains(formattedCode))
                {
                    stockLine = line;
                    break;
                }
            }

            if (stockLine.isEmpty())
            {
                Logger::info(QString("No data found for code: %1 (formatted: %2)").arg(originalCode, formattedCode));
                continue;
            }

            Logger::info(QString("Processing stock %1: %2").arg(originalCode, stockLine.left(100)));

            StockInfo stockInfo = parseSinaStockData(originalCode, stockLine);
            if (!stockInfo.code().isEmpty())
            {
                stockInfos.append(stockInfo);
                Logger::info(QString("Parsed Sina stock: %1 - %2, Price: %3")
                                 .arg(stockInfo.code(), stockInfo.name())
                                 .arg(stockInfo.currentPrice()));
            }
        }

        return stockInfos;
    }

    QNetworkAccessManager *m_networkManager;
    QList<QNetworkReply *> m_pendingReplies;
    QStringList m_pendingCodes;
    QList<StockInfo> m_pendingStockInfos;
    int m_expectedReplies;
};

#endif // STOCKAPISERVICE_HPP
