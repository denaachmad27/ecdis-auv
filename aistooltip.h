#ifndef AISTOOLTIP_H
#define AISTOOLTIP_H

#include <QFrame>
#include <QVBoxLayout>
#include <QPainter>

class AISTooltip : public QFrame
{
    Q_OBJECT
public:
    explicit AISTooltip(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(1);
        setLayout(layout);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor bgColor(248, 248, 248, 128); // Transparan 50%
        p.setBrush(bgColor);
        p.setPen(QColor("#cccccc"));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
    }
};

#endif // AISTOOLTIP_H
