#include "editwaypointdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>

EditWaypointDialog::EditWaypointDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Waypoint");

    labelEdit = new QLineEdit(this);
    remarkEdit = new QLineEdit(this);
    turnRadiusSpin = new QDoubleSpinBox(this);
    turnRadiusSpin->setRange(0.0, 1000.0);
    turnRadiusSpin->setSuffix(" nm");
    turnRadiusSpin->setValue(10.0);

    activeCheck = new QCheckBox("Active", this);
    activeCheck->setChecked(true);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Waypoint Label", labelEdit);
    formLayout->addRow("Remark", remarkEdit);
    formLayout->addRow("Turning Radius [nautical miles]", turnRadiusSpin);
    formLayout->addRow(activeCheck);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &EditWaypointDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &EditWaypointDialog::reject);
}

void EditWaypointDialog::setData(const QString &label, const QString &remark, double turnRadius, bool active)
{
    labelEdit->setText(label);
    remarkEdit->setText(remark);
    turnRadiusSpin->setValue(turnRadius);
    activeCheck->setChecked(active);
}

QString EditWaypointDialog::getLabel() const
{
    return labelEdit->text();
}

QString EditWaypointDialog::getRemark() const
{
    return remarkEdit->text();
}

double EditWaypointDialog::getTurnRadius() const
{
    return turnRadiusSpin->value();
}

bool EditWaypointDialog::isActive() const
{
    return activeCheck->isChecked();
}
