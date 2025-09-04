#ifndef EBLVRM_H
#define EBLVRM_H

#include <QObject>
#include <QPainter>

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

  void setEblEnabled(bool on) { eblEnabled = on; }
  void setVrmEnabled(bool on) { vrmEnabled = on; }
  void setMeasureMode(bool on) { measureMode = on; }

  // Update on mouse move when measuring
  void onMouseMove(EcWidget* w, int x, int y);

  // Draw overlays
  void draw(EcWidget* w, QPainter& p);
};

#endif // EBLVRM_H

