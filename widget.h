#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QButtonGroup>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class DataPage;
class control;
class camera;
class SensorWorker;
class CameraCapture;
class QThread;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void onNavButtonClicked(int id);
    void onCameraOpenRequested();
    void onCameraCloseRequested();

private:
    void setupUiPages();
    void setupNavigation();
    void setupCameraSwitching();
    void setupSensorWorker();
    void setupActuatorSync();

    Ui::Widget *ui;

    // sub-pages
    DataPage *m_dataPage;
    control *m_controlPage;
    camera  *m_cameraPage;

    QButtonGroup *m_navGroup;

    // hardware worker (runs on its own thread)
    SensorWorker *m_sensorWorker;
    QThread      *m_workerThread;

    // camera capture (runs on its own thread)
    CameraCapture *m_cameraCapture;
    QThread       *m_cameraThread;
};
#endif // WIDGET_H
