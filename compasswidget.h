#ifndef COMPASSWIDGET_H
#define COMPASSWIDGET_H

#include <QWidget>
#include <QPainter>

class CompassWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompassWidget(QWidget *parent = nullptr);

    void setHeading(double h); // heading kapal (derajat)

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double heading; // nilai heading (0-360 derajat)
};

#endif // COMPASSWIDGET_H
