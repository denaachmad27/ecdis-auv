#include "CompassWidget.h"
#include "qpainterpath.h"
#include <QtMath>
#include <QPainter>

inline double deg2rad(double deg) { return deg * M_PI / 180.0; }

CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent), heading(0) {
    setFixedSize(200, 200); // ukuran widget tetap
}

void CompassWidget::setHeading(double h) {
    heading = fmod(h, 360.0); // biar 0–360
    update(); // redraw
}

void CompassWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int margin = 40;                     // padding dari tepi panel
    int r = (qMin(w, h) / 2) - margin;   // radius lingkaran
    QPoint center(w / 2, h / 2);

    // Helper: sudut kompas -> titik layar (0°=N, 90°=E, 180°=S, 270°=W)
    auto polar = [&](double compassDeg, double extraRadius) -> QPoint {
        double a = deg2rad(compassDeg - 90.0); // shift supaya 0° di atas
        double R = r + extraRadius;
        int x = int(std::lround(center.x() + R * std::cos(a)));
        int y = int(std::lround(center.y() + R * std::sin(a))); // +sin karena Y ke bawah
        return QPoint(x, y);
    };

    // Lingkaran kompas
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(center, r, r);

    // Huruf arah (di luar lingkaran)
    QFont letterFont = p.font();
    letterFont.setBold(true);
    letterFont.setPointSize(11);
    p.setFont(letterFont);
    const int offsetLabel = 12;   // jarak huruf dari lingkaran (ke luar)
    auto drawLabel = [&](double deg, const QString& txt) {
        QPoint pt = polar(deg, offsetLabel);
        QRect rect(pt.x() - 15, pt.y() - 15, 30, 30);
        p.drawText(rect, Qt::AlignCenter, txt);
    };
    drawLabel(0,   "N");
    drawLabel(90,  "E");
    drawLabel(180, "S");
    drawLabel(270, "W");

    // Angka derajat (lebih luar dari huruf)
    // QFont degFont("Arial", 8);
    // p.setFont(degFont);
    // const int offsetDegree = 26;  // pastikan > offsetLabel agar di luar
    // for (int deg : {0, 90, 180, 270}) {
    //     QPoint pt = polar(deg, offsetDegree);
    //     QRect rect(pt.x() - 12, pt.y() - 12, 24, 24);
    //     p.drawText(rect, Qt::AlignCenter, QString::number(deg) + "°");
    // }

    // Kapal (segitiga) mengikuti heading (0° menghadap ke atas)
    // Gambar kapal (bentuk panah lurus seperti kompas)
    p.save();
    p.translate(center);
    p.rotate(heading);

    QPolygon ship;

    // Ujung depan lancip (lebih panjang ke depan)
    ship << QPoint(0, -r * 0.7);

    // Sisi kanan badan
    ship << QPoint(r * 0.25, r * 0.2);

    // Belakang rata agak panjang
    ship << QPoint(r * 0.25, r * 0.6);
    ship << QPoint(-r * 0.25, r * 0.6);

    // Sisi kiri badan
    ship << QPoint(-r * 0.25, r * 0.2);

    p.setBrush(Qt::gray);
    p.setPen(QPen(Qt::white, 1));
    p.drawPolygon(ship);

    p.restore();
}
