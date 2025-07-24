#pragma once

#include "StockInfo.hpp"
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

struct StockConfig
{
    QStringList stockCodes;
    QString apiSource;
    QString hotkey{ "Ctrl+Alt+S" };
    QString theme{ "light" };
    qreal windowOpacity{ 1.0 };
    int refreshInterval{ 5000 };
    QByteArray lastGeometry;
    int rowHeight{ 25 };

    QStringList availableThemes{ "light", "dark" };

    StockConfig()
        : filepath_(QString("%1/stocks.ini").arg(QApplication::applicationDirPath()))
    {
        QSettings settings(filepath_, QSettings::IniFormat);

        apiSource = settings.value("api/source", "sina").toString();
        stockCodes = settings.value("Stock/Codes").toString().split(",", Qt::SkipEmptyParts);

        settings.beginGroup("UI");
        theme = settings.value("Theme", "light").toString();
        windowOpacity = settings.value("WindowOpacity", 1.0).toReal();
        refreshInterval = settings.value("RefreshInterval", 5000).toInt();
        lastGeometry = settings.value("Rect").toByteArray();
        rowHeight = settings.value("RowHeight", 25).toInt();
        settings.endGroup();

        hotkey = settings.value("Hotkey/Global", "Ctrl+Alt+S").toString();
    }

    ~StockConfig()
    {
        save();
    }

    void save()
    {
        QSettings settings(filepath_, QSettings::IniFormat);
        settings.setValue("api/source", apiSource);
        settings.setValue("Stock/Codes", stockCodes.join(","));

        // Save [UI] section
        settings.beginGroup("UI");
        settings.setValue("Theme", theme);
        settings.setValue("WindowOpacity", QString::number(windowOpacity, 'f', 2));
        settings.setValue("RefreshInterval", refreshInterval);
        settings.setValue("Rect", lastGeometry);
        settings.setValue("RowHeight", rowHeight);
        settings.endGroup();

        settings.setValue("Hotkey/Global", hotkey);
        settings.sync();
    }

    bool isDarkTheme() const
    {
        return theme == "dark";
    }

    QColor getPriceColor(bool up) const
    {
        if (up)
        {
            return isDarkTheme() ? QColor(255, 85, 85) : QColor(220, 20, 60);
        }
        else
        {
            return isDarkTheme() ? QColor(85, 255, 85) : QColor(34, 139, 34);
        }
    }

    QColor getTextColor() const
    {
        return isDarkTheme() ? QColor(255, 255, 255) : QColor(0, 0, 0);
    }

private:
    QString filepath_;
};
