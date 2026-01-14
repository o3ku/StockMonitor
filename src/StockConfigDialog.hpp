#pragma once

#include "StockConfig.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

class StockConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StockConfigDialog(const std::shared_ptr<StockConfig> &config, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        auto form = new QFormLayout;
        auto apiSourceCombo = new QComboBox(this);
        apiSourceCombo->addItem("新浪财经", "sina");
        apiSourceCombo->addItem("腾讯财经", "tencent");
        apiSourceCombo->addItem("东方财富", "eastmoney");
        int apiIndex = apiSourceCombo->findData(config->apiSource);
        apiSourceCombo->setCurrentIndex(apiIndex != -1 ? apiIndex : 0);
        form->addRow("数据来源:", apiSourceCombo);

        auto refreshIntervalSpin = new QSpinBox(this);
        refreshIntervalSpin->setRange(1000, 60000);
        refreshIntervalSpin->setSingleStep(1000);
        refreshIntervalSpin->setSuffix(" 毫秒");
        refreshIntervalSpin->setValue(config->refreshInterval);
        form->addRow("刷新间隔:", refreshIntervalSpin);

        auto autoHideDelayBox = new QComboBox(this);
        autoHideDelayBox->addItem("禁用", 0);
        autoHideDelayBox->addItem("2秒", 2000);
        autoHideDelayBox->addItem("5秒", 5000);
        autoHideDelayBox->addItem("10秒", 10000);
        autoHideDelayBox->setCurrentText(QString("%1秒").arg(config->autoHideDelay / 1000));
        form->addRow("自动隐藏:", autoHideDelayBox);

        auto themeCombo = new QComboBox(this);
        themeCombo->addItem("浅色", "light");
        themeCombo->addItem("深色", "dark");
        int themeIndex = themeCombo->findData(config->theme);
        themeCombo->setCurrentIndex(themeIndex != -1 ? themeIndex : 0);
        form->addRow("主题:", themeCombo);

        auto opacitySpin = new QDoubleSpinBox(this);
        opacitySpin->setRange(0.3, 1.0);
        opacitySpin->setSingleStep(0.05);
        opacitySpin->setDecimals(2);
        opacitySpin->setValue(config->windowOpacity);
        form->addRow("窗口透明度:", opacitySpin);

        auto rowHeightSpin = new QSpinBox(this);
        rowHeightSpin->setRange(20, 50);
        rowHeightSpin->setSingleStep(1);
        rowHeightSpin->setSuffix(" 像素");
        rowHeightSpin->setValue(config->rowHeight);
        form->addRow("行高:", rowHeightSpin);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, [=]() {
            config->apiSource = apiSourceCombo->currentData().toString();
            config->refreshInterval = refreshIntervalSpin->value();
            config->theme = themeCombo->currentData().toString();
            config->windowOpacity = opacitySpin->value();
            config->rowHeight = rowHeightSpin->value();
            config->autoHideDelay = autoHideDelayBox->currentData().toInt();
            this->accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form, 1);
        layout->addWidget(buttonBox);
    }
};
