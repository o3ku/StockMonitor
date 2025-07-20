#pragma once

#include <QDateTime>
#include <QString>

class StockInfo
{
public:
    StockInfo() = default;
    StockInfo(const QString &code, const QString &name = QString())
        : m_code(code.toUpper())
        , m_name(name)
    {
    }

    QString code() const
    {
        return m_code;
    }
    void setCode(const QString &code)
    {
        m_code = code.toUpper();
    }

    QString name() const
    {
        return m_name;
    }
    void setName(const QString &name)
    {
        m_name = name;
    }

    double currentPrice() const
    {
        return m_currentPrice;
    }
    void setCurrentPrice(double price)
    {
        m_currentPrice = price;
    }

    double change() const
    {
        return m_change;
    }
    void setChange(double change)
    {
        m_change = change;
    }

    double changePercent() const
    {
        return m_changePercent;
    }
    void setChangePercent(double percent)
    {
        m_changePercent = percent;
    }

    double high() const
    {
        return m_high;
    }
    void setHigh(double high)
    {
        m_high = high;
    }

    double low() const
    {
        return m_low;
    }
    void setLow(double low)
    {
        m_low = low;
    }

    double yesterday() const
    {
        return m_yesterday;
    }
    void setYesterday(double yesterday)
    {
        m_yesterday = yesterday;
    }

    long long volume() const
    {
        return m_volume;
    }
    void setVolume(long long volume)
    {
        m_volume = volume;
    }

    double turnover() const
    {
        return m_turnover;
    }
    void setTurnover(double turnover)
    {
        m_turnover = turnover;
    }

    double openPrice() const
    {
        return m_openPrice;
    }
    void setOpenPrice(double price)
    {
        m_openPrice = price;
    }

    double bid1() const
    {
        return m_bid1;
    }
    void setBid1(double price)
    {
        m_bid1 = price;
    }

    double ask1() const
    {
        return m_ask1;
    }
    void setAsk1(double price)
    {
        m_ask1 = price;
    }

    long long bid1Volume() const
    {
        return m_bid1Volume;
    }
    void setBid1Volume(long long volume)
    {
        m_bid1Volume = volume;
    }

    long long ask1Volume() const
    {
        return m_ask1Volume;
    }
    void setAsk1Volume(long long volume)
    {
        m_ask1Volume = volume;
    }

    QString tradeDate() const
    {
        return m_tradeDate;
    }
    void setTradeDate(const QString &date)
    {
        m_tradeDate = date;
    }

    QString tradeTime() const
    {
        return m_tradeTime;
    }
    void setTradeTime(const QString &time)
    {
        m_tradeTime = time;
    }

    QDateTime lastUpdate() const
    {
        return m_lastUpdate;
    }
    void setLastUpdate(const QDateTime &dateTime)
    {
        m_lastUpdate = dateTime;
    }

    bool isUp() const
    {
        return m_change > 0;
    }

    QString changeText() const
    {
        if (m_change >= 0)
        {
            return QString("+%1").arg(m_change, 0, 'f', 2);
        }
        else
        {
            return QString("%1").arg(m_change, 0, 'f', 2);
        }
    }

    QString changePercentText() const
    {
        if (m_changePercent >= 0)
        {
            return QString("+%1%").arg(m_changePercent, 0, 'f', 2);
        }
        else
        {
            return QString("%1%").arg(m_changePercent, 0, 'f', 2);
        }
    }

    QString volumeText() const
    {
        if (m_volume >= 100000000)  // 1亿
        {
            return QString("%1亿").arg(m_volume / 100000000.0, 0, 'f', 2);
        }
        else if (m_volume >= 10000)  // 1万
        {
            return QString("%1万").arg(m_volume / 10000.0, 0, 'f', 1);
        }
        else
        {
            return QString("%1").arg(m_volume);
        }
    }

    QString turnoverText() const
    {
        if (m_turnover >= 100000000)  // 1亿
        {
            return QString("%1亿").arg(m_turnover / 100000000.0, 0, 'f', 2);
        }
        else if (m_turnover >= 10000)  // 1万
        {
            return QString("%1万").arg(m_turnover / 10000.0, 0, 'f', 1);
        }
        else
        {
            return QString("%1").arg(m_turnover, 0, 'f', 0);
        }
    }

    QString detailedTooltip() const
    {
        return QString("股票代码: %1\n"
                       "股票名称: %2\n"
                       "当前价格: %3\n"
                       "涨跌额: %4\n"
                       "涨跌幅: %5\n"
                       "开盘价: %6\n"
                       "昨收价: %7\n"
                       "最高价: %8\n"
                       "最低价: %9\n"
                       "成交量: %10\n"
                       "成交额: %11\n"
                       "买一价: %12 (%13手)\n"
                       "卖一价: %14 (%15手)\n"
                       "交易日期: %16\n"
                       "交易时间: %17\n"
                       "更新时间: %18")
            .arg(m_code)
            .arg(m_name)
            .arg(QString::number(m_currentPrice, 'f', 2))
            .arg(changeText())
            .arg(changePercentText())
            .arg(QString::number(m_openPrice, 'f', 2))
            .arg(QString::number(m_yesterday, 'f', 2))
            .arg(QString::number(m_high, 'f', 2))
            .arg(QString::number(m_low, 'f', 2))
            .arg(volumeText())
            .arg(turnoverText())
            .arg(QString::number(m_bid1, 'f', 2))
            .arg(m_bid1Volume / 100)  // 转换为手
            .arg(QString::number(m_ask1, 'f', 2))
            .arg(m_ask1Volume / 100)  // 转换为手
            .arg(m_tradeDate)
            .arg(m_tradeTime)
            .arg(m_lastUpdate.toString("hh:mm:ss"));
    }

private:
    QString m_code;
    QString m_name;
    double m_currentPrice = 0.0;
    double m_change = 0.0;
    double m_changePercent = 0.0;
    double m_high = 0.0;
    double m_low = 0.0;
    double m_yesterday = 0.0;
    double m_openPrice = 0.0;    // 开盘价
    long long m_volume = 0;      // 成交量（股）
    double m_turnover = 0.0;     // 成交额（元）
    double m_bid1 = 0.0;         // 买一价
    double m_ask1 = 0.0;         // 卖一价
    long long m_bid1Volume = 0;  // 买一量（股）
    long long m_ask1Volume = 0;  // 卖一量（股）
    QString m_tradeDate;         // 交易日期
    QString m_tradeTime;         // 交易时间
    QDateTime m_lastUpdate = QDateTime::currentDateTime();
};
