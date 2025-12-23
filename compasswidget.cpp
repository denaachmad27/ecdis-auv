#include "CompassWidget.h"
#include "appconfig.h"
#include "qdebug.h"
#include "qpainterpath.h"
#include <QtMath>
#include <QPainter>
#include <QSvgRenderer>

inline double deg2rad(double deg) { return deg * M_PI / 180.0; }

CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent), heading(0)
{
    setFixedWidth(200);                               // width tetap 200
    setMinimumHeight(200);                            // tinggi minimum (opsional)
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding); // tinggi bisa melebar
}

QSize CompassWidget::sizeHint() const {
    return QSize(200, 260); // default tinggi yang “diinginkan” (opsional)
}

QSize CompassWidget::minimumSizeHint() const {
    return QSize(200, 200); // batas minimum (opsional)
}

void CompassWidget::setHeading(double h) {
    heading = fmod(h, 360.0); // biar 0–360
    update(); // redraw
}

void CompassWidget::setHeadingRot(double r) {
    heading = fmod(heading-r, 360.0); // biar 0–360
    update(); // redraw
}

void CompassWidget::setRotation(double r) {
    rotate = r;
    update(); // redraw
}

void CompassWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int margin = 40;
    int r = (qMin(w, h) / 2) - margin;
    QPoint center(w / 2, h / 2);

    auto polar = [&](double compassDeg, double extraRadius) -> QPoint {
        double a = deg2rad(compassDeg - 90.0);
        double R = r + extraRadius;
        int x = int(std::lround(center.x() + R * std::cos(a)));
        int y = int(std::lround(center.y() + R * std::sin(a)));
        return QPoint(x, y);
    };

    // lingkaran kompas
    QColor compassColor = (AppConfig::theme() == AppConfig::AppTheme::Light) ? Qt::black : Qt::white;
    p.setPen(QPen(compassColor, 1));
    p.drawEllipse(center, r, r);

    // huruf arah
    QFont letterFont = p.font();
    letterFont.setPointSize(11);
    p.setFont(letterFont);
    p.setPen(compassColor);  // gunakan warna yang sama dengan lingkaran
    const int offsetLabel = 12;
    auto drawLabel = [&](double deg, const QString& txt) {
        QPoint pt = polar(deg, offsetLabel);
        QRect rect(pt.x() - 15, pt.y() - 15, 30, 30);
        p.drawText(rect, Qt::AlignCenter, txt);
    };
    drawLabel(0-rotate,   "N");
    drawLabel(90-rotate,  "E");
    drawLabel(180-rotate, "S");
    drawLabel(270-rotate, "W");

    // === Gambar kapal pakai SVG ===
    static QSvgRenderer shipSvgDark(QString(":/icon/ship-dark.svg"));
    static QSvgRenderer shipSvgLight(QString(":/icon/ship-light.svg"));

    QSvgRenderer* shipSvg = nullptr;
    switch (AppConfig::theme()) {
        case AppConfig::AppTheme::Light:
            shipSvg = &shipSvgLight;
            break;
        case AppConfig::AppTheme::Dim:
        case AppConfig::AppTheme::Dark:
        default:
            shipSvg = &shipSvgDark;
            break;
    }

    QSize shipSize(r * 1.4, r * 1.4);  // ukuran relatif
    QRect targetRect(-shipSize.width()/2, -shipSize.height()/2,
                     shipSize.width(), shipSize.height());

    p.save();
    p.translate(center);
    p.rotate(heading);
    shipSvg->render(&p, targetRect);
    p.restore();
}
