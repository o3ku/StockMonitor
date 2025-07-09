#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <QString>
#include <QDebug>
#include <QDateTime>

class Logger
{
public:
    static void info(const QString& message)
    {
        qDebug() << "[INFO]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << message;
    }
    
    static void debug(const QString& message)
    {
        qDebug() << "[DEBUG]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << message;
    }
    
    static void error(const QString& message)
    {
        qDebug() << "[ERROR]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << message;
    }
    
    static void apiStart(const QString& operation, const QString& url)
    {
        qDebug() << "[API START]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") 
                 << operation << "URL:" << url;
    }
    
    static void apiComplete(const QString& operation, double duration, int responseSize)
    {
        qDebug() << "[API COMPLETE]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") 
                 << operation << QString("Duration: %1ms, Size: %2 bytes").arg(duration, 0, 'f', 0).arg(responseSize);
    }
    
    static void apiError(const QString& operation, double duration, const QString& error)
    {
        qDebug() << "[API ERROR]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") 
                 << operation << QString("Duration: %1ms, Error: %2").arg(duration, 0, 'f', 0).arg(error);
    }

private:
    Logger() = default;
};

#endif // LOGGER_HPP
