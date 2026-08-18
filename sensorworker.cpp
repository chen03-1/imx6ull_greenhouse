#include "sensorworker.h"
#include <QtMath>
#include <QDebug>
#include <QFile>
#include <QSocketNotifier>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>
#include <linux/i2c-dev.h>

static const char *CMD_FIFO_PATH = "/tmp/sensor_cmd";

// ============================================================================
// 构造 / 析构
// ============================================================================

SensorWorker::SensorWorker(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))  // this 为 parent，moveToThread 时跟随移动
    , m_stdinNotifier(nullptr)
{
    // 默认硬件路径（可按实际设备树调整）
    m_ds18b20Path  = QStringLiteral("/sys/class/misc/ds18b20/value");
    m_bh1750I2C    = QStringLiteral("/dev/i2c-1");
    m_mhz19bUART   = QStringLiteral("/dev/ttymxc2");
    m_fanGpioPath  = QStringLiteral("/sys/class/gpio/gpio3/value");
    m_servoPwmPath = QStringLiteral("/sys/class/pwm/pwmchip0/pwm0/");

    connect(m_timer, &QTimer::timeout, this, &SensorWorker::onTimerTick);

    // ---- 命名管道命令监听 ----
    // 串口端: echo "search sensor" > /tmp/sensor_cmd
    ::mkfifo(CMD_FIFO_PATH, 0666);
    int fifoFd = ::open(CMD_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fifoFd >= 0) {
        m_stdinNotifier = new QSocketNotifier(fifoFd, QSocketNotifier::Read, this);
        connect(m_stdinNotifier, &QSocketNotifier::activated,
                this, &SensorWorker::onStdinRead);

        // 先读掉 open 时可能残留的数据
        char dummy[256];
        while (::read(fifoFd, dummy, sizeof(dummy)) > 0) {}
    } else {
        qWarning() << "[SensorWorker] cannot open FIFO" << CMD_FIFO_PATH
                   << "errno:" << errno;
    }
}

SensorWorker::~SensorWorker()
{
    stop();

    closeMHZ19B();   // 关闭串口
    closeFanGpio();  // 关闭风扇 GPIO

    // 关闭 FIFO
    if (m_stdinNotifier) {
        int fd = m_stdinNotifier->socket();
        if (fd >= 0)
            ::close(fd);
        delete m_stdinNotifier;
        m_stdinNotifier = nullptr;
    }
    ::unlink(CMD_FIFO_PATH);
}

// ============================================================================
// 公共配置接口
// ============================================================================

void SensorWorker::setDS18B20DevicePath(const QString &path)  { m_ds18b20Path  = path; }
void SensorWorker::setBH1750I2CDevice(const QString &path)    { m_bh1750I2C    = path; }
void SensorWorker::setMHZ19BUARTDevice(const QString &path)   { m_mhz19bUART   = path; }
void SensorWorker::setFanGpioPath(const QString &path)         { m_fanGpioPath  = path; }
void SensorWorker::setServoPwmPath(const QString &path)        { m_servoPwmPath = path; }

void SensorWorker::setInterval(int msec)
{
    if (m_timer->isActive())
        m_timer->setInterval(msec);
    else
        m_timer->setInterval(msec);
}

// ============================================================================
// 生命周期
// ============================================================================

void SensorWorker::start()
{
    if (m_timer->interval() <= 0)
        m_timer->setInterval(500);   // 默认 500ms 采集周期

    m_timer->start();
    qDebug() << "[SensorWorker] started, interval =" << m_timer->interval() << "ms";
}

void SensorWorker::stop()
{
    m_timer->stop();
    qDebug() << "[SensorWorker] stopped";
}

// ============================================================================
// 传感器使能 slot
// ============================================================================

void SensorWorker::enableDS18B20(bool enabled)
{
    m_ds18b20Enabled = enabled;
    if (!enabled) {
        if (m_ds18b20Connected) {
            m_ds18b20Connected = false;
            emit ds18b20StatusChanged(false);
        }
    }
}

void SensorWorker::enableBH1750(bool enabled)
{
    m_bh1750Enabled = enabled;
    if (!enabled) {
        if (m_bh1750Connected) {
            m_bh1750Connected = false;
            emit bh1750StatusChanged(false);
        }
    }
}

void SensorWorker::enableMHZ19B(bool enabled)
{
    m_mhz19bEnabled = enabled;
    if (enabled) {
        openMHZ19B();   // 打开并配置串口
    } else {
        closeMHZ19B();
        if (m_mhz19bConnected) {
            m_mhz19bConnected = false;
            emit mhz19bStatusChanged(false);
        }
    }
}

// ============================================================================
// 执行器控制 slot
// ============================================================================

void SensorWorker::setFanOutput(bool on, int speedPercent)
{
    Q_UNUSED(speedPercent);  // GPIO 风扇仅支持开关，无调速

    m_fanOn    = on;
    m_fanSpeed = on ? 100 : 0;

    if (on)
        openFanGpio();   // 首次使用时打开

    bool ok = writeFanGpio(on);
    if (!ok) {
        emit sensorError("Fan", "GPIO 写入失败");
    }
    emit fanOutputApplied(m_fanOn, m_fanSpeed);
}

void SensorWorker::setServoOutput(bool on, int angle)
{
    m_servoOn    = on;
    m_servoAngle = on ? qBound(0, angle, 180) : 0;

    bool ok = writeServoPwm(m_servoOn, m_servoAngle);
    if (!ok) {
        emit sensorError("Servo", "PWM 写入失败");
    }
    emit servoOutputApplied(m_servoOn, m_servoAngle);
}

// ============================================================================
// 状态查询
// ============================================================================

bool SensorWorker::isDS18B20Enabled() const { return m_ds18b20Enabled; }
bool SensorWorker::isBH1750Enabled()  const { return m_bh1750Enabled; }
bool SensorWorker::isMHZ19BEnabled()  const { return m_mhz19bEnabled; }
bool SensorWorker::isFanOn()          const { return m_fanOn; }
bool SensorWorker::isServoOn()        const { return m_servoOn; }
int  SensorWorker::fanSpeed()         const { return m_fanSpeed; }
int  SensorWorker::servoAngle()       const { return m_servoAngle; }

// ============================================================================
// 定时器回调 —— 驱动所有传感器采集
// ============================================================================

void SensorWorker::onTimerTick()
{
    // --- DS18B20 ---
    if (m_ds18b20Enabled) {
        double t = readDS18B20();
        bool ok = (t > -100.0);   // 失败返回 -999.0，有效温度 > -55°C
        if (ok) {
            m_lastTemp = t;
            emit ds18b20ValueReady(t);
        }
        if (ok != m_ds18b20Connected) {
            m_ds18b20Connected = ok;
            emit ds18b20StatusChanged(ok);
        }
    }

    // --- BH1750 ---
    if (m_bh1750Enabled) {
        double lux = readBH1750();
        bool ok = (lux >= 0.0);   // 失败返回 -1.0，有效照度 >= 0
        if (ok) {
            m_lastLux = lux;
            emit bh1750ValueReady(lux);
        }
        if (ok != m_bh1750Connected) {
            m_bh1750Connected = ok;
            emit bh1750StatusChanged(ok);
        }
    }

    // --- MH-Z19B ---
    if (m_mhz19bEnabled) {
        double co2 = readMHZ19B();
        bool ok = (co2 >= 0.0);   // 失败返回 -1.0，有效 CO2 >= 0
        if (ok) {
            m_lastCo2 = co2;
            emit mhz19bValueReady(co2);
        }
        if (ok != m_mhz19bConnected) {
            m_mhz19bConnected = ok;
            emit mhz19bStatusChanged(ok);
        }
    }

    // 每 80 tick（40s）自动打印一次传感器状态
    m_tickCount++;
    if (m_tickCount >= 80) {
        m_tickCount = 0;
        printSensorInfo();
    }
}

// ============================================================================
// stdin 命令处理 —— 输入 "search sensor" 打印当前传感器信息
// ============================================================================

void SensorWorker::onStdinRead()
{
    int fd = m_stdinNotifier ? m_stdinNotifier->socket() : -1;
    if (fd < 0) return;

    char buf[256];
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';
    m_stdinBuffer.append(buf);

    // 逐行处理
    int idx;
    while ((idx = m_stdinBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_stdinBuffer.left(idx).trimmed();
        m_stdinBuffer.remove(0, idx + 1);

        if (line == "search sensor") {
            printSensorInfo();
        }
    }
}

void SensorWorker::printSensorInfo()
{
    qDebug() << "========== Sensor Status ==========";

    qDebug().noquote()
        << QString("DS18B20  | %1 °C  | %2")
           .arg(m_ds18b20Connected
                ? QString::number(m_lastTemp, 'f', 1)
                : QString("--"))
           .arg(m_ds18b20Connected ? "connected" : "disconnected");

    qDebug().noquote()
        << QString("BH1750   | %1 Lux | %2")
           .arg(m_bh1750Connected
                ? QString::number(m_lastLux, 'f', 1)
                : QString("--"))
           .arg(m_bh1750Connected ? "connected" : "disconnected");

    qDebug().noquote()
        << QString("MH-Z19B  | %1 ppm | %2")
           .arg(m_mhz19bConnected
                ? QString::number((int)m_lastCo2)
                : QString("--"))
           .arg(m_mhz19bConnected ? "connected" : "disconnected");

    qDebug() << "====================================";
}

// ============================================================================
// 硬件读取接口 (TODO: 填入真实硬件访问代码)
// ============================================================================

double SensorWorker::readDS18B20()
{
    QFile file(m_ds18b20Path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -999.0;

    QString content = file.readAll().trimmed();
    file.close();

    bool ok = false;
    int raw = content.toInt(&ok);
    if (!ok)
        return -999.0;

    return raw / 10000.0;  // 转换为 °C
}

double SensorWorker::readBH1750()
{
    QByteArray devPath = m_bh1750I2C.toLatin1();
    int fd = ::open(devPath.constData(), O_RDWR);
    if (fd < 0)
        return -1.0;

    if (::ioctl(fd, I2C_SLAVE, 0x23) < 0) {
        ::close(fd);
        return -1.0;
    }

    // 上电 + 连续高分辨率模式
    char cmd = 0x01;
    ::write(fd, &cmd, 1);
    cmd = 0x10;
    ::write(fd, &cmd, 1);
    ::usleep(180000);

    // 读取 2 字节（高字节在前）
    unsigned char buf[2] = {0};
    ssize_t n = ::read(fd, buf, 2);
    ::close(fd);

    if (n != 2)
        return -1.0;

    int raw = (buf[0] << 8) | buf[1];  // buf[0]=高, buf[1]=低
    return raw / 1.2;
}

bool SensorWorker::openMHZ19B()
{
    if (m_mhz19bFd >= 0) return true;  // already open

    QByteArray devPath = m_mhz19bUART.toLatin1();
    m_mhz19bFd = ::open(devPath.constData(), O_RDWR | O_NOCTTY);
    if (m_mhz19bFd < 0) {
        qWarning() << "[SensorWorker] MH-Z19B: cannot open" << m_mhz19bUART;
        return false;
    }

    struct termios tio;
    ::tcgetattr(m_mhz19bFd, &tio);
    ::cfsetospeed(&tio, B9600);
    ::cfsetispeed(&tio, B9600);
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    ::tcsetattr(m_mhz19bFd, TCSANOW, &tio);

    return true;
}

void SensorWorker::closeMHZ19B()
{
    if (m_mhz19bFd >= 0) {
        ::close(m_mhz19bFd);
        m_mhz19bFd = -1;
    }
}

double SensorWorker::readMHZ19B()
{
    if (m_mhz19bFd < 0)
        return -1.0;

    // 查询命令: FF 01 86 00 00 00 00 00 79
    uint8_t cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    if (::write(m_mhz19bFd, cmd, 9) != 9)
        return -1.0;

    ::usleep(500000);  // 等待传感器应答

    uint8_t resp[9] = {0};
    ssize_t n = ::read(m_mhz19bFd, resp, sizeof(resp));
    if (n < 9 || resp[0] != 0xFF || resp[1] != 0x86)
        return -1.0;

    return (resp[2] << 8) | resp[3];  // CO2 ppm
}

bool SensorWorker::openFanGpio()
{
    if (m_fanFd >= 0) return true;

    QByteArray devPath = m_fanGpioPath.toLatin1();
    m_fanFd = ::open(devPath.constData(), O_WRONLY);
    if (m_fanFd < 0) {
        qWarning() << "[SensorWorker] Fan: cannot open" << m_fanGpioPath;
        return false;
    }
    return true;
}

void SensorWorker::closeFanGpio()
{
    if (m_fanFd >= 0) {
        ::close(m_fanFd);
        m_fanFd = -1;
    }
}

bool SensorWorker::writeFanGpio(bool on)
{
    if (m_fanFd < 0) return false;
    const char *val = on ? "1" : "0";
    return ::write(m_fanFd, val, 1) == 1;
}

bool SensorWorker::writeServoPwm(bool on, int angle)
{
    Q_UNUSED(on);
    Q_UNUSED(angle);
    // TODO: 写 /sys/class/pwm/xxx/
    return true;
}
