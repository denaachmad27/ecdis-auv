#ifndef _pickwindow_h_
#define _pickwindow_h_

#include "ecwidget.h"
#include <QDialog>
#include <QTextEdit>

// SevenCs Kernel EC2007
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#pragma pack (pop)
#else
#include <stdio.h>
#include <X11/Xlib.h>
#include <eckernel.h>
#endif

extern QString sideBarText;

// window for the pick report
class PickWindow : public QDialog
{
    Q_OBJECT

public:
    PickWindow(QWidget *parent, EcDictInfo *dict, EcDENC *dc);
    void fill(QList<EcFeature> & pickFeatureList);
    void fillStyle(QList<EcFeature> & pickFeatureList, EcCoordinate lat, EcCoordinate lon);
    void fillWarningOnly(QList<EcFeature> & pickFeatureList, EcCoordinate lat, EcCoordinate lon);

    QString ownShipAutoFill();
  QJsonObject fillJson(QList<EcFeature> & pickFeatureList);
  QJsonObject fillJsonSubs(QList<EcFeature> & pickFeatureList);

  QString parseTxtFile(const QString &filePath);

  // Build AIS HTML (same style as fill) and append LAT/LON/Range/Bearing
  QString buildAisHtml(EcFeature feature,
                       EcDictInfo* dictInfo,
                       double lat,
                       double lon,
                       double rangeNm,
                       double bearingDeg);

private:
    QTextEdit  *textEdit;
  EcDictInfo *dictInfo;
  EcDENC     *denc;

  QString latViewMode;
  QString longViewMode;
};

#endif // _pickwindow_h_
