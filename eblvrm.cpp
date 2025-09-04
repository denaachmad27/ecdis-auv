#include "eblvrm.h"
#include "ecwidget.h"
#include <eckernel.h>
#include <QtMath>
#include <QPainterPath>

void EblVrm::onMouseMove(EcWidget* w, int x, int y)
{
  if (!measureMode || !w || !w->isReady()) return;
  // Convert cursor to lat/lon
  EcCoordinate lat, lon;
  if (!w->XyToLatLon(x, y, lat, lon)) {
    liveHasCursor = false; return;
  }
  liveHasCursor = true;
  liveCursorLat = lat;
  liveCursorLon = lon;

  // Compute distance and bearing from ownship to cursor
  double distNM = 0.0, bearing = 0.0;
  EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                         w->getOwnShipLat(), w->getOwnShipLon(),
                                         lat, lon,
                                         &distNM, &bearing);
  // Update parameters
  eblBearingDeg = bearing;
  vrmRadiusNM = distNM;
}

void EblVrm::draw(EcWidget* w, QPainter& p)
{
  if (!w || !w->isReady()) return;
  // Project ownship
  int cx=0, cy=0;
  if (!w->LatLonToXy(w->getOwnShipLat(), w->getOwnShipLon(), cx, cy)) return;

  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);

  // Draw EBL: line from ownship along bearing (or to cursor if live)
  if (eblEnabled) {
    // Determine endpoint: if measuring with live cursor, draw to that; else extend a fixed distance (e.g., 12 NM)
    int ex=0, ey=0;
    if (measureMode && liveHasCursor && w->LatLonToXy(liveCursorLat, liveCursorLon, ex, ey)) {
      // ok
    } else {
      // Compute endpoint from ownship using bearing and fixed 12 NM
      EcCoordinate lat2=0, lon2=0;
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                   w->getOwnShipLat(), w->getOwnShipLon(),
                                   12.0, eblBearingDeg,
                                   &lat2, &lon2);
      if (!w->LatLonToXy(lat2, lon2, ex, ey)) { ex = cx; ey = cy; }
    }
    QPen pen(QColor(0, 200, 255)); pen.setWidth(2); pen.setStyle(Qt::DashLine);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawLine(cx, cy, ex, ey);

    // Draw measurement label (distance and bearing)
    double distNM = vrmRadiusNM;
    double brg = eblBearingDeg;
    // Fallback compute if not measuring
    if (!(measureMode && liveHasCursor)) {
      EcCoordinate lat2=0, lon2=0;
      double dcalc=0.0, bcalc=0.0;
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, w->getOwnShipLat(), w->getOwnShipLon(), 12.0, eblBearingDeg, &lat2, &lon2);
      EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, w->getOwnShipLat(), w->getOwnShipLon(), lat2, lon2, &dcalc, &bcalc);
      distNM = dcalc; brg = bcalc;
    }
    QString degree = QString::fromUtf8("\u00B0");
    QString text = QString("%1 NM @ %2%3").arg(QString::number(distNM, 'f', 2)).arg(QString::number(brg, 'f', 0)).arg(degree);
    QFont f("Arial", 9, QFont::Bold); p.setFont(f); QFontMetrics fm(f);
    int midX = (cx + ex) / 2; int midY = (cy + ey) / 2;
    int dx = ex - cx, dy = ey - cy; double len = qSqrt(double(dx*dx + dy*dy));
    double nx = (len>0.0)? (-double(dy)/len) : 0.0; double ny = (len>0.0)? (double(dx)/len) : 0.0;
    int offset = qMax(24, int(fm.height()*2 + 4));
    int lx = midX + int(nx * offset); int ly = midY + int(ny * offset);
    int tw = fm.horizontalAdvance(text); int ascent = fm.ascent(); int th = fm.height();
    int pad = 4;
    // Theme-aware colors
    QColor win = w->palette().color(QPalette::Window);
    int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
    bool darkTheme = (luma < 128);
    QColor bgCol = darkTheme ? QColor(0,0,0,160) : QColor(255,255,255,210);
    QColor fgCol = darkTheme ? QColor(240,240,240) : QColor(30,30,30);
    QRect bgRect(lx - tw/2 - pad, ly - ascent - pad, tw + pad*2, th + pad*2);
    p.setPen(Qt::NoPen); p.setBrush(bgCol); p.drawRoundedRect(bgRect, 4, 4);
    p.setPen(fgCol); p.drawText(bgRect.left() + pad, bgRect.top() + pad + ascent, text);
  }

  // Draw VRM: ring around ownship with current radius
  if (vrmEnabled) {
    // Approximate ring by sampling positions every few degrees (24 segments)
    const int segs = 72; // 5° step
    QPainterPath ring;
    bool started = false;
    for (int i = 0; i < segs; ++i) {
      double brg = (360.0 * i) / segs;
      EcCoordinate lat2=0, lon2=0;
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                   w->getOwnShipLat(), w->getOwnShipLon(),
                                   vrmRadiusNM, brg,
                                   &lat2, &lon2);
      int px=0, py=0;
      if (!w->LatLonToXy(lat2, lon2, px, py)) continue;
      if (!started) { ring.moveTo(px, py); started = true; }
      else { ring.lineTo(px, py); }
    }
    if (started) {
      ring.closeSubpath();
      QPen rpen(QColor(0, 255, 128)); rpen.setWidth(2); rpen.setStyle(Qt::SolidLine);
      p.setPen(rpen); p.setBrush(Qt::NoBrush);
      p.drawPath(ring);

      // Add radius label near top of ring (bearing 0° from ownship)
      EcCoordinate latTop=0, lonTop=0;
      int tx=0, ty=0;
      if (EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, w->getOwnShipLat(), w->getOwnShipLon(), vrmRadiusNM, 0.0, &latTop, &lonTop),
          w->LatLonToXy(latTop, lonTop, tx, ty)) {
        QString rtext = QString("R: %1 NM").arg(QString::number(vrmRadiusNM, 'f', 2));
        QFont rf("Arial", 9, QFont::Bold); p.setFont(rf); QFontMetrics rfm(rf);
        int tw = rfm.horizontalAdvance(rtext); int ascent = rfm.ascent(); int th = rfm.height();
        int off = qMax(24, int(rfm.height()*2 + 4));
        int ly = ty - off;
        int pad = 4;
        QColor win = w->palette().color(QPalette::Window);
        int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
        bool darkTheme = (luma < 128);
        QColor bgCol = darkTheme ? QColor(0,0,0,160) : QColor(255,255,255,210);
        QColor fgCol = darkTheme ? QColor(240,240,240) : QColor(30,30,30);
        QRect bgRect(tx - tw/2 - pad, ly - ascent - pad, tw + pad*2, th + pad*2);
        p.setPen(Qt::NoPen); p.setBrush(bgCol); p.drawRoundedRect(bgRect, 4, 4);
        p.setPen(fgCol); p.drawText(bgRect.left() + pad, bgRect.top() + pad + ascent, rtext);
      }
    }
  }

  p.restore();
}
