#ifndef CAMERA_H
#define CAMERA_H

#include <QWidget>

class QImage;

namespace Ui {
class camera;
}

class camera : public QWidget
{
    Q_OBJECT

public:
    explicit camera(QWidget *parent = nullptr);
    ~camera();

public slots:
    /// 显示采集到的一帧图像
    void displayFrame(const QImage &image);
    /// 摄像头运行状态（开/关时更新提示文字）
    void setCameraState(bool running);

private:
    Ui::camera *ui;
};

#endif // CAMERA_H
