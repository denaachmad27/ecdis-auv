#ifndef SEARCHWINDOW_H
#define SEARCHWINDOW_H

#include <QDialog>
#include <QTextEdit>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QDialogButtonBox;
class QErrorMessage;
QT_END_NAMESPACE

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

// window for the pick report
class SearchWindow : public QDialog
{
    Q_OBJECT

public:
    SearchWindow(const QString &title, QWidget *parent);

public slots:
    void onOkClick();

public:
    EcCoordinate latitude() const;
    QString getCheckedRadioButtonValue() const;

private:
    QLabel *latitudeLabel;
    QLabel *longitudeLabel;
    QLineEdit *latitudeEdit;
    QLineEdit *longitudeEdit;
    QTextEdit *textEdit;
    QRadioButton *radio1;
    QRadioButton *radio2;
    QRadioButton *radio3;
    QRadioButton *radio4;
    QDialogButtonBox *buttonBox;

    EcCoordinate searchLat, searchLon;

};

#endif // SEARCHWINDOW_H
