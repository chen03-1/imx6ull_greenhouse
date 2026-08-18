#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include <QPropertyAnimation>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal thumbPosition READ thumbPosition WRITE setThumbPosition)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked, bool animate = true);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    qreal thumbPosition() const;
    void setThumbPosition(qreal pos);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool  m_checked;
    qreal m_thumbPos;       // 0.0 = OFF (left), 1.0 = ON (right)
    QPropertyAnimation *m_anim;
};

#endif // TOGGLESWITCH_H
