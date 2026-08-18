#ifndef CURVEWIDGET_H
#define CURVEWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>

class CurveWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CurveWidget(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setYLabel(const QString &label);
    void setYUnit(const QString &unit);
    void setLineColor(const QColor &color);
    void setMaxPoints(int count);
    void setYRange(double min, double max);
    void setAutoRange(bool enabled);

public slots:
    void addDataPoint(double value);
    void addDataPoints(const QVector<double> &values);
    void clearData();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void recalcYRange();

    QString m_title;
    QString m_yLabel;
    QString m_yUnit;
    QColor  m_lineColor;

    QVector<double> m_data;
    int    m_maxPoints;
    double m_yMin;
    double m_yMax;
    bool   m_autoRange;

    // Y-axis tick parameters (computed in resizeEvent / recalcYRange)
    struct TickInfo {
        double minVal;
        double maxVal;
        double step;
        int    numTicks;
    };
    TickInfo m_ticks;

    // layout margins for drawing
    int m_marginLeft;
    int m_marginRight;
    int m_marginTop;
    int m_marginBottom;
};

#endif // CURVEWIDGET_H
