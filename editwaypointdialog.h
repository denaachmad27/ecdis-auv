#ifndef EDITWAYPOINTDIALOG_H
#define EDITWAYPOINTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>

class EditWaypointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditWaypointDialog(QWidget *parent = nullptr);

    void setData(const QString &label, const QString &remark, double turnRadius, bool active);
    QString getLabel() const;
    QString getRemark() const;
    double getTurnRadius() const;
    bool isActive() const;

private:
    QLineEdit *labelEdit;
    QLineEdit *remarkEdit;
    QDoubleSpinBox *turnRadiusSpin;
    QCheckBox *activeCheck;
    QDialogButtonBox *buttonBox;
};

#endif // EDITWAYPOINTDIALOG_H
