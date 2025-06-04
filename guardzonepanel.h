#ifndef GUARDZONEPANEL_H
#define GUARDZONEPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QColorDialog>

#include "guardzone.h"

// Forward declarations
class EcWidget;
class GuardZoneManager;

class GuardZoneListItem : public QListWidgetItem
{
public:
    GuardZoneListItem(const GuardZone& guardZone, QListWidget* parent = nullptr);

    void updateFromGuardZone(const GuardZone& guardZone);
    int getGuardZoneId() const { return guardZoneId; }

private:
    int guardZoneId;
    void updateDisplayText(const GuardZone& guardZone);
};

class GuardZonePanel : public QWidget
{
    Q_OBJECT

public:
    explicit GuardZonePanel(EcWidget* ecWidget, GuardZoneManager* manager, QWidget *parent = nullptr);
    ~GuardZonePanel();

    void refreshGuardZoneList();
    void selectGuardZone(int guardZoneId);

    void validatePanelState();
    void recoverFromError();
    int getGuardZoneListCount() const;

    // Public access to list widget for specific needs
    QListWidget* getGuardZoneList() const { return guardZoneList; }

public slots:
    void onGuardZoneModified();
    void onGuardZoneCreated();
    void onGuardZoneDeleted();

private slots:
    // List operations
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);
    void onItemChanged(QListWidgetItem* item);

    // Context menu
    void onItemContextMenu(const QPoint& pos);

    // Bulk operations
    void onSelectAll();
    void onSelectNone();
    void onToggleSelectedActive();
    void onDeleteSelected();
    void onChangeSelectedColor();

    // Filter operations
    void onFilterChanged();
    void onShowActiveChanged(bool show);
    void onShowInactiveChanged(bool show);
    void onShapeFilterChanged();

    // Import/Export
    void onExportSelected();
    void onExportAll();
    void onImportGuardZones();

signals:
    void guardZoneSelected(int guardZoneId);
    void guardZoneEditRequested(int guardZoneId);
    void guardZoneVisibilityChanged(int guardZoneId, bool visible);
    void validationCompleted(bool success);
    void exportCompleted(bool success);
    void importCompleted(bool success);

private:
    // Core components
    EcWidget* ecWidget;
    GuardZoneManager* guardZoneManager;

    // UI components
    QVBoxLayout* mainLayout;

    // Info section
    QGroupBox* infoGroup;
    QLabel* totalLabel;
    QLabel* activeLabel;
    QLabel* inactiveLabel;

    // Filter section
    QGroupBox* filterGroup;
    QLineEdit* searchEdit;
    QCheckBox* showActiveCheck;
    QCheckBox* showInactiveCheck;
    QComboBox* shapeFilterCombo;

    // List section
    QGroupBox* listGroup;
    QListWidget* guardZoneList;

    // Bulk operations section
    QGroupBox* bulkGroup;
    QPushButton* selectAllBtn;
    QPushButton* selectNoneBtn;
    QPushButton* toggleActiveBtn;
    QPushButton* deleteSelectedBtn;
    QPushButton* changeColorBtn;

    // Import/Export section
    QGroupBox* importExportGroup;
    QPushButton* exportSelectedBtn;
    QPushButton* exportAllBtn;
    QPushButton* importBtn;

    // Helper methods
    void setupUI();
    void setupConnections();
    void updateInfoLabels();
    void applyFilters();
    bool passesFilter(const GuardZone& guardZone);
    QList<int> getSelectedGuardZoneIds();
    void showContextMenu(const QPoint& pos, QListWidgetItem* item);
    void centerOnGuardZone(int guardZoneId);
    void toggleGuardZoneActive(int guardZoneId);
    void changeGuardZoneColor(int guardZoneId);
    void renameGuardZone(int guardZoneId);
    void showGuardZoneProperties(int guardZoneId);
    void deleteGuardZone(int guardZoneId);
    void updateExistingItems();

    // Import/Export methods
    bool exportGuardZones(const QString& filename, const QList<int>& selectedIds = QList<int>());
    bool importGuardZones(const QString& filename);
};

#endif // GUARDZONEPANEL_H
