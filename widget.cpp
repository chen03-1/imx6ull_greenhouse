#include "widget.h"
#include "ui_widget.h"

#include "data.h"
#include "control.h"
#include "camera.h"
#include "sensorworker.h"
#include "cameracapture.h"

#include <QThread>
#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_sensorWorker(nullptr)
    , m_workerThread(nullptr)
    , m_cameraCapture(nullptr)
    , m_cameraThread(nullptr)
{
    ui->setupUi(this);

    // 固定窗口大小（7寸屏 1024×600）
    setFixedSize(1024, 600);

    setupUiPages();
    setupNavigation();
    setupCameraSwitching();
    setupSensorWorker();
    setupActuatorSync();

    // ---- 传感器默认开启（此时连接已就绪，信号不会丢失）----
    m_controlPage->setDS18B20Enabled(true);
    m_controlPage->setBH1750Enabled(true);
    m_controlPage->setMHZ19BEnabled(true);
}

Widget::~Widget()
{
    // 停止硬件采集线程
    if (m_sensorWorker) {
        m_sensorWorker->stop();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

    // 停止摄像头采集线程（stop 必须在采集线程内执行）
    if (m_cameraCapture) {
        QMetaObject::invokeMethod(m_cameraCapture, "stop", Qt::QueuedConnection);
    }
    if (m_cameraThread) {
        m_cameraThread->quit();
        m_cameraThread->wait(3000);
    }
    delete ui;
}

// ============================================================================
// 子页面创建
// ============================================================================

void Widget::setupUiPages()
{
    m_dataPage    = new DataPage(this);
    m_controlPage = new control(this);
    m_cameraPage  = new camera(this);

    ui->pageStack->addWidget(m_dataPage);     // index 0
    ui->pageStack->addWidget(m_controlPage);  // index 1
    ui->pageStack->addWidget(m_cameraPage);   // index 2

    ui->pageStack->setCurrentIndex(0);
}

// ============================================================================
// 底部导航
// ============================================================================

void Widget::setupNavigation()
{
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    m_navGroup->addButton(ui->btnNavData,    0);
    m_navGroup->addButton(ui->btnNavControl, 1);
    m_navGroup->addButton(ui->btnNavCamera,  2);

    connect(m_navGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &Widget::onNavButtonClicked);
}

// ============================================================================
// 摄像头切换
// ============================================================================

void Widget::setupCameraSwitching()
{
    connect(m_controlPage, &control::cameraOpenRequested,
            this, &Widget::onCameraOpenRequested);
    connect(m_controlPage, &control::cameraCloseRequested,
            this, &Widget::onCameraCloseRequested);

    // ---- OV5640 采集线程 ----
    m_cameraCapture = new CameraCapture();      // 无 parent，由 moveToThread 管理
    m_cameraThread  = new QThread(this);
    m_cameraCapture->moveToThread(m_cameraThread);
    m_cameraCapture->setSize(1024, 600);

    // 线程结束时清理采集对象
    connect(m_cameraThread, &QThread::finished,
            m_cameraCapture, &QObject::deleteLater);

    // 抓到的帧 → 摄像头页面显示
    connect(m_cameraCapture, &CameraCapture::frameReady,
            m_cameraPage, &camera::displayFrame);

    // 运行状态 → 页面提示文字
    connect(m_cameraCapture, &CameraCapture::stateChanged,
            m_cameraPage, &camera::setCameraState);

    // 出错时提示
    connect(m_cameraCapture, &CameraCapture::errorOccurred,
            this, [](const QString &msg) {
        qWarning() << "Camera error:" << msg;
    });

    // 控制页按钮 → 开始/停止采集（跨线程 queued 连接）
    connect(m_controlPage, &control::cameraOpenRequested,
            m_cameraCapture, &CameraCapture::start);
    connect(m_controlPage, &control::cameraCloseRequested,
            m_cameraCapture, &CameraCapture::stop);

    // 启动采集线程（默认不采集，等用户点击"打开摄像头"）
    m_cameraThread->start();
}

// ============================================================================
// 硬件采集线程
// ============================================================================

void Widget::setupSensorWorker()
{
    m_sensorWorker = new SensorWorker();        // 无 parent，由 moveToThread 管理
    m_workerThread = new QThread(this);

    m_sensorWorker->moveToThread(m_workerThread);

    // 线程启动后开始采集
    connect(m_workerThread, &QThread::started,
            m_sensorWorker, &SensorWorker::start);

    // 线程结束时清理 worker
    connect(m_workerThread, &QThread::finished,
            m_sensorWorker, &QObject::deleteLater);

    // ── control 传感器 toggle → SensorWorker ──
    connect(m_controlPage, &control::ds18b20Toggled,
            m_sensorWorker, &SensorWorker::enableDS18B20);
    connect(m_controlPage, &control::bh1750Toggled,
            m_sensorWorker, &SensorWorker::enableBH1750);
    connect(m_controlPage, &control::mhz19bToggled,
            m_sensorWorker, &SensorWorker::enableMHZ19B);

    // ── control 执行器 → SensorWorker ──
    connect(m_controlPage, &control::fanToggled, this, [this](bool on) {
        QMetaObject::invokeMethod(m_sensorWorker, [this, on]() {
            m_sensorWorker->setFanOutput(on, m_controlPage->fanSpeed());
        }, Qt::QueuedConnection);
    });
    connect(m_controlPage, &control::servoToggled, this, [this](bool on) {
        QMetaObject::invokeMethod(m_sensorWorker, [this, on]() {
            m_sensorWorker->setServoOutput(on, m_controlPage->servoAngle());
        }, Qt::QueuedConnection);
    });
    connect(m_controlPage, &control::servoAngleChanged, this, [this](int angle) {
        if (m_controlPage->isServoOn()) {
            QMetaObject::invokeMethod(m_sensorWorker, [this, angle]() {
                m_sensorWorker->setServoOutput(true, angle);
            }, Qt::QueuedConnection);
        }
    });

    // ── SensorWorker 传感器数据 → DataPage ──
    connect(m_sensorWorker, &SensorWorker::ds18b20ValueReady,
            m_dataPage, &DataPage::setDS18B20Value);
    connect(m_sensorWorker, &SensorWorker::bh1750ValueReady,
            m_dataPage, &DataPage::setBH1750Value);
    connect(m_sensorWorker, &SensorWorker::mhz19bValueReady,
            m_dataPage, &DataPage::setMHZ19BValue);

    // ── SensorWorker 连接状态 → DataPage ──
    connect(m_sensorWorker, &SensorWorker::ds18b20StatusChanged,
            m_dataPage, &DataPage::setDS18B20Status);
    connect(m_sensorWorker, &SensorWorker::bh1750StatusChanged,
            m_dataPage, &DataPage::setBH1750Status);
    connect(m_sensorWorker, &SensorWorker::mhz19bStatusChanged,
            m_dataPage, &DataPage::setMHZ19BStatus);

    // ── SensorWorker 执行器回读 → DataPage ──
    connect(m_sensorWorker, &SensorWorker::fanOutputApplied,
            m_dataPage, &DataPage::setFanStatus);
    connect(m_sensorWorker, &SensorWorker::servoOutputApplied,
            m_dataPage, &DataPage::setServoStatus);

    // ── 错误日志（调试用）──
    connect(m_sensorWorker, &SensorWorker::sensorError,
            this, [](const QString &name, const QString &err) {
        qWarning() << "[SensorWorker]" << name << ":" << err;
    });

    // ── 启动线程 ──
    m_workerThread->start();
}

// ============================================================================
// 执行器 UI 同步（control 滑块立即反映到 DataPage，不等硬件回读）
// ============================================================================

void Widget::setupActuatorSync()
{
    connect(m_controlPage, &control::fanToggled, this, [this](bool on) {
        m_dataPage->setFanStatus(on, m_controlPage->fanSpeed());
    });
    connect(m_controlPage, &control::servoToggled, this, [this](bool on) {
        m_dataPage->setServoStatus(on, m_controlPage->servoAngle());
    });
    connect(m_controlPage, &control::servoAngleChanged, this, [this](int angle) {
        m_dataPage->setServoStatus(m_controlPage->isServoOn(), angle);
    });
}

// ============================================================================
// 导航 / 摄像头 slot
// ============================================================================

void Widget::onNavButtonClicked(int id)
{
    ui->pageStack->setCurrentIndex(id);
}

void Widget::onCameraOpenRequested()
{
    ui->pageStack->setCurrentIndex(2);
    ui->btnNavCamera->setChecked(true);
}

void Widget::onCameraCloseRequested()
{
    ui->pageStack->setCurrentIndex(1);
    ui->btnNavControl->setChecked(true);
}
