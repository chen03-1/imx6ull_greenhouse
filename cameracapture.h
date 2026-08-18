#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QString>

/// OV5640 摄像头 V4L2 采集类
/// 运行在独立线程中，定时抓帧，YUYV 转 RGB 后以 frameReady 信号发出
class CameraCapture : public QObject
{
    Q_OBJECT

public:
    explicit CameraCapture(QObject *parent = nullptr);
    ~CameraCapture();

    // ---- 配置 ----
    void setDevice(const QString &device);   // 默认 /dev/video1
    void setSize(int width, int height);     // 默认 640x480

    bool isRunning() const;

public slots:
    void start();
    void stop();

signals:
    void frameReady(const QImage &image);
    void stateChanged(bool running);
    void errorOccurred(const QString &message);

private slots:
    void captureOneFrame();

private:
    bool initDevice();
    void releaseDevice();
    QImage yuyvToQImage(const unsigned char *data, int w, int h);

    // ---- 配置 ----
    QString m_device = QStringLiteral("/dev/video1");
    int m_width  = 640;
    int m_height = 480;

    // ---- 采集 ----
    QTimer *m_timer;
    int     m_fd = -1;
    bool    m_running = false;

    // ---- mmap 缓冲区（4 个）----
    unsigned char *m_buffers[4]   = { nullptr, nullptr, nullptr, nullptr };
    unsigned int   m_bufLength[4] = { 0, 0, 0, 0 };
};

#endif // CAMERACAPTURE_H
