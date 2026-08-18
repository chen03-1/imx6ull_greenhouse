#ifndef DATAPAGE_H
#define DATAPAGE_H

#include <QWidget>
#include <QButtonGroup>
#include "curvewidget.h"

namespace Ui {
class DataPage;
}

class DataPage : public QWidget
{
    Q_OBJECT

public:
    explicit DataPage(QWidget *parent = nullptr);
    ~DataPage();

public slots:
    // ---- sensor value updates (thread-safe via queued connections) ----
    void setDS18B20Value(double temperature);
    void setBH1750Value(double lux);
    void setMHZ19BValue(double co2ppm);

    // ---- sensor connection status ----
    void setDS18B20Status(bool connected);
    void setBH1750Status(bool connected);
    void setMHZ19BStatus(bool connected);

    // ---- fan / servo status ----
    void setFanStatus(bool on, int speedPercent);
    void setServoStatus(bool on, int angle);

private slots:
    void onSensorSelectorChanged(int id);

private:
    void setupChartWidgets();
    void switchChartTo(int sensorId);

    Ui::DataPage *ui;

    // three independent curve widgets, one per sensor
    CurveWidget *m_tempChart;   // DS18B20
    CurveWidget *m_luxChart;    // BH1750
    CurveWidget *m_co2Chart;    // MH-Z19B

    QButtonGroup *m_sensorGroup;
};

#endif // DATAPAGE_H
