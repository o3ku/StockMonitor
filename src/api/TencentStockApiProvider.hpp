#pragma once

#include "../StockInfo.hpp"
#include "IStockApiProvider.hpp"
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTextCodec>
#include <QUrl>
#include <QVariant>

class TencentStockApiProvider : public IStockApiProvider
{
    Q_OBJECT

public:
    explicit TencentStockApiProvider(QObject *parent = nullptr)
        : IStockApiProvider(parent)
    {
    }

    QString name() const override
    {
        return "tencent";
    }

    void searchStocksAsync(const QString &keyword, quint64 requestId) override
    {
        QString encodedKeyword = QUrl::toPercentEncoding(keyword);
        QUrl url(QString("https://smartbox.gtimg.cn/s3/?v=2&t=all&c=1&q=%1").arg(encodedKeyword));
        QNetworkRequest req(url);
        auto reply = netmgr_->get(req);
        reply->setProperty("searchKeyword", keyword);
        reply->setProperty("searchRequestId", QVariant::fromValue(requestId));
        connect(reply, &QNetworkReply::finished, this, &TencentStockApiProvider::onSearchRequestSucceeded);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }

    void getStockInfosAsync(const QStringList &codes) override
    {
        QStringList formattedCodes;
        for (const QString &code : codes)
        {
            QString formattedCode = formatStockCode(code);
            formattedCodes.append(formattedCode);
        }

        QUrl url(QString("https://qt.gtimg.cn/q=%1").arg(formattedCodes.join(",")));
        QNetworkRequest req(url);
        auto reply = netmgr_->get(req);
        connect(reply, &QNetworkReply::finished, this, &TencentStockApiProvider::onStockInfoRequestSucceeded);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }

private slots:
    void onSearchRequestSucceeded()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;
        const QString keyword = reply->property("searchKeyword").toString();
        const quint64 requestId = reply->property("searchRequestId").toULongLong();
        if (reply->error() != QNetworkReply::NoError)
        {
            emit errorOccurred(QString("Search failed: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray &responseData = reply->readAll();
        // 使用GBK编码解析响应数据
        QTextCodec *codec = QTextCodec::codecForName("GBK");
        QString response = codec ? codec->toUnicode(responseData) : QString::fromUtf8(responseData);

        // 解析新接口返回的数据格式
        QList<StockInfo> results;

        // 解析 v_hint="sz~002609~捷顺科技~jskj~GP-A^sz~300193~佳士科技~jskj~GP-A^..." 格式
        QRegularExpression pattern("v_hint=\"([^\"]+)\"");
        QRegularExpressionMatch match = pattern.match(response);

        if (match.hasMatch())
        {
            QString data = match.captured(1);
            QStringList stockEntries = data.split('^', Qt::SkipEmptyParts);

            for (const QString &entry : stockEntries)
            {
                QStringList fields = entry.split('~');

                if (fields.size() >= 3)
                {
                    QString market = fields[0];  // sz, sh, us, hk...
                    QString code = fields[1];    // 股票代码
                    QString name = fields[2];    // 股票名称

                    if (!code.isEmpty() && !name.isEmpty())
                    {
                        // 解码Unicode转义序列
                        name = decodeUnicodeEscapes(name);

                        const QString marketLower = market.toLower();
                        QString standardCode;
                        if (marketLower == "us")
                        {
                            standardCode = normalizeUsSymbol(code);
                        }
                        else
                        {
                            standardCode = marketPrefixFor(marketLower) + code;
                        }
                        StockInfo result(standardCode, name);
                        results.append(result);
                    }
                }
            }
        }

        emit searchCompleted(requestId, keyword, results);
    }

    void onStockInfoRequestSucceeded()
    {
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;
        if (reply->error() != QNetworkReply::NoError)
        {
            emit errorOccurred(QString("Stock info failed: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray &responseData = reply->readAll();
        // 使用GBK编码解析响应数据
        QTextCodec *codec = QTextCodec::codecForName("GBK");
        QString response = codec ? codec->toUnicode(responseData) : QString::fromUtf8(responseData);

        // 由于我们无法从worker获取属性，我们需要重新解析所有可能的股票代码
        // 这里简化处理，直接解析响应中的所有股票数据
        QList<StockInfo> stockInfos = parseAllStockDataFromResponse(response);
        emit stockInfosReceived(stockInfos);
    }

    void onRequestFailed(const QString &errorMessage, int statusCode)
    {
        Q_UNUSED(statusCode)
        emit errorOccurred(errorMessage);
    }

private:
    QString decodeUnicodeEscapes(const QString &input)
    {
        QString result = input;
        QRegularExpression unicodePattern("\\\\u([0-9a-fA-F]{4})");
        QRegularExpressionMatchIterator iterator = unicodePattern.globalMatch(input);
        
        while (iterator.hasNext())
        {
            QRegularExpressionMatch match = iterator.next();
            QString unicodeStr = match.captured(1);
            bool ok;
            ushort unicodeValue = unicodeStr.toUShort(&ok, 16);
            if (ok)
            {
                QChar unicodeChar(unicodeValue);
                result.replace(match.captured(0), unicodeChar);
            }
        }
        
        return result;
    }

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
        if (isUsSymbol(upperCode))
        {
            return QString("us%1").arg(normalizeUsSymbol(upperCode));
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

    QList<StockInfo> parseAllStockDataFromResponse(const QString &response)
    {
        QList<StockInfo> stockInfos;
        QStringList lines = response.split("\n", Qt::SkipEmptyParts);

        for (const QString &line : lines)
        {
            if (line.startsWith("v_") && line.contains("="))
            {
                // 从行中提取股票代码
                QString stockCode = extractStockCodeFromLine(line);
                if (!stockCode.isEmpty())
                {
                    StockInfo stockInfo = parseTencentStockData(stockCode, line);
                    if (!stockInfo.code().isEmpty())
                    {
                        stockInfos.append(stockInfo);
                    }
                }
            }
        }

        return stockInfos;
    }

    QString extractStockCodeFromLine(const QString &line)
    {
        // 从 "v_sh000001=" 或 "v_usAAPL=" 这样的格式中提取股票代码
        QRegularExpression pattern("v_([a-z]{2}[A-Za-z0-9\\.]{1,8})=");
        QRegularExpressionMatch match = pattern.match(line);

        if (match.hasMatch())
        {
            QString formattedCode = match.captured(1);
            // 转换为标准格式
            if (formattedCode.startsWith("sh"))
            {
                return "SH" + formattedCode.mid(2);
            }
            else if (formattedCode.startsWith("sz"))
            {
                return "SZ" + formattedCode.mid(2);
            }
            else if (formattedCode.startsWith("hk"))
            {
                return "HK" + formattedCode.mid(2);
            }
            else if (formattedCode.startsWith("us"))
            {
                return normalizeUsSymbol(formattedCode.mid(2));
            }
        }

        return QString();
    }

    // 这个方法在当前实现中未使用，移除以避免编译错误
    /*
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
                // 使用更精确的匹配模式，确保找到正确的股票数据行
                if (line.startsWith(QString("v_%1=").arg(formattedCode)) || line.contains(QString("=\"%1~").arg(formattedCode)))
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
            else
            {
                // 尝试使用不同的格式化方式再次查找
                QString altFormattedCode;
                if (originalCode.startsWith("SH"))
                {
                    altFormattedCode = "sh" + originalCode.mid(2);
                }
                else if (originalCode.startsWith("SZ"))
                {
                    altFormattedCode = "sz" + originalCode.mid(2);
                }

                if (!altFormattedCode.isEmpty())
                {
                    for (const QString &line : lines)
                    {
                        if (line.startsWith(QString("v_%1=").arg(altFormattedCode)) || line.contains(QString("=\"%1~").arg(altFormattedCode)))
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
            }
        }

        return stockInfos;
    }
    */

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

            if (fields.size() >= 35)  // 确保有足够的字段
            {
                stockInfo.setCode(code);
                stockInfo.setName(fields[1]);

                // 设置基本价格信息
                stockInfo.setCurrentPrice(fields[3].toDouble());
                stockInfo.setYesterday(fields[4].toDouble());
                stockInfo.setVolume(fields[36].toLongLong() * 100);      // 转换为股数
                stockInfo.setTurnover(fields[37].toDouble() * 10000);    // 转换为元
                stockInfo.setBid1(fields[9].toDouble());                 // 买一价
                stockInfo.setAsk1(fields[19].toDouble());                // 卖一价
                stockInfo.setBid1Volume(fields[10].toLongLong() * 100);  // 买一量
                stockInfo.setAsk1Volume(fields[20].toLongLong() * 100);  // 卖一量
                stockInfo.setChange(fields[31].toDouble());
                stockInfo.setChangePercent(fields[32].toDouble());

                stockInfo.setLastUpdate(QDateTime::currentDateTime());
            }
        }

        return stockInfo;
    }

    bool isUsSymbol(const QString &upperCode) const
    {
        if (upperCode.startsWith("US")) return true;
        for (const QChar ch : upperCode)
        {
            if (ch.isLetter()) return true;
        }
        return false;
    }

    QString normalizeUsSymbol(const QString &upperCode) const
    {
        QString symbol = upperCode;
        if (symbol.startsWith("US"))
        {
            symbol = symbol.mid(2);
        }
        int dot = symbol.indexOf('.');
        if (dot >= 0)
        {
            symbol = symbol.left(dot);
        }
        return symbol.toUpper();
    }

    QString marketPrefixFor(const QString &marketLower) const
    {
        if (marketLower == "sh") return "SH";
        if (marketLower == "sz") return "SZ";
        if (marketLower == "hk") return "HK";
        return marketLower.toUpper();
    }
};
