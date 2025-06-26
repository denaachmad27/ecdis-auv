#ifndef AISTOOLTIP_H
#define AISTOOLTIP_H

#include <QFrame>
#include <QVBoxLayout>
#include <QPainter>

class AISTooltip : public QFrame
{
    Q_OBJECT
public:
    explicit AISTooltip(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // AISTOOLTIP_H
