#include "cameracapture.h"

#include <QDebug>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cstring>
#include <errno.h>

#include <linux/videodev2.h>

static const int BUFFER_COUNT = 4;

// ============================================================================
// 构造 / 析构
// ============================================================================

CameraCapture::CameraCapture(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))   // this 为 parent，moveToThread 时跟随移动
{
    connect(m_timer, &QTimer::timeout, this, &CameraCapture::captureOneFrame);
}

CameraCapture::~CameraCapture()
{
    stop();
}

// ============================================================================
// 配置
// ============================================================================

void CameraCapture::setDevice(const QString &device) { m_device = device; }
void CameraCapture::setSize(int width, int height)   { m_width = width; m_height = height; }

bool CameraCapture::isRunning() const { return m_running; }

// ============================================================================
// 启动 / 停止
// ============================================================================

void CameraCapture::start()
{
    if (m_running) return;
    qDebug() << "[Camera] initDevice...";
    if (!initDevice()) {
        qDebug() << "[Camera] initDevice FAILED";
        return;
    }

    m_running = true;
    m_timer->start(66);    // 15fps
    emit stateChanged(true);
    qDebug() << "[Camera] started OK, size:" << m_width << "x" << m_height;
}

void CameraCapture::stop()
{
    m_timer->stop();
    if (m_running) {
        m_running = false;
        emit stateChanged(false);
    }
    releaseDevice();
    qDebug() << "[Camera] stopped";
}

// ============================================================================
// 打开摄像头并初始化
// ============================================================================

bool CameraCapture::initDevice()
{
    // 1. 打开设备
    m_fd = ::open(m_device.toLatin1().constData(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        emit errorOccurred("无法打开 " + m_device);
        return false;
    }

    // 2. 设置格式：RGB565（摄像头硬件直接输出，跳过 YUV 转换）
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = m_width;
    fmt.fmt.pix.height      = m_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (::ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        emit errorOccurred("设置格式失败");
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // 驱动可能返回与请求不同的实际格式，以实际值为准
    m_width  = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
    qDebug().nospace() << "[Camera] format: " << m_width << "x" << m_height
                       << " fourcc: 0x"
                       << QString::number(fmt.fmt.pix.pixelformat, 16);

    // 3. 申请 4 个 mmap 缓冲区
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (::ioctl(m_fd, VIDIOC_REQBUFS, &req) < 0) {
        emit errorOccurred("申请缓冲区失败");
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // 4. mmap 每个缓冲区并入队
    for (unsigned int i = 0; i < req.count && i < BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (::ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
            break;

        m_bufLength[i] = buf.length;
        m_buffers[i] = (unsigned char *)::mmap(
            nullptr, buf.length,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            m_fd, buf.m.offset);

        if (m_buffers[i] == MAP_FAILED) {
            m_buffers[i] = nullptr;
            emit errorOccurred("mmap 失败");
            releaseDevice();
            return false;
        }

        ::ioctl(m_fd, VIDIOC_QBUF, &buf);   // 缓冲区入队
    }

    // 5. 开始采集流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(m_fd, VIDIOC_STREAMON, &type) < 0) {
        emit errorOccurred("启动采集流失败");
        releaseDevice();
        return false;
    }
    return true;
}

// ============================================================================
// 定时抓一帧
// ============================================================================

void CameraCapture::captureOneFrame()
{
    if (m_fd < 0 || !m_running) return;

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // 出队：拿到一帧数据
    if (::ioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) return;   // 还没有新帧
        emit errorOccurred("出队失败");
        return;
    }

    // 摄像头已直接输出 RGB565，只需包成 QImage 再拷贝一份
    if (buf.index < BUFFER_COUNT && m_buffers[buf.index]) {
        QImage img(m_buffers[buf.index], m_width, m_height, QImage::Format_RGB16);
        emit frameReady(img.copy());   // 必须 copy：V4L2 缓冲马上被下一帧覆盖
    }

    // 每 30 帧打印一次计数
    static int frameCount = 0;
    if (++frameCount % 30 == 0)
        qDebug() << "[Camera] frames captured:" << frameCount;

    // 重新入队，供下次采集
    ::ioctl(m_fd, VIDIOC_QBUF, &buf);
}

// ============================================================================
// YUYV → RGB888（scanLine 直接写内存，每帧独立缓冲，无跨线程共享）
// ============================================================================
QImage CameraCapture::yuyvToQImage(const unsigned char *data, int w, int h)
{
    QImage img(w, h, QImage::Format_RGB888);   // 每帧新建，避免共享竞争

    const unsigned char *src = data;

    for (int y = 0; y < h; y++) {
        uchar *dst = img.scanLine(y);          // 直接拿行内存指针
        for (int x = 0; x < w; x += 2) {
            int y0 = src[0] - 16;
            int u  = src[1] - 128;
            int y1 = src[2] - 16;
            int v  = src[3] - 128;
            src += 4;

            // YUV → RGB（+128 四舍五入，保证亮度正确）
            int r0 = (298*y0 + 409*v + 128) >> 8;
            int g0 = (298*y0 - 100*u - 208*v + 128) >> 8;
            int b0 = (298*y0 + 516*u + 128) >> 8;

            int r1 = (298*y1 + 409*v + 128) >> 8;
            int g1 = (298*y1 - 100*u - 208*v + 128) >> 8;
            int b1 = (298*y1 + 516*u + 128) >> 8;

            auto clamp = [](int val) {
                return val < 0 ? 0 : (val > 255 ? 255 : val);
            };

            // 像素 1（3 字节 RGB）
            dst[0] = clamp(r0);
            dst[1] = clamp(g0);
            dst[2] = clamp(b0);

            // 像素 2（共用同一组 UV）
            dst[3] = clamp(r1);
            dst[4] = clamp(g1);
            dst[5] = clamp(b1);

            dst += 6;
        }
    }
    return img;
}

// ============================================================================
// 释放设备
// ============================================================================

void CameraCapture::releaseDevice()
{
    if (m_fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ::ioctl(m_fd, VIDIOC_STREAMOFF, &type);

        for (int i = 0; i < BUFFER_COUNT; i++) {
            if (m_buffers[i])
                ::munmap(m_buffers[i], m_bufLength[i]);
            m_buffers[i] = nullptr;
            m_bufLength[i] = 0;
        }
        ::close(m_fd);
        m_fd = -1;
    }
}
