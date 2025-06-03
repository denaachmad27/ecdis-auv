
#include <QtWidgets>

#include "searchwindow.h"

SearchWindow::SearchWindow(const QString &title, QWidget *parent)
    : QDialog(parent)
{

    latitudeLabel = new QLabel(tr("Latitude:"));
    longitudeLabel = new QLabel(tr("Longitude:"));

    latitudeEdit = new QLineEdit;
    longitudeEdit = new QLineEdit;

    QGroupBox *groupBox = new QGroupBox(tr("Warning"));

    radio1 = new QRadioButton(tr("&Show All"));
    radio2 = new QRadioButton(tr("Caution Area (CTNARE)"));
    radio3 = new QRadioButton(tr("Restricted Area (RESARE)"));
    radio4 = new QRadioButton(tr("Depth Area (DEPARE)"));

    // Create a vertical layout for the group box
    QVBoxLayout *groupLayout = new QVBoxLayout;
    groupLayout->addWidget(radio1);
    groupLayout->addWidget(radio2);
    groupLayout->addWidget(radio3);
    groupLayout->addWidget(radio4);

    // Set the layout for the group box
    groupBox->setLayout(groupLayout);

    radio1->setChecked(true);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
                                     | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SearchWindow::onOkClick);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SearchWindow::reject);

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addWidget(latitudeLabel, 0, 0);
    mainLayout->addWidget(latitudeEdit, 0, 1);
    mainLayout->addWidget(longitudeLabel, 1, 0);
    mainLayout->addWidget(longitudeEdit, 1, 1);
    mainLayout->addWidget(groupBox, 2, 0, 1, 2); // Row 2, Column 0, spanning 1 row and 2 columns
    mainLayout->addWidget(buttonBox, 3, 0, 1, 3);
    setLayout(mainLayout);

    setMinimumSize( QSize( 300, 200 ) );

    setWindowTitle(title);
}

QString SearchWindow::getCheckedRadioButtonValue() const {
    if (radio1->isChecked()) {
        return "0"; // Value for Show All
    } else if (radio2->isChecked()) {
        return "1"; // Value for Caution Area
    } else if (radio3->isChecked()) {
        return "2"; // Value for Restricted Area
    } else if (radio4->isChecked()) {
        return "3"; // Value for Depth Area
    }
    return "0"; // Default value if none are checked
}

void SearchWindow::onOkClick() {
    searchLat = latitudeEdit->text().toDouble();
    searchLon = longitudeEdit->text().toDouble();
    accept();
}
