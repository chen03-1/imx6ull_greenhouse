#ifndef SENSORWORKER_H
#define SENSORWORKER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QByteArray>

class QSocketNotifier;

class SensorWorker : public QObject
{
    Q_OBJECT

public:
    explicit SensorWorker(QObject *parent = nullptr);
    ~SensorWorker();

    // ---- 设置硬件路径 / 设备节点 ----
    void setDS18B20DevicePath(const QString &path);
    void setBH1750I2CDevice(const QString &path);
    void setMHZ19BUARTDevice(const QString &path);
    void setFanGpioPath(const QString &path);
    void setServoPwmPath(const QString &path);

    // ---- 采集周期 (ms) ----
    void setInterval(int msec);

    // ---- 传感器使能查询 ----
    bool isDS18B20Enabled() const;
    bool isBH1750Enabled() const;
    bool isMHZ19BEnabled() const;
    bool isFanOn() const;
    bool isServoOn() const;
    int  fanSpeed() const;
    int  servoAngle() const;

public slots:
    // ---- 由 control 页触发，控制各传感器启停 ----
    void enableDS18B20(bool enabled);
    void enableBH1750(bool enabled);
    void enableMHZ19B(bool enabled);

    // ---- 执行器控制 ----
    void setFanOutput(bool on, int speedPercent);
    void setServoOutput(bool on, int angle);

    // ---- 采集生命周期 ----
    void start();
    void stop();

signals:
    // ---- 传感器数据 → DataPage ----
    void ds18b20ValueReady(double temperature);
    void bh1750ValueReady(double lux);
    void mhz19bValueReady(double co2ppm);

    // ---- 传感器连接状态 → DataPage ----
    void ds18b20StatusChanged(bool connected);
    void bh1750StatusChanged(bool connected);
    void mhz19bStatusChanged(bool connected);

    // ---- 执行器实际状态 → DataPage（回读确认）----
    void fanOutputApplied(bool on, int speedPercent);
    void servoOutputApplied(bool on, int angle);

    // ---- 错误信息 ----
    void sensorError(const QString &sensorName, const QString &error);

private slots:
    void onTimerTick();
    void onStdinRead();

private:
    // ========== 硬件读取接口 ==========

    /// DS18B20: 读 /sys/class/misc/ds18b20/value
    /// 返回温度 °C；失败返回 -999.0
    double readDS18B20();

    /// BH1750: 通过 /dev/i2c-1 读光照 (I2C addr 0x23)
    /// 返回 Lux；失败返回 -1.0
    double readBH1750();

    /// MH-Z19B: 通过 UART 读 CO2 (9600 8N1)
    /// 返回 ppm；失败返回 -1.0
    double readMHZ19B();

    /// 打开 MH-Z19B 串口并配置
    bool openMHZ19B();
    /// 关闭 MH-Z19B 串口
    void closeMHZ19B();

    /// 打开风扇 GPIO
    bool openFanGpio();
    /// 关闭风扇 GPIO
    void closeFanGpio();
    /// 写风扇 GPIO (on="1", off="0")
    bool writeFanGpio(bool on);

    /// 写舵机 PWM
    bool writeServoPwm(bool on, int angle);

    /// 打印当前传感器信息到 stdout
    void printSensorInfo();

    // ========== 成员变量 ==========

    // ---- 定时器 ----
    QTimer *m_timer;

    // ---- stdin 命令监听 ----
    QSocketNotifier *m_stdinNotifier;
    QByteArray       m_stdinBuffer;

    // ---- 硬件路径 ----
    QString m_ds18b20Path;
    QString m_bh1750I2C;
    QString m_mhz19bUART;
    QString m_fanGpioPath;
    QString m_servoPwmPath;

    // ---- MH-Z19B UART fd（保持打开，避免每次重新配置）----
    int m_mhz19bFd = -1;

    // ---- 风扇 GPIO fd ----
    int m_fanFd = -1;

    // ---- 各传感器使能标志 ----
    bool m_ds18b20Enabled  = false;
    bool m_bh1750Enabled   = false;
    bool m_mhz19bEnabled   = false;

    // ---- 执行器目标状态 ----
    bool m_fanOn      = false;
    int  m_fanSpeed   = 0;
    bool m_servoOn    = false;
    int  m_servoAngle = 0;

    // ---- 连接状态追踪 ----
    bool m_ds18b20Connected = false;
    bool m_bh1750Connected  = false;
    bool m_mhz19bConnected  = false;

    // ---- 最新读数缓存（供 search sensor 命令查询）----
    double m_lastTemp = -999.0;
    double m_lastLux  = -999.0;
    double m_lastCo2  = -999.0;

    // ---- 定时打印计数器（每 40s 打印一次）----
    int m_tickCount = 0;
};

#endif // SENSORWORKER_H
