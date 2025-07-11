#pragma once

#include <QString>

class StockSearchResult
{
public:
    StockSearchResult() = default;
    StockSearchResult(const QString &code, const QString &name, const QString &market = QString(), const QString &type = QString())
        : m_code(code.toUpper())
        , m_name(name)
        , m_market(market)
        , m_type(type)
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

    QString market() const
    {
        return m_market;
    }
    void setMarket(const QString &market)
    {
        m_market = market;
    }

    QString type() const
    {
        return m_type;
    }
    void setType(const QString &type)
    {
        m_type = type;
    }

    QString displayText() const
    {
        return QString("%1 - %2").arg(m_code, m_name);
    }

private:
    QString m_code;
    QString m_name;
    QString m_market;
    QString m_type;
};
