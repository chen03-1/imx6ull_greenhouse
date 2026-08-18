#include "curvewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QtMath>

CurveWidget::CurveWidget(QWidget *parent)
    : QWidget(parent)
    , m_title("Curve")
    , m_yLabel("Value")
    , m_yUnit("")
    , m_lineColor(0x00, 0xAA, 0xFF)   // blue
    , m_maxPoints(100)
    , m_yMin(0)
    , m_yMax(100)
    , m_autoRange(true)
{
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);

    // initialize margins
    m_marginLeft   = 60;
    m_marginRight  = 20;
    m_marginTop    = 30;
    m_marginBottom = 40;

    m_ticks = {0, 100, 20, 5};
}

void CurveWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void CurveWidget::setYLabel(const QString &label)
{
    m_yLabel = label;
    update();
}

void CurveWidget::setYUnit(const QString &unit)
{
    m_yUnit = unit;
    update();
}

void CurveWidget::setLineColor(const QColor &color)
{
    m_lineColor = color;
    update();
}

void CurveWidget::setMaxPoints(int count)
{
    m_maxPoints = qMax(10, count);
    while (m_data.size() > m_maxPoints)
        m_data.removeFirst();
    if (m_autoRange) recalcYRange();
    update();
}

void CurveWidget::setYRange(double min, double max)
{
    m_autoRange = false;
    m_yMin = min;
    m_yMax = max;
    recalcYRange();
    update();
}

void CurveWidget::setAutoRange(bool enabled)
{
    m_autoRange = enabled;
    if (enabled) recalcYRange();
    update();
}

void CurveWidget::addDataPoint(double value)
{
    m_data.append(value);
    while (m_data.size() > m_maxPoints)
        m_data.removeFirst();

    if (m_autoRange) {
        bool changed = false;
        if (value < m_yMin) { m_yMin = value; changed = true; }
        if (value > m_yMax) { m_yMax = value; changed = true; }
        // also periodically shrink range if data is stable
        if (m_data.size() % 20 == 0) { recalcYRange(); changed = true; }
        if (changed) recalcYRange();
    }
    update();
}

void CurveWidget::addDataPoints(const QVector<double> &values)
{
    for (double v : values)
        m_data.append(v);
    while (m_data.size() > m_maxPoints)
        m_data.removeFirst();
    if (m_autoRange) recalcYRange();
    update();
}

void CurveWidget::clearData()
{
    m_data.clear();
    if (m_autoRange) {
        m_yMin = 0;
        m_yMax = 100;
        recalcYRange();
    }
    update();
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------
void CurveWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();

    // ---- background ----
    p.fillRect(rect(), QColor(0xF5, 0xF5, 0xF5));

    int plotLeft   = m_marginLeft;
    int plotRight  = w - m_marginRight;
    int plotTop    = m_marginTop;
    int plotBottom = h - m_marginBottom;
    int plotW = plotRight - plotLeft;
    int plotH = plotBottom - plotTop;

    if (plotW <= 0 || plotH <= 0) return;

    QRect plotRect(plotLeft, plotTop, plotW, plotH);

    // ---- plot area background ----
    p.fillRect(plotRect, Qt::white);

    // ---- grid lines ----
    p.save();
    p.setPen(QPen(QColor(0xDD, 0xDD, 0xDD), 1, Qt::DotLine));
    const TickInfo &tk = m_ticks;
    if (tk.step > 0 && tk.numTicks > 0) {
        for (int i = 0; i <= tk.numTicks; i++) {
            double val = tk.minVal + i * tk.step;
            double y = plotBottom - (val - tk.minVal) / (tk.maxVal - tk.minVal) * plotH;
            if (y >= plotTop && y <= plotBottom) {
                p.drawLine(plotLeft, (int)y, plotRight, (int)y);
            }
        }
    }
    p.restore();

    // ---- Y axis labels ----
    p.save();
    QFont yFont("Consolas", 9);
    p.setFont(yFont);
    p.setPen(QColor(0x66, 0x66, 0x66));
    if (tk.step > 0 && tk.numTicks > 0) {
        for (int i = 0; i <= tk.numTicks; i++) {
            double val = tk.minVal + i * tk.step;
            double y = plotBottom - (val - tk.minVal) / (tk.maxVal - tk.minVal) * plotH;
            if (y >= plotTop && y <= plotBottom) {
                QString lbl = QString::number(val, 'f', 1);
                QRect textRect(0, (int)y - 10, plotLeft - 6, 20);
                p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, lbl);
            }
        }
    }
    QFont unitFont("Microsoft YaHei", 9);
    p.setFont(unitFont);
    p.setPen(QColor(0x99, 0x99, 0x99));
    p.drawText(2, plotTop, plotLeft - 4, 16, Qt::AlignRight | Qt::AlignVCenter, m_yUnit);
    p.restore();

    // ---- X axis time labels ----
    p.save();
    QFont xFont("Consolas", 8);
    p.setFont(xFont);
    p.setPen(QColor(0x99, 0x99, 0x99));
    if (m_data.size() >= 2) {
        int numLabels = qMin(5, plotW / 60);
        if (numLabels < 2) numLabels = 2;
        for (int i = 0; i <= numLabels; i++) {
            int idx = m_data.size() - 1 - (m_data.size() - 1) * i / numLabels;
            if (idx < 0) idx = 0;
            int x = plotLeft + plotW * i / numLabels;
            QString lbl = QString("%1s").arg(i * (m_data.size() - 1) / numLabels);
            p.drawText(x - 25, plotBottom + 4, 50, 16, Qt::AlignCenter, lbl);
        }
    }
    p.restore();

    // ---- title ----
    p.save();
    QFont titleFont("Microsoft YaHei", 10, QFont::Bold);
    p.setFont(titleFont);
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(plotLeft, 2, plotW, m_marginTop - 4, Qt::AlignLeft | Qt::AlignVCenter, m_title);
    p.restore();

    // ---- border around plot area ----
    p.setPen(QPen(QColor(0xBB, 0xBB, 0xBB), 1));
    p.drawRect(plotRect);

    // ---- data line ----
    if (m_data.size() >= 2) {
        p.save();
        QPen linePen(m_lineColor, 2);
        p.setPen(linePen);
        p.setClipRect(plotRect.adjusted(1, 1, -1, -1));

        double yRange = tk.maxVal - tk.minVal;
        if (yRange <= 0) yRange = 1;

        QPainterPath linePath;
        double x0 = plotLeft + (double)0 / (m_maxPoints - 1) * plotW;
        double y0 = plotBottom - (m_data[0] - tk.minVal) / yRange * plotH;
        linePath.moveTo(x0, qBound((double)plotTop, y0, (double)plotBottom));
        for (int i = 1; i < m_data.size(); i++) {
            double xi = plotLeft + (double)i / (m_maxPoints - 1) * plotW;
            double yi = plotBottom - (m_data[i] - tk.minVal) / yRange * plotH;
            linePath.lineTo(xi, qBound((double)plotTop, yi, (double)plotBottom));
        }
        p.drawPath(linePath);

        p.restore();
    } else {
        p.save();
        p.setPen(QColor(0xAA, 0xAA, 0xAA));
        QFont hintFont("Microsoft YaHei", 12);
        p.setFont(hintFont);
        p.drawText(plotRect, Qt::AlignCenter, "等待数据...");
        p.restore();
    }
}

void CurveWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    recalcYRange();
}

void CurveWidget::recalcYRange()
{
    if (m_data.isEmpty()) {
        m_ticks = {m_yMin, m_yMax, (m_yMax - m_yMin) / 5.0, 5};
        return;
    }

    double minY = m_yMin;
    double maxY = m_yMax;

    if (m_autoRange && !m_data.isEmpty()) {
        minY = m_data[0];
        maxY = m_data[0];
        for (double v : m_data) {
            if (v < minY) minY = v;
            if (v > maxY) maxY = v;
        }
        // add 10% padding
        double pad = (maxY - minY) * 0.1;
        if (pad < 1.0) pad = 1.0;
        minY -= pad;
        maxY += pad;
        if (minY < 0 && maxY > 0) {
            // keep zero visible if range spans it
        }
    }

    // ensure valid range
    if (maxY - minY < 1.0) {
        double mid = (minY + maxY) / 2.0;
        minY = mid - 5;
        maxY = mid + 5;
    }

    // compute nice tick step
    double rawStep = (maxY - minY) / 5.0;
    double magnitude = qPow(10, qFloor(qLn(rawStep) / qLn(10.0)));
    double residual = rawStep / magnitude;
    double niceStep;
    if (residual <= 1.5)      niceStep = 1.0 * magnitude;
    else if (residual <= 3.5) niceStep = 2.0 * magnitude;
    else if (residual <= 7.5) niceStep = 5.0 * magnitude;
    else                      niceStep = 10.0 * magnitude;

    double niceMin = qFloor(minY / niceStep) * niceStep;
    double niceMax = qCeil(maxY / niceStep) * niceStep;

    int numTicks = qRound((niceMax - niceMin) / niceStep);
    if (numTicks > 10) { niceStep *= 2; numTicks /= 2; }
    if (numTicks < 2)  { niceStep /= 2; numTicks *= 2; }

    m_ticks = {niceMin, niceMax, niceStep, numTicks};
}
