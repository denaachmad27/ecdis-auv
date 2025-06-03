#ifndef GUARDZONECHECKDIALOG_H
#define GUARDZONECHECKDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QDateTime>
#include <QStandardPaths>
#include <QToolTip>

// Struktur untuk melacak objek berbahaya dengan klasifikasi
struct DetectedObject {
    QString name;        // Nama objek
    QString type;        // Tipe objek (S-57 code)
    QString description; // Deskripsi tambahan
    double lat;          // Latitude
    double lon;          // Longitude
    int level;           // Level bahaya: 1=Info, 2=Note, 3=Warning, 4=Danger
};

class GuardZoneCheckDialog : public QDialog
{
    Q_OBJECT

public:
    GuardZoneCheckDialog(QWidget *parent = nullptr) : QDialog(parent), currentFilter(0)
    {
        setWindowTitle(tr("GuardZone Check Results"));
        setMinimumSize(700, 500);

        // Main layout
        mainLayout = new QVBoxLayout(this);

        // Title and summary
        titleLabel = new QLabel(tr("GuardZone Check Results"), this);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        mainLayout->addWidget(titleLabel);

        summaryLabel = new QLabel(this);
        mainLayout->addWidget(summaryLabel);

        // Separator
        QFrame *line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(line);

        // Filter controls
        filterGroup = new QGroupBox(tr("Filter by Level"), this);
        QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);

        filterButtons = new QButtonGroup(this);
        filterAllButton = new QRadioButton(tr("All"), this);
        filterDangerButton = new QRadioButton(tr("Danger"), this);
        filterWarningButton = new QRadioButton(tr("Warning"), this);
        filterNoteButton = new QRadioButton(tr("Note"), this);
        filterInfoButton = new QRadioButton(tr("Info"), this);

        filterButtons->addButton(filterAllButton, 0);
        filterButtons->addButton(filterDangerButton, 1);
        filterButtons->addButton(filterWarningButton, 2);
        filterButtons->addButton(filterNoteButton, 3);
        filterButtons->addButton(filterInfoButton, 4);

        filterAllButton->setChecked(true);

        filterLayout->addWidget(filterAllButton);
        filterLayout->addWidget(filterDangerButton);
        filterLayout->addWidget(filterWarningButton);
        filterLayout->addWidget(filterNoteButton);
        filterLayout->addWidget(filterInfoButton);

        mainLayout->addWidget(filterGroup);

        // Table
        objectTable = new QTableWidget(0, 4, this);
        objectTable->setHorizontalHeaderLabels(QStringList() << tr("Level") << tr("Object") << tr("Type") << tr("Description"));
        objectTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        objectTable->verticalHeader()->setVisible(false);
        objectTable->setSelectionBehavior(QTableWidget::SelectRows);
        objectTable->setSelectionMode(QTableWidget::SingleSelection);
        objectTable->setEditTriggers(QTableWidget::NoEditTriggers);
        objectTable->setAlternatingRowColors(true);

        mainLayout->addWidget(objectTable);

        // Bottom buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();

        showAllButton = new QPushButton(tr("Show All on Map"), this);
        centerButton = new QPushButton(tr("Center on Selected"), this);
        exportButton = new QPushButton(tr("Export Results"), this);
        closeButton = new QPushButton(tr("Close"), this);

        buttonLayout->addWidget(showAllButton);
        buttonLayout->addWidget(centerButton);
        buttonLayout->addWidget(exportButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);

        mainLayout->addLayout(buttonLayout);

        // Connect signals
        connect(objectTable, &QTableWidget::cellClicked, this, &GuardZoneCheckDialog::onTableItemClicked);
        connect(filterButtons, QOverload<int>::of(&QButtonGroup::buttonClicked),
                [this](int id) { currentFilter = id; onFilterChanged(); });
        connect(showAllButton, &QPushButton::clicked, this, &GuardZoneCheckDialog::onShowAllClicked);
        connect(exportButton, &QPushButton::clicked, this, &GuardZoneCheckDialog::onExportClicked);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

        // Center button is initially disabled until a row is selected
        centerButton->setEnabled(false);
        connect(centerButton, &QPushButton::clicked, [this]() {
            int row = objectTable->currentRow();
            if (row >= 0 && row < filteredObjects.size()) {
                // Kirim true untuk zoom in
                emit objectSelected(filteredObjects[row].lat, filteredObjects[row].lon, true);
            }
        });
    }

    ~GuardZoneCheckDialog()
    {
        // Tidak perlu melakukan apa-apa di sini karena Qt akan membersihkan semua widget anak
    }

    // Set data untuk ditampilkan dalam dialog
    void setData(const QList<DetectedObject> &objects, double radius, bool isCircle, int pointCount)
    {
        detectedObjects = objects;
        guardZoneRadius = radius;
        isCircularGuardZone = isCircle;
        polygonPointCount = pointCount;

        // Count objects by level
        int totalCount = objects.size();
        int dangerCount = 0;
        int warningCount = 0;
        int noteCount = 0;
        int infoCount = 0;

        for (const DetectedObject &obj : objects) {
            if (obj.level == 4) dangerCount++;
            else if (obj.level == 3) warningCount++;
            else if (obj.level == 2) noteCount++;
            else if (obj.level == 1) infoCount++;
        }

        // Update summary
        QString summary;
        if (isCircularGuardZone) {
            summary = tr("Circular GuardZone (radius: %1 NM)\n").arg(guardZoneRadius, 0, 'f', 2);
        } else {
            summary = tr("Polygon GuardZone (%1 points)\n").arg(polygonPointCount);
        }

        summary += tr("Total Objects: %1\n").arg(totalCount);
        summary += tr("Danger: %1, Warning: %2, Note: %3, Info: %4")
                       .arg(dangerCount).arg(warningCount).arg(noteCount).arg(infoCount);

        summaryLabel->setText(summary);

        // Update table
        updateTable();
    }

signals:
    // Signal untuk memperbaharui tampilan peta saat objek dipilih
    void objectSelected(double lat, double lon, bool zoom);

private slots:
    // Slot untuk menangani klik pada tabel
    void onTableItemClicked(int row, int column)
    {
        Q_UNUSED(column);
        centerButton->setEnabled(row >= 0);

        // Highlight selected row
        objectTable->clearSelection(); // Clear previous selection
        objectTable->selectRow(row);   // Select entire row

        // Show a tooltip or status message indicating the object can be centered
        if (row >= 0 && row < filteredObjects.size()) {
            QString message = tr("Click 'Center on Selected' to center map on %1").arg(filteredObjects[row].name);
            QToolTip::showText(QCursor::pos(), message);
        }
    }

    // Slot untuk menangani perubahan filter level
    void onFilterChanged()
    {
        updateTable();
    }

    // Slot untuk menangani klik tombol Show All
    void onShowAllClicked()
    {
        // This would be implemented to highlight all objects on the map
        QMessageBox::information(this, tr("Show All"), tr("This feature will highlight all detected objects on the map."));
    }

    // Slot untuk menangani klik tombol Export
    void onExportClicked()
    {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Export Results"),
                                                        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                                                            "/GuardZoneCheck_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv",
                                                        tr("CSV Files (*.csv);;All Files (*)"));
        if (fileName.isEmpty())
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Export Error"), tr("Unable to open file for writing."));
            return;
        }

        QTextStream out(&file);

        // Write header
        out << "Level,Object,Type,Description\n";

        // Write data
        for (const DetectedObject &obj : detectedObjects) {
            QString level;
            if (obj.level == 4) level = "DANGER";
            else if (obj.level == 3) level = "WARNING";
            else if (obj.level == 2) level = "NOTE";
            else if (obj.level == 1) level = "INFO";

            out << level << ","
                << "\"" << obj.name << "\"" << ","
                << "\"" << obj.type << "\"" << ","
                << "\"" << obj.description << "\"\n";
        }

        file.close();
        QMessageBox::information(this, tr("Export Successful"), tr("Results have been exported to %1").arg(fileName));
    }

private:
    // Metode untuk memperbaharui tampilan tabel
    void updateTable()
    {
        // Clear table
        objectTable->setRowCount(0);
        filteredObjects.clear();

        // Filter objects
        for (const DetectedObject &obj : detectedObjects) {
            bool include = false;

            if (currentFilter == 0) {
                // All
                include = true;
            } else {
                // Filter by level
                int level = 0;
                if (currentFilter == 1) level = 4; // Danger
                else if (currentFilter == 2) level = 3; // Warning
                else if (currentFilter == 3) level = 2; // Note
                else if (currentFilter == 4) level = 1; // Info

                include = (obj.level == level);
            }

            if (include) {
                filteredObjects.append(obj);

                int row = objectTable->rowCount();
                objectTable->insertRow(row);

                // Level
                QTableWidgetItem *levelItem = new QTableWidgetItem();
                QString levelText;
                QColor bgColor;

                if (obj.level == 4) {
                    levelText = tr("DANGER");
                    bgColor = QColor(255, 80, 80); // Red
                } else if (obj.level == 3) {
                    levelText = tr("WARNING");
                    bgColor = QColor(255, 180, 0); // Orange
                } else if (obj.level == 2) {
                    levelText = tr("NOTE");
                    bgColor = QColor(180, 180, 0); // Yellow
                } else if (obj.level == 1) {
                    levelText = tr("INFO");
                    bgColor = QColor(0, 150, 255); // Blue
                }

                levelItem->setText(levelText);
                levelItem->setBackgroundColor(bgColor);
                levelItem->setTextColor(Qt::white);
                levelItem->setTextAlignment(Qt::AlignCenter);

                objectTable->setItem(row, 0, levelItem);

                // Object name
                QTableWidgetItem *nameItem = new QTableWidgetItem(obj.name);
                objectTable->setItem(row, 1, nameItem);

                // Object type
                QTableWidgetItem *typeItem = new QTableWidgetItem(obj.type);
                objectTable->setItem(row, 2, typeItem);

                // Description
                QTableWidgetItem *descItem = new QTableWidgetItem(obj.description);
                objectTable->setItem(row, 3, descItem);
            }
        }

        // Adjust row heights
        for (int i = 0; i < objectTable->rowCount(); i++) {
            objectTable->setRowHeight(i, 24);
        }
    }

    QVBoxLayout *mainLayout;
    QLabel *titleLabel;
    QLabel *summaryLabel;

    // Filter controls
    QGroupBox *filterGroup;
    QButtonGroup *filterButtons;
    QRadioButton *filterAllButton;
    QRadioButton *filterDangerButton;
    QRadioButton *filterWarningButton;
    QRadioButton *filterNoteButton;
    QRadioButton *filterInfoButton;

    // Table
    QTableWidget *objectTable;

    // Bottom buttons
    QPushButton *showAllButton;
    QPushButton *centerButton;
    QPushButton *exportButton;
    QPushButton *closeButton;

    // Data
    QList<DetectedObject> detectedObjects;
    QList<DetectedObject> filteredObjects;
    int currentFilter; // 0=All, 1=Danger, 2=Warning, 3=Note, 4=Info

    // GuardZone info
    double guardZoneRadius;
    bool isCircularGuardZone;
    int polygonPointCount;
};

#endif // GUARDZONECHECKDIALOG_H
