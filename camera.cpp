#include "camera.h"
#include "ui_camera.h"

#include <QLabel>
#include <QPixmap>
#include <QDebug>

camera::camera(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::camera)
{
    ui->setupUi(this);
}

camera::~camera()
{
    delete ui;
}

// ============================================================================
// 显示一帧图像
// ============================================================================

void camera::displayFrame(const QImage &image)
{
    static int shown = 0;
    if (++shown % 30 == 0)
        qDebug() << "[CameraPage] displayFrame size:" << image.size();

    if (image.isNull()) return;

    // 缩放至显示区域并保持比例（快速算法，降低 CPU 开销）
    QPixmap pix = QPixmap::fromImage(image.scaled(
        ui->videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    ui->videoLabel->setPixmap(pix);
}

// ============================================================================
// 运行状态提示
// ============================================================================

void camera::setCameraState(bool running)
{
    if (!running)
        ui->videoLabel->clear();   // 清除最后一帧画面

    ui->videoLabel->setText(running ? "摄像头已打开，等待画面..." : "无视频信号");
}
