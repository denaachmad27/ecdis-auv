#include "eblvrm.h"
#include "ecwidget.h"
#include <eckernel.h>
#include <QtMath>
#include <QPainterPath>
#include <QString>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>

QString EblVrm::formatDistance(double nm) const {
  QStringList parts;
  if (showNmUnit) {
    parts << QString("%1 NM").arg(QString::number(nm, 'f', 2));
  }
  if (showYardUnit) {
    const double yards = nm * 1852.0 / 0.9144; // 1 NM = 1852 m, 1 yd = 0.9144 m
    parts << QString("%1 yd").arg(QString::number(yards, 'f', 0));
  }
  if (showKmUnit) {
    const double km = nm * 1.852; // 1 NM = 1.852 km
    parts << QString("%1 km").arg(QString::number(km, 'f', 2));
  }
  return parts.join(" / ");
}

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
                                         navShip.lat, navShip.lon,
                                         lat, lon,
                                         &distNM, &bearing);
  // Update EBL/VRM only while picking the very first point
  if (!(measuringActive && !measurePoints.isEmpty())) {
    eblBearingDeg = bearing;
    vrmRadiusNM = distNM;
  }
}

void EblVrm::draw(EcWidget* w, QPainter& p)
{
  if (!w || !w->isReady()) return;
  // Project ownship
  int cx=0, cy=0;
  if (!w->LatLonToXy(navShip.lat, navShip.lon, cx, cy)) return;

  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);

  // Draw EBL: line from ownship along bearing (or to cursor if live)
  // Hide EBL while measuring after the first point; only show measurement lines then.
  if (eblEnabled && !(measureMode && measuringActive)) {
    // Determine endpoint:
    // - if measuring with live cursor: draw to cursor
    // - else if we have a fixed target: draw to that fixed point
    // - else extend along bearing a fixed distance (12 NM) as fallback
    int ex=0, ey=0;
    EcCoordinate lat2=0, lon2=0;
    bool haveEndpoint = false;
    if (measureMode && liveHasCursor) {
      lat2 = liveCursorLat; lon2 = liveCursorLon; haveEndpoint = w->LatLonToXy(lat2, lon2, ex, ey);
    } else if (eblHasFixedPoint) {
      lat2 = eblFixedLat; lon2 = eblFixedLon; haveEndpoint = w->LatLonToXy(lat2, lon2, ex, ey);
    } else {
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                   navShip.lat, navShip.lon,
                                   12.0, eblBearingDeg,
                                   &lat2, &lon2);
      haveEndpoint = w->LatLonToXy(lat2, lon2, ex, ey);
    }
    if (!haveEndpoint) { ex = cx; ey = cy; }
    QPen pen(QColor(0, 200, 255)); pen.setWidth(2); pen.setStyle(Qt::DashLine);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawLine(cx, cy, ex, ey);

    // Draw measurement label (distance and bearing)
    double distNM = 0.0;
    double brg = 0.0;
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, lat2, lon2, &distNM, &brg);
    QString degree = QString::fromUtf8("\u00B0");
    QString text = QString("%1 @ %2%3")
                     .arg(formatDistance(distNM))
                     .arg(QString::number(brg, 'f', 0)).arg(degree);
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
  // Suppress VRM while in active measure session to avoid area visuals
  if (vrmEnabled && !(measureMode && measuringActive)) {
    // If we have a fixed target, VRM radius follows dynamic distance from ownship to fixed target.
    double vr = vrmRadiusNM;
    if (eblHasFixedPoint) {
      double dnm=0.0, btmp=0.0;
      EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, eblFixedLat, eblFixedLon, &dnm, &btmp);
      vr = dnm;
    }
    // Approximate ring by sampling positions every few degrees (24 segments)
    const int segs = 72; // 5° step
    QPainterPath ring;
    bool started = false;
    for (int i = 0; i < segs; ++i) {
      double brg = (360.0 * i) / segs;
      EcCoordinate lat2=0, lon2=0;
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                   navShip.lat, navShip.lon,
                                   vr, brg,
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
      if (EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon, vr, 0.0, &latTop, &lonTop),
          w->LatLonToXy(latTop, lonTop, tx, ty)) {
        QString rtext = QString("R: %1").arg(formatDistance(vr));
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

  // Draw Measure Session (multi-segment like route)
  if (measureMode && measuringActive) {
    p.setRenderHint(QPainter::Antialiasing, true);

    // Theme-aware colors
    QColor win = w->palette().color(QPalette::Window);
    int luma = qRound(0.2126*win.red() + 0.7152*win.green() + 0.0722*win.blue());
    bool darkTheme = (luma < 128);
    QColor segColor = QColor(0, 128, 255); // blue
    QColor labelBg = darkTheme ? QColor(0,0,0,160) : QColor(255,255,255,210);
    QColor labelFg = darkTheme ? QColor(240,240,240) : QColor(30,30,30);
    QPen sp(segColor); sp.setWidth(2); sp.setStyle(Qt::SolidLine);
    p.setPen(sp);
    p.setBrush(Qt::NoBrush);

    auto formatLatLon = [](double lat, double lon){
      return QString("%1, %2")
        .arg(QString::number(lat, 'f', 5))
        .arg(QString::number(lon, 'f', 5));
    };

    QFont f("Arial", 9, QFont::Bold);
    p.setFont(f);
    QFontMetrics fm(f);
    int pad = 4;

    auto drawLabel = [&](int x1, int y1, int x2, int y2, const QString& text){
      int midX = (x1 + x2) / 2; int midY = (y1 + y2) / 2;
      int dx = x2 - x1, dy = y2 - y1; double len = qSqrt(double(dx*dx + dy*dy));
      double nx = (len>0.0)? (-double(dy)/len) : 0.0; double ny = (len>0.0)? (double(dx)/len) : 0.0;
      int offset = qMax(24, int(fm.height()*2 + 4));
      int lx = midX + int(nx * offset); int ly = midY + int(ny * offset);
      int tw = fm.horizontalAdvance(text); int ascent = fm.ascent(); int th = fm.height();
      QRect bgRect(lx - tw/2 - pad, ly - ascent - pad, tw + pad*2, th + pad*2);
      p.setPen(Qt::NoPen); p.setBrush(labelBg); p.drawRoundedRect(bgRect, 4, 4);
      p.setPen(labelFg); p.drawText(bgRect.left() + pad, bgRect.top() + pad + ascent, text);
    };

    // 1) First leg: from ownship to first point (dynamic)
    if (!measurePoints.isEmpty()) {
      const double lat1 = navShip.lat;
      const double lon1 = navShip.lon;
      const double lat2 = measurePoints[0].x();
      const double lon2 = measurePoints[0].y();
      int x1=cx, y1=cy, x2=0, y2=0;
      if (w->LatLonToXy(lat2, lon2, x2, y2)) {
        p.setPen(sp);
        p.drawLine(x1, y1, x2, y2);
        double dnm=0.0, brg=0.0;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, lat1, lon1, lat2, lon2, &dnm, &brg);
        QString degree = QString::fromUtf8("\u00B0");
        QString text = QString("%1 @ %2%3  %4")
                       .arg(formatDistance(dnm))
                       .arg(QString::number(brg,'f',0)).arg(degree)
                       .arg(formatLatLon(lat2, lon2));
        drawLabel(x1, y1, x2, y2, text);
      }
    }

    // 2) Subsequent legs: between fixed points
    for (int i = 1; i < measurePoints.size(); ++i) {
      double latA = measurePoints[i-1].x();
      double lonA = measurePoints[i-1].y();
      double latB = measurePoints[i].x();
      double lonB = measurePoints[i].y();
      int xa=0, ya=0, xb=0, yb=0;
      if (w->LatLonToXy(latA, lonA, xa, ya) && w->LatLonToXy(latB, lonB, xb, yb)) {
        p.setPen(sp);
        p.drawLine(xa, ya, xb, yb);
        double dnm=0.0, brg=0.0;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, latA, lonA, latB, lonB, &dnm, &brg);
        QString degree = QString::fromUtf8("\u00B0");
        QString text = QString("%1 @ %2%3  %4")
                       .arg(formatDistance(dnm))
                       .arg(QString::number(brg,'f',0)).arg(degree)
                       .arg(formatLatLon(latB, lonB));
        drawLabel(xa, ya, xb, yb, text);
      }
    }

    // 3) Ghost leg to live cursor (from last vertex or ownship if empty)
    if (liveHasCursor) {
      double latA = (measurePoints.isEmpty() ? navShip.lat : measurePoints.back().x());
      double lonA = (measurePoints.isEmpty() ? navShip.lon : measurePoints.back().y());
      double latB = liveCursorLat;
      double lonB = liveCursorLon;
      int xa=0, ya=0, xb=0, yb=0;
      if (w->LatLonToXy(latA, lonA, xa, ya) && w->LatLonToXy(latB, lonB, xb, yb)) {
        QPen ghostPen(segColor); ghostPen.setStyle(Qt::DashLine); ghostPen.setWidth(2);
        p.setPen(ghostPen);
        p.drawLine(xa, ya, xb, yb);
        double dnm=0.0, brg=0.0;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, latA, lonA, latB, lonB, &dnm, &brg);
        QString degree = QString::fromUtf8("\u00B0");
        QString text = QString("%1 @ %2%3  %4")
                       .arg(formatDistance(dnm))
                       .arg(QString::number(brg,'f',0)).arg(degree)
                       .arg(formatLatLon(latB, lonB));
        drawLabel(xa, ya, xb, yb, text);
        p.setPen(sp);
      }
    }

    // 4) Total distance label near last point (sum with dynamic first leg)
    if (!measurePoints.isEmpty()) {
      double totalNM = 0.0;
      // First dynamic leg
      double dnm=0.0, brgTmp=0.0;
      EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, navShip.lat, navShip.lon,
                                             measurePoints[0].x(), measurePoints[0].y(), &dnm, &brgTmp);
      totalNM += dnm;
      // Fixed segments
      for (int i=1;i<measurePoints.size();++i) {
        double d=0.0,b=0.0;
        EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, measurePoints[i-1].x(), measurePoints[i-1].y(),
                                               measurePoints[i].x(), measurePoints[i].y(), &d, &b);
        totalNM += d;
      }
      // Anchor label at last point (or cursor if present)
      double latL, lonL;
      if (liveHasCursor) { latL = liveCursorLat; lonL = liveCursorLon; }
      else               { latL = measurePoints.back().x(); lonL = measurePoints.back().y(); }
      int lx=0, ly=0; if (w->LatLonToXy(latL, lonL, lx, ly)) {
        QString t = QString("Total: %1").arg(formatDistance(totalNM));
        int tw = fm.horizontalAdvance(t); int ascent = fm.ascent(); int th = fm.height();
        QRect bgRect(lx - tw/2 - pad, ly - ascent - pad - 16, tw + pad*2, th + pad*2);
        p.setPen(Qt::NoPen); p.setBrush(labelBg); p.drawRoundedRect(bgRect, 4, 4);
        p.setPen(labelFg); p.drawText(bgRect.left() + pad, bgRect.top() + pad + ascent, t);
      }
    }
  }

  // Temporary VRM ring while measuring next segment:
  // Show green ring only during measurement, only after first point placed,
  // and only while cursor is moving for the next point.
  if (measureMode && measuringActive && liveHasCursor && measurePoints.size() >= 1) {
    // Center ring at last placed point; radius is lastPoint -> cursor
    EcCoordinate cLat = measurePoints.back().x();
    EcCoordinate cLon = measurePoints.back().y();
    const int segs = 72;
    QPainterPath ring;
    bool started = false;
    double dnm=0.0, btmp=0.0;
    EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84, cLat, cLon, liveCursorLat, liveCursorLon, &dnm, &btmp);
    for (int i = 0; i < segs; ++i) {
      double brg = (360.0 * i) / segs;
      EcCoordinate lat2=0, lon2=0;
      EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84, cLat, cLon, dnm, brg, &lat2, &lon2);
      int px=0, py=0;
      if (!w->LatLonToXy(lat2, lon2, px, py)) continue;
      if (!started) { ring.moveTo(px, py); started = true; } else { ring.lineTo(px, py); }
    }
    if (started) {
      ring.closeSubpath();
      QPen rpen(QColor(0, 255, 128)); rpen.setWidth(2); rpen.setStyle(Qt::SolidLine);
      p.setPen(rpen); p.setBrush(Qt::NoBrush);
      p.drawPath(ring);
    }
  }

  p.restore();
}
