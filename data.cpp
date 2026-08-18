#include "data.h"
#include "ui_data.h"

#include <QVBoxLayout>
#include <QButtonGroup>

DataPage::DataPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DataPage)
{
    ui->setupUi(this);

    // ---- 创建三个曲线图控件，放入 chartContainer ---- //
    setupChartWidgets();

    // ---- 传感器选择按钮组（互斥）---- //
    m_sensorGroup = new QButtonGroup(this);
    m_sensorGroup->setExclusive(true);
    m_sensorGroup->addButton(ui->btnDS18B20, 0);
    m_sensorGroup->addButton(ui->btnBH1750,  1);
    m_sensorGroup->addButton(ui->btnMHZ19B,  2);

    connect(m_sensorGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &DataPage::onSensorSelectorChanged);

    // 默认显示温度曲线
    switchChartTo(0);
}

DataPage::~DataPage()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// 曲线图控件初始化
// ---------------------------------------------------------------------------
void DataPage::setupChartWidgets()
{
    QVBoxLayout *chartLayout = new QVBoxLayout(ui->chartContainer);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->setSpacing(0);

    // --- 温度曲线 ---
    m_tempChart = new CurveWidget(ui->chartContainer);
    m_tempChart->setTitle("DS18B20 温度变化曲线");
    m_tempChart->setYLabel("温度");
    m_tempChart->setYUnit("°C");
    m_tempChart->setLineColor(QColor(0x00, 0xCC, 0xFF));
    m_tempChart->setYRange(20, 40);
    m_tempChart->setMaxPoints(120);   // ~1 minute of data at 500ms interval

    // --- 光照曲线 ---
    m_luxChart = new CurveWidget(ui->chartContainer);
    m_luxChart->setTitle("BH1750 光照强度变化曲线");
    m_luxChart->setYLabel("光照");
    m_luxChart->setYUnit("Lux");
    m_luxChart->setLineColor(QColor(0xFF, 0xAA, 0x33));
    m_luxChart->setYRange(0, 2000);
    m_luxChart->setMaxPoints(120);

    // --- CO₂ 曲线 ---
    m_co2Chart = new CurveWidget(ui->chartContainer);
    m_co2Chart->setTitle("MH-Z19B CO₂ 浓度变化曲线");
    m_co2Chart->setYLabel("CO₂");
    m_co2Chart->setYUnit("ppm");
    m_co2Chart->setLineColor(QColor(0x44, 0xDD, 0x55));
    m_co2Chart->setYRange(550, 2000);
    m_co2Chart->setMaxPoints(120);

    // 全部加入 layout，初始全部隐藏
    chartLayout->addWidget(m_tempChart);
    chartLayout->addWidget(m_luxChart);
    chartLayout->addWidget(m_co2Chart);

    m_tempChart->hide();
    m_luxChart->hide();
    m_co2Chart->hide();
}

// ---------------------------------------------------------------------------
// 切换显示的曲线
// ---------------------------------------------------------------------------
void DataPage::switchChartTo(int sensorId)
{
    m_tempChart->hide();
    m_luxChart->hide();
    m_co2Chart->hide();

    switch (sensorId) {
    case 0: m_tempChart->show(); break;
    case 1: m_luxChart->show();  break;
    case 2: m_co2Chart->show();  break;
    }
}

void DataPage::onSensorSelectorChanged(int id)
{
    switchChartTo(id);
}

// ---------------------------------------------------------------------------
// 传感器数值更新
// ---------------------------------------------------------------------------
void DataPage::setDS18B20Value(double temperature)
{
    ui->ds18b20ValueLabel->setText(QString::number(temperature, 'f', 1));
    m_tempChart->addDataPoint(temperature);
}

void DataPage::setBH1750Value(double lux)
{
    ui->bh1750ValueLabel->setText(QString::number(lux, 'f', 1));
    m_luxChart->addDataPoint(lux);
}

void DataPage::setMHZ19BValue(double co2ppm)
{
    ui->mhz19bValueLabel->setText(QString::number((int)co2ppm));
    m_co2Chart->addDataPoint(co2ppm);
}

// ---------------------------------------------------------------------------
// 传感器连接状态
// ---------------------------------------------------------------------------
void DataPage::setDS18B20Status(bool connected)
{
    if (connected) {
        ui->ds18b20StatusLabel->setText("已连接");
        ui->ds18b20StatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");
    } else {
        ui->ds18b20StatusLabel->setText("未连接");
        ui->ds18b20StatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #dd4444; background: transparent;");
    }
}

void DataPage::setBH1750Status(bool connected)
{
    if (connected) {
        ui->bh1750StatusLabel->setText("已连接");
        ui->bh1750StatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");
    } else {
        ui->bh1750StatusLabel->setText("未连接");
        ui->bh1750StatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #dd4444; background: transparent;");
    }
}

void DataPage::setMHZ19BStatus(bool connected)
{
    if (connected) {
        ui->mhz19bStatusLabel->setText("已连接");
        ui->mhz19bStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");
    } else {
        ui->mhz19bStatusLabel->setText("未连接");
        ui->mhz19bStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #dd4444; background: transparent;");
    }
}

// ---------------------------------------------------------------------------
// 风扇 / 舵机状态
// ---------------------------------------------------------------------------
void DataPage::setFanStatus(bool on, int speedPercent)
{
    if (on) {
        ui->fanValueLabel->setText(QString("ON  %1%").arg(speedPercent));
        ui->fanValueLabel->setStyleSheet(
            "font: 20pt \"Consolas\"; color: #44ddff; background: transparent;");
        ui->fanStatusLabel->setText("运行中");
        ui->fanStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");
    } else {
        ui->fanValueLabel->setText("OFF");
        ui->fanValueLabel->setStyleSheet(
            "font: 20pt \"Consolas\"; color: #666688; background: transparent;");
        ui->fanStatusLabel->setText("已关闭");
        ui->fanStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #666688; background: transparent;");
    }
}

void DataPage::setServoStatus(bool on, int angle)
{
    if (on) {
        ui->servoValueLabel->setText(QString("ON  %1°").arg(angle));
        ui->servoValueLabel->setStyleSheet(
            "font: 20pt \"Consolas\"; color: #ffcc44; background: transparent;");
        ui->servoStatusLabel->setText("运行中");
        ui->servoStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");
    } else {
        ui->servoValueLabel->setText("OFF");
        ui->servoValueLabel->setStyleSheet(
            "font: 20pt \"Consolas\"; color: #666688; background: transparent;");
        ui->servoStatusLabel->setText("已关闭");
        ui->servoStatusLabel->setStyleSheet(
            "font: 9pt \"Microsoft YaHei\"; color: #666688; background: transparent;");
    }
}
