#ifndef AUTOROUTESTARTDIALOG_H
#define AUTOROUTESTARTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#endif

#include "eckernel.h"

/**
 * @brief Dialog for selecting starting point for auto route
 *
 * Allows user to choose between:
 * - Starting from own ship position
 * - Manually selecting a starting point on the chart
 */
class AutoRouteStartDialog : public QDialog
{
    Q_OBJECT

public:
    enum StartMode {
        FROM_OWNSHIP,
        MANUAL_SELECTION
    };

    /**
     * @brief Constructor
     * @param ownShipLat Current own ship latitude
     * @param ownShipLon Current own ship longitude
     * @param targetLat Target position latitude
     * @param targetLon Target position longitude
     * @param parent Parent widget
     */
    AutoRouteStartDialog(EcCoordinate ownShipLat, EcCoordinate ownShipLon,
                         EcCoordinate targetLat, EcCoordinate targetLon,
                         QWidget* parent = nullptr);

    ~AutoRouteStartDialog() = default;

    /**
     * @brief Get selected start mode
     * @return StartMode enum value
     */
    StartMode getStartMode() const;

    /**
     * @brief Get selected start position (only valid if mode is MANUAL_SELECTION)
     * @param lat Output latitude
     * @param lon Output longitude
     */
    void getManualStartPosition(EcCoordinate& lat, EcCoordinate& lon) const;

    /**
     * @brief Set manual start position (called from main widget after user clicks)
     * @param lat Latitude
     * @param lon Longitude
     */
    void setManualStartPosition(EcCoordinate lat, EcCoordinate lon);

    /**
     * @brief Check if user confirmed the selection
     * @return true if confirmed
     */
    bool isConfirmed() const { return m_confirmed; }

signals:
    void manualSelectionRequested();
    void startPositionSelected(EcCoordinate lat, EcCoordinate lon);

private slots:
    void onOwnShipRadioToggled(bool checked);
    void onManualRadioToggled(bool checked);
    void onSelectOnMapClicked();
    void onContinueClicked();
    void onCancelClicked();

private:
    void setupUI();
    void updateLabels();
    QString formatCoordinate(double lat, double lon) const;
    QString formatDistance(double distanceNM) const;
    QString getDialogStyleSheet() const;

    // Position data
    EcCoordinate m_ownShipLat, m_ownShipLon;
    EcCoordinate m_targetLat, m_targetLon;
    EcCoordinate m_manualLat, m_manualLon;

    bool m_manualPositionSet;
    bool m_confirmed;

    // UI Components
    QRadioButton* m_ownShipRadio;
    QRadioButton* m_manualRadio;
    QPushButton* m_selectOnMapButton;
    QPushButton* m_continueButton;
    QPushButton* m_cancelButton;

    QLabel* m_ownShipPosLabel;
    QLabel* m_targetPosLabel;
    QLabel* m_manualPosLabel;
    QLabel* m_distanceLabel;
    QLabel* m_instructionLabel;
};

#endif // AUTOROUTESTARTDIALOG_H
