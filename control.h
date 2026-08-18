#ifndef CONTROL_H
#define CONTROL_H

#include <QWidget>
#include "toggleswitch.h"

class QSlider;
class QLabel;
class QPushButton;

namespace Ui {
class control;
}

class control : public QWidget
{
    Q_OBJECT

public:
    explicit control(QWidget *parent = nullptr);
    ~control();

    // ---- query switch states ----
    bool isDS18B20Enabled() const;
    bool isBH1750Enabled() const;
    bool isMHZ19BEnabled() const;
    bool isFanOn() const;
    bool isServoOn() const;
    int  fanSpeed() const;       // 风扇 ON=100, OFF=0
    int  servoAngle() const;     // 0-180

    // ---- programmatic toggle (emit signals) ----
    void setDS18B20Enabled(bool enabled);
    void setBH1750Enabled(bool enabled);
    void setMHZ19BEnabled(bool enabled);

signals:
    // ---- sensor toggles ----
    void ds18b20Toggled(bool enabled);
    void bh1750Toggled(bool enabled);
    void mhz19bToggled(bool enabled);

    // ---- actuator ----
    void fanToggled(bool on);
    void servoToggled(bool on);
    void servoAngleChanged(int angle);

    // ---- camera ----
    void cameraOpenRequested();
    void cameraCloseRequested();

private slots:
    void onServoSliderChanged(int value);

private:
    void setupSensorToggles();
    void setupFanButtons();
    void setupServoToggle();

    Ui::control *ui;

    // sensor toggle switches
    ToggleSwitch *m_toggleDS18B20;
    ToggleSwitch *m_toggleBH1750;
    ToggleSwitch *m_toggleMHZ19B;

    // fan ON/OFF buttons
    QPushButton *m_btnFanOn;
    QPushButton *m_btnFanOff;
    bool m_fanOn = false;

    // servo toggle
    ToggleSwitch *m_toggleServo;
};

#endif // CONTROL_H
