#include "control.h"
#include "ui_control.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>

control::control(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::control)
{
    ui->setupUi(this);

    // 创建传感器 toggle 开关
    setupSensorToggles();

    // 创建风扇 ON/OFF 按钮 + 舵机 toggle
    setupFanButtons();
    setupServoToggle();

    // ---- 滑块连接 ----
    connect(ui->servoSlider, &QSlider::valueChanged,
            this, &control::onServoSliderChanged);

    // 舵机滑块初始禁用
    ui->servoSlider->setEnabled(false);

    // ---- 摄像头按钮 ----
    connect(ui->btnOpenCamera, &QPushButton::clicked,
            this, &control::cameraOpenRequested);
    connect(ui->btnCloseCamera, &QPushButton::clicked,
            this, &control::cameraCloseRequested);
}

control::~control()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// 传感器 toggle 开关（添加到左侧 sensorGroup 的 layout 中）
// ---------------------------------------------------------------------------
void control::setupSensorToggles()
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ui->sensorGroup->layout());
    if (!layout) return;

    // --- DS18B20 ---
    {
        QWidget *row = new QWidget(ui->sensorGroup);
        row->setStyleSheet("background: transparent;");
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(4, 4, 4, 4);

        QLabel *icon = new QLabel("DS18B20", row);
        icon->setStyleSheet("font: bold 12pt \"Microsoft YaHei\"; color: #00ccff; background: transparent;");

        m_toggleDS18B20 = new ToggleSwitch(row);
        QLabel *status = new QLabel("关", row);
        status->setStyleSheet("font: 10pt \"Microsoft YaHei\"; color: #888888; background: transparent;");
        status->setMinimumWidth(24);
        status->setAlignment(Qt::AlignCenter);
        connect(m_toggleDS18B20, &ToggleSwitch::toggled, this, [this, status](bool on) {
            status->setText(on ? "开" : "关");
            status->setStyleSheet(on
                ? "font: 10pt \"Microsoft YaHei\"; color: #1b8a3d; background: transparent;"
                : "font: 10pt \"Microsoft YaHei\"; color: #999; background: transparent;");
            emit ds18b20Toggled(on);
        });

        h->addWidget(icon);
        h->addStretch();
        h->addWidget(status);
        h->addWidget(m_toggleDS18B20);
        layout->addWidget(row);

    }

    // --- BH1750 ---
    {
        QWidget *row = new QWidget(ui->sensorGroup);
        row->setStyleSheet("background: transparent;");
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(4, 4, 4, 4);

        QLabel *icon = new QLabel("BH1750", row);
        icon->setStyleSheet("font: bold 12pt \"Microsoft YaHei\"; color: #ffaa33; background: transparent;");

        m_toggleBH1750 = new ToggleSwitch(row);
        QLabel *status = new QLabel("关", row);
        status->setStyleSheet("font: 10pt \"Microsoft YaHei\"; color: #888888; background: transparent;");
        status->setMinimumWidth(24);
        status->setAlignment(Qt::AlignCenter);
        connect(m_toggleBH1750, &ToggleSwitch::toggled, this, [this, status](bool on) {
            status->setText(on ? "开" : "关");
            status->setStyleSheet(on
                ? "font: 10pt \"Microsoft YaHei\"; color: #1b8a3d; background: transparent;"
                : "font: 10pt \"Microsoft YaHei\"; color: #999; background: transparent;");
            emit bh1750Toggled(on);
        });

        h->addWidget(icon);
        h->addStretch();
        h->addWidget(status);
        h->addWidget(m_toggleBH1750);
        layout->addWidget(row);

    }

    // --- MH-Z19B ---
    {
        QWidget *row = new QWidget(ui->sensorGroup);
        row->setStyleSheet("background: transparent;");
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(4, 4, 4, 4);

        QLabel *icon = new QLabel("MH-Z19B", row);
        icon->setStyleSheet("font: bold 12pt \"Microsoft YaHei\"; color: #44dd55; background: transparent;");

        m_toggleMHZ19B = new ToggleSwitch(row);
        QLabel *status = new QLabel("关", row);
        status->setStyleSheet("font: 10pt \"Microsoft YaHei\"; color: #888888; background: transparent;");
        status->setMinimumWidth(24);
        status->setAlignment(Qt::AlignCenter);
        connect(m_toggleMHZ19B, &ToggleSwitch::toggled, this, [this, status](bool on) {
            status->setText(on ? "开" : "关");
            status->setStyleSheet(on
                ? "font: 10pt \"Microsoft YaHei\"; color: #1b8a3d; background: transparent;"
                : "font: 10pt \"Microsoft YaHei\"; color: #999; background: transparent;");
            emit mhz19bToggled(on);
        });

        h->addWidget(icon);
        h->addStretch();
        h->addWidget(status);
        h->addWidget(m_toggleMHZ19B);
        layout->addWidget(row);

    }
}

// ---------------------------------------------------------------------------
// 风扇 ON/OFF 圆角按钮
// ---------------------------------------------------------------------------
void control::setupFanButtons()
{
    // 找到 fanHeaderLayout
    QHBoxLayout *headerLayout = nullptr;
    QList<QHBoxLayout *> layouts = ui->fanFrame->findChildren<QHBoxLayout *>();
    for (auto *l : layouts) {
        if (l->indexOf(ui->fanIconLabel) >= 0) {
            headerLayout = l;
            break;
        }
    }
    if (!headerLayout) return;

    // 关闭按钮
    m_btnFanOff = new QPushButton("OFF", ui->fanFrame);
    m_btnFanOff->setFixedSize(60, 36);
    m_btnFanOff->setStyleSheet(
        "QPushButton {"
        "  font: bold 11pt \"Microsoft YaHei\";"
        "  color: #fff;"
        "  background: #b83030;"
        "  border: 1px solid #8f2020;"
        "  border-radius: 18px;"       // 圆角
        "}"
        "QPushButton:hover { background: #d43838; }"
        "QPushButton:pressed { background: #8f2020; }");

    // 开启按钮
    m_btnFanOn = new QPushButton("ON", ui->fanFrame);
    m_btnFanOn->setFixedSize(60, 36);
    m_btnFanOn->setStyleSheet(
        "QPushButton {"
        "  font: bold 11pt \"Microsoft YaHei\";"
        "  color: #fff;"
        "  background: #1b8a3d;"
        "  border: 1px solid #156b2f;"
        "  border-radius: 18px;"       // 圆角
        "}"
        "QPushButton:hover { background: #20a048; }"
        "QPushButton:pressed { background: #156b2f; }");

    connect(m_btnFanOff, &QPushButton::clicked, this, [this]() {
        m_fanOn = false;
        emit fanToggled(false);
    });

    connect(m_btnFanOn, &QPushButton::clicked, this, [this]() {
        m_fanOn = true;
        emit fanToggled(true);
    });

    headerLayout->addWidget(m_btnFanOff);
    headerLayout->addWidget(m_btnFanOn);
}

// ---------------------------------------------------------------------------
// 舵机 toggle 开关
// ---------------------------------------------------------------------------
void control::setupServoToggle()
{
    QList<QHBoxLayout *> layouts = ui->servoFrame->findChildren<QHBoxLayout *>();
    QHBoxLayout *headerLayout = nullptr;
    for (auto *l : layouts) {
        if (l->indexOf(ui->servoIconLabel) >= 0) {
            headerLayout = l;
            break;
        }
    }

    m_toggleServo = new ToggleSwitch(ui->servoFrame);
    connect(m_toggleServo, &ToggleSwitch::toggled, this, [this](bool on) {
        ui->servoSlider->setEnabled(on);
        if (!on) {
            ui->servoSlider->setValue(0);
            ui->servoAngleLabel->setText("0°");
        }
        emit servoToggled(on);
    });

    if (headerLayout) {
        headerLayout->addWidget(m_toggleServo);
    }
}

// ---------------------------------------------------------------------------
// 滑块回调
// ---------------------------------------------------------------------------
void control::onServoSliderChanged(int value)
{
    ui->servoAngleLabel->setText(QString::number(value) + "°");
    emit servoAngleChanged(value);
}

// ---------------------------------------------------------------------------
// 状态查询
// ---------------------------------------------------------------------------
bool control::isDS18B20Enabled() const { return m_toggleDS18B20->isChecked(); }
bool control::isBH1750Enabled()  const { return m_toggleBH1750->isChecked(); }
bool control::isMHZ19BEnabled()  const { return m_toggleMHZ19B->isChecked(); }
bool control::isFanOn()          const { return m_fanOn; }
bool control::isServoOn()        const { return m_toggleServo->isChecked(); }
int  control::fanSpeed()         const { return m_fanOn ? 100 : 0; }
int  control::servoAngle()       const { return ui->servoSlider->value(); }

void control::setDS18B20Enabled(bool enabled)
{
    m_toggleDS18B20->setChecked(enabled, false);
}

void control::setBH1750Enabled(bool enabled)
{
    m_toggleBH1750->setChecked(enabled, false);
}

void control::setMHZ19BEnabled(bool enabled)
{
    m_toggleMHZ19B->setChecked(enabled, false);
}
