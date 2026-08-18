#include "toggleswitch.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QtMath>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
    , m_checked(false)
    , m_thumbPos(0.0)
    , m_anim(nullptr)
{
    setFixedSize(76, 36);
    setCursor(Qt::PointingHandCursor);
}

bool ToggleSwitch::isChecked() const
{
    return m_checked;
}

void ToggleSwitch::setChecked(bool checked, bool animate)
{
    if (m_checked == checked && qFuzzyCompare(m_thumbPos, checked ? 1.0 : 0.0))
        return;

    m_checked = checked;

    if (m_anim && m_anim->state() == QAbstractAnimation::Running)
        m_anim->stop();

    if (animate) {
        if (!m_anim) {
            m_anim = new QPropertyAnimation(this, "thumbPosition", this);
            m_anim->setDuration(150);
        }
        m_anim->setStartValue(m_thumbPos);
        m_anim->setEndValue(checked ? 1.0 : 0.0);
        m_anim->start();
    } else {
        m_thumbPos = checked ? 1.0 : 0.0;
        update();
    }

    emit toggled(m_checked);
}

qreal ToggleSwitch::thumbPosition() const
{
    return m_thumbPos;
}

void ToggleSwitch::setThumbPosition(qreal pos)
{
    m_thumbPos = pos;
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(76, 36);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return QSize(60, 30);
}

// ---------------------------------------------------------------------------
// painting
// ---------------------------------------------------------------------------
void ToggleSwitch::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    qreal radius = h / 2.0;
    qreal thumbDiam = h - 6;       // thumb circle diameter
    qreal thumbRadius = thumbDiam / 2.0;

    // track bounding rect (full widget)
    QRectF trackRect(0, 0, w, h);

    // ---- track fill ----
    QColor trackColor;
    if (m_thumbPos > 0.01) {
        trackColor = QColor(0x2D, 0x7D, 0xD2);  // blue when ON
    } else {
        trackColor = QColor(0xBB, 0xBB, 0xBB);  // gray when OFF
    }
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(trackRect, radius, radius);

    // ---- track border ----
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1));
    p.drawRoundedRect(trackRect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // ---- thumb ----
    qreal thumbCenterX = thumbRadius + 3 + (w - thumbDiam - 6) * m_thumbPos;
    qreal thumbCenterY = h / 2.0;
    QRectF thumbRect(thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                     thumbDiam, thumbDiam);

    // thumb body (white)
    p.setBrush(Qt::white);
    p.setPen(QPen(QColor(0xAA, 0xAA, 0xAA), 1));
    p.drawEllipse(thumbRect);
}

// ---------------------------------------------------------------------------
// mouse interaction
// ---------------------------------------------------------------------------
void ToggleSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked, true);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
