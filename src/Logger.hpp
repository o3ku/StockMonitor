#pragma once

#include <QDateTime>
#include <QDebug>
#include <QString>

class Logger
{
public:
    static void info(const QString &message)
    {
        qDebug() << "[INFO]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << message;
    }

    static void error(const QString &message)
    {
        qDebug() << "[ERROR]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << message;
    }

private:
    Logger() = default;
};
