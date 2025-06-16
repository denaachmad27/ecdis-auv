#ifndef _pickwindow_h_
#define _pickwindow_h_

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
    QString ownShipAutoFill();
    QJsonObject fillJson(QList<EcFeature> & pickFeatureList);
    QJsonObject fillJsonSubs(QList<EcFeature> & pickFeatureList);

private:
    QTextEdit  *textEdit;
  EcDictInfo *dictInfo;
  EcDENC     *denc;

};

#endif // _pickwindow_h_
