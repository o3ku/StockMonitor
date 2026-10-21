#pragma once

#include <QApplication>
#include <QColor>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

struct StockConfig
{
    QStringList stockCodes;
    QStringList pinnedStocks;
    QString apiSource;
    QString hotkey{ "Ctrl+Alt+S" };
    QString theme{ "light" };
    qreal windowOpacity{ 1.0 };
    int refreshInterval{ 5000 };
    QByteArray lastGeometry;
    int rowHeight{ 25 };
    int autoHideDelay{ 0 };

    QStringList availableThemes{ "light", "dark" };

    StockConfig()
        : filepath_(QString("%1/stocks.ini").arg(QApplication::applicationDirPath()))
    {
        QSettings settings(filepath_, QSettings::IniFormat);

        apiSource = settings.value("api/source", "sina").toString();
        stockCodes = settings.value("Stock/Codes").toString().split(",", Qt::SkipEmptyParts);
        pinnedStocks = settings.value("Stock/Pinned").toString().split(",", Qt::SkipEmptyParts);

        settings.beginGroup("UI");
        theme = settings.value("Theme", "light").toString();
        windowOpacity = settings.value("WindowOpacity", 1.0).toReal();
        refreshInterval = settings.value("RefreshInterval", 5000).toInt();
        lastGeometry = settings.value("Rect").toByteArray();
        rowHeight = settings.value("RowHeight", 25).toInt();
        QVariant autoHideDelayMs = settings.value("AutoHideDelayMs");
        if (autoHideDelayMs.isValid())
        {
            autoHideDelay = autoHideDelayMs.toInt();
        }
        else
        {
            int legacyDelay = settings.value("AutoHideSeconds", 0).toInt();
            autoHideDelay = legacyDelay > 0 && legacyDelay <= 30 ? legacyDelay * 1000 : legacyDelay;
        }
        settings.endGroup();

        hotkey = settings.value("Hotkey/Global", "Ctrl+Alt+S").toString();
    }

    ~StockConfig()
    {
        if (QCoreApplication::instance())
            save();
    }

    void save()
    {
        QSettings settings(filepath_, QSettings::IniFormat);
        settings.setValue("api/source", apiSource);
        settings.setValue("Stock/Codes", stockCodes.join(","));
        settings.setValue("Stock/Pinned", pinnedStocks.join(","));

        // Save [UI] section
        settings.beginGroup("UI");
        settings.setValue("Theme", theme);
        settings.setValue("WindowOpacity", QString::number(windowOpacity, 'f', 2));
        settings.setValue("RefreshInterval", refreshInterval);
        settings.setValue("Rect", lastGeometry);
        settings.setValue("RowHeight", rowHeight);
        settings.setValue("AutoHideDelayMs", autoHideDelay);
        settings.setValue("AutoHideSeconds", autoHideDelay);
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
            return isDarkTheme() ? QColor("#EB7A69") : QColor(220, 20, 60);
        }
        else
        {
            return isDarkTheme() ? QColor("#76E0A1") : QColor(34, 139, 34);
        }
    }

    QColor getTextColor() const
    {
        return isDarkTheme() ? QColor(255, 255, 255) : QColor(0, 0, 0);
    }

private:
    QString filepath_;
};
