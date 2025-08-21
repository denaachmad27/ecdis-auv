#ifndef COMPASSWIDGET_H
#define COMPASSWIDGET_H

#include <QWidget>
#include <QPainter>

class CompassWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompassWidget(QWidget *parent = nullptr);

    void setHeading(double h); // heading kapal (derajat)
    void setHeadingRot(double r);
    void setRotation(double r);

protected:
    void paintEvent(QPaintEvent *event) override;

    // override supaya layout tahu ukuran ideal widget ini
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    double heading; // nilai heading (0-360 derajat)
    double rotate;
};

#endif // COMPASSWIDGET_H
