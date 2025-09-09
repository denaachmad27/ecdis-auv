#ifndef EBLVRM_H
#define EBLVRM_H

#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QVector>

class EcWidget; // fwd

class EblVrm : public QObject {
  Q_OBJECT
public:
  explicit EblVrm(QObject* parent = nullptr) : QObject(parent) {}

  // States
  bool eblEnabled = false;     // show EBL line
  bool vrmEnabled = false;     // show VRM ring
  bool measureMode = false;    // live update from ownship->cursor

  // Parameters
  double eblBearingDeg = 0.0;  // 0..360
  double vrmRadiusNM = 1.0;    // radius in NM

  // Live cursor (used when measureMode is true)
  double liveCursorLat = 0.0;
  double liveCursorLon = 0.0;
  bool   liveHasCursor = false;

  // Continuous measure session (multi-segment like route)
  // Points are stored as (lat, lon). When not empty, the first leg is
  // dynamically measured from ownship -> first point.
  QVector<QPointF> measurePoints;
  bool measuringActive = false; // track session lifetime while measureMode is on

  // Session controls
  void startMeasureSession() { measuringActive = true; measurePoints.clear(); }
  void clearMeasureSession() { measuringActive = false; measurePoints.clear(); liveHasCursor = false; }
  void addMeasurePoint(double lat, double lon) { if (measureMode) { if (!measuringActive) measuringActive = true; measurePoints.append(QPointF(lat, lon)); } }

  void setEblEnabled(bool on) { eblEnabled = on; }
  void setVrmEnabled(bool on) { vrmEnabled = on; }
  void setMeasureMode(bool on) {
    measureMode = on;
    if (on) {
      // Auto-show EBL/VRM when starting measurement from ownship
      eblEnabled = true;
      vrmEnabled = true;
      // Clear previous fixed target and live cursor state for a fresh session
      clearFixedPoint();
      liveHasCursor = false;
    }
  }

  // Update on mouse move when measuring
  void onMouseMove(EcWidget* w, int x, int y);

  // Fixed EBL target after finishing measurement
  bool eblHasFixedPoint = false;
  double eblFixedLat = 0.0;
  double eblFixedLon = 0.0;
  void clearFixedPoint() { eblHasFixedPoint = false; eblFixedLat = eblFixedLon = 0.0; }
  void commitFirstPointAsFixedTarget() {
    if (!measurePoints.isEmpty()) {
      eblHasFixedPoint = true;
      eblFixedLat = measurePoints.first().x();
      eblFixedLon = measurePoints.first().y();
      // Ensure visibility of EBL/VRM after finishing measurement
      eblEnabled = true;
      vrmEnabled = true;
    }
  }

  // Draw overlays
  void draw(EcWidget* w, QPainter& p);
};

#endif // EBLVRM_H
