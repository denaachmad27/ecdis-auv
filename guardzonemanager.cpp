#include "guardzonemanager.h"
#include "ecwidget.h"

#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QApplication>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#pragma pack (pop)
#else
#include <eckernel.h>
#endif

// Constants
const double GuardZoneManager::VERTEX_THRESHOLD = 15.0;
const double GuardZoneManager::EDGE_THRESHOLD = 10.0;

// New constants for advanced editing
const int GuardZoneManager::HANDLE_SIZE = 8;
const int GuardZoneManager::HANDLE_HOVER_SIZE = 12;
const double GuardZoneManager::HANDLE_DETECT_RADIUS = 15.0;
// =============================================

GuardZoneManager::GuardZoneManager(EcWidget* parent)
    : QObject(parent)
    , ecWidget(parent)
    , editingGuardZone(false)
    , editingGuardZoneId(-1)
    // ========== INITIALIZE ADVANCED EDIT VARIABLES ==========
    , currentEditMode(EDIT_NONE)
    , draggedHandleIndex(-1)
    , isDragging(false)
    // ========================================================
{
}

GuardZoneManager::~GuardZoneManager()
{
}

void GuardZoneManager::startEditGuardZone(int guardZoneId)
{
    qDebug() << "Starting advanced edit mode for GuardZone ID:" << guardZoneId;

    if (!ecWidget) {
        qDebug() << "EcWidget is null!";
        return;
    }

    // Find guardzone
    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    bool found = false;

    for (const GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            // ========== BACKUP ORIGINAL SHAPE ==========
            originalGuardZone = gz;  // Make a copy for restore functionality
            hasOriginalBackup = true;
            // ==========================================
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "GuardZone with ID" << guardZoneId << "not found!";
        return;
    }

    editingGuardZone = true;
    editingGuardZoneId = guardZoneId;

    // Initialize advanced edit mode
    currentEditMode = EDIT_NONE;
    isDragging = false;
    draggedHandleIndex = -1;

    // Create edit handles for the guardzone
    updateEditHandles();

    qDebug() << "Advanced edit mode activated for GuardZone ID:" << guardZoneId;

    emit editModeChanged(true);
    emit statusMessage(tr("Advanced edit mode: Drag handles to modify GuardZone. Right-click for options. Press ESC to finish."));

    // Force redraw to show edit handles
    ecWidget->update();
}

void GuardZoneManager::finishEditGuardZone()
{
    qDebug() << "Finishing advanced edit mode";

    editingGuardZone = false;
    editingGuardZoneId = -1;

    // Cleanup advanced edit mode
    currentEditMode = EDIT_NONE;
    isDragging = false;
    draggedHandleIndex = -1;
    clearEditHandles();

    // ========== CLEANUP ORIGINAL BACKUP ==========
    hasOriginalBackup = false;
    // ============================================

    emit editModeChanged(false);
    emit statusMessage(tr("Edit mode finished"));

    if (ecWidget) {
        ecWidget->update();
    }
}

bool GuardZoneManager::handleMousePress(QMouseEvent *e)
{
    if (!editingGuardZone || !ecWidget) return false;

    if (e->button() == Qt::LeftButton) {
        // ========== CHECK HANDLE CLICK FIRST (PRIORITY) ==========
        int handleIndex = findHandleAt(e->pos());
        if (handleIndex >= 0) {
            // Start dragging the handle
            startDragHandle(handleIndex, e->pos());
            return true; // Event handled
        }
        // =========================================================

        // ========== CHECK FOR EDGE CLICK (POLYGON ONLY) ==========
        QList<GuardZone>& guardZones = ecWidget->getGuardZones();
        GuardZone* editingGz = nullptr;

        for (GuardZone& gz : guardZones) {
            if (gz.id == editingGuardZoneId) {
                editingGz = &gz;
                break;
            }
        }

        // If editing polygon, check for edge click
        if (editingGz && editingGz->shape == GUARD_ZONE_POLYGON) {
            int edgeIndex = findEdgeAt(e->pos());
            if (edgeIndex >= 0) {
                addVertexToPolygon(edgeIndex, e->pos());
                return true; // Event handled
            }
        }
        // =========================================================
    }
    else if (e->button() == Qt::RightButton) {
        // ========== RIGHT CLICK HANDLING ==========
        int handleIndex = findHandleAt(e->pos());

        if (handleIndex >= 0) {
            // Right click on handle - remove vertex (polygon only)
            if (editHandles[handleIndex].type == HANDLE_VERTEX) {
                removeVertexFromPolygon(handleIndex);
                return true; // Event handled
            }
        }

        // Right click on empty area - show context menu
        showEditContextMenu(e->pos());
        return true; // Event handled
        // ========================================
    }

    return false; // Event not handled, let normal processing continue
}

bool GuardZoneManager::handleMouseMove(QMouseEvent *e)
{
    if (!editingGuardZone || !ecWidget) return false;

    // ========== ADVANCED EDIT MOUSE MOVE ==========
    if (isDragging && draggedHandleIndex >= 0) {
        // Update the dragged handle position
        updateDragHandle(e->pos());
        ecWidget->update(); // Force redraw
        return true; // Event handled
    } else {
        // Check for handle hover effects
        int hoverHandle = findHandleAt(e->pos());
        bool needsUpdate = false;

        // Update hover state for all handles
        for (int i = 0; i < editHandles.size(); i++) {
            bool wasHovered = editHandles[i].hovered;
            editHandles[i].hovered = (i == hoverHandle);

            if (wasHovered != editHandles[i].hovered) {
                needsUpdate = true;
            }
        }

        // Change cursor based on hover state
        if (hoverHandle >= 0) {
            ecWidget->setCursor(Qt::SizeAllCursor);
        } else {
            ecWidget->setCursor(Qt::ArrowCursor);
        }

        if (needsUpdate) {
            ecWidget->update();
        }

        return true; // Always handle mouse move in edit mode
    }
    // =============================================

    return false;
}

bool GuardZoneManager::handleMouseRelease(QMouseEvent *e)
{
    if (!editingGuardZone || !ecWidget) return false;

    // ========== ADVANCED EDIT MOUSE RELEASE ==========
    if (isDragging && e->button() == Qt::LeftButton) {
        finishDragHandle();
        return true; // Event handled
    }
    // ================================================

    return false;
}

int GuardZoneManager::getGuardZoneAtPosition(int x, int y)
{
    if (!ecWidget) return -1;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    // Konversi screen coordinates ke lat/lon
    EcCoordinate clickLat, clickLon;
    if (!ecWidget->XyToLatLon(x, y, clickLat, clickLon)) {
        return -1;
    }

    // Check setiap guardzone (dari yang terakhir dibuat/paling atas)
    for (int i = guardZones.size() - 1; i >= 0; i--) {
        const GuardZone& gz = guardZones[i];

        if (!gz.active) continue;

        if (gz.shape == GUARD_ZONE_CIRCLE) {
            // Check if click is inside circle
            double centerLat = gz.centerLat;
            double centerLon = gz.centerLon;

            // Calculate distance dari center ke click point
            double distance, bearing;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   centerLat, centerLon,
                                                   clickLat, clickLon,
                                                   &distance, &bearing);

            // Jika distance kurang dari radius, point ada di dalam circle
            if (distance <= gz.radius) {
                return gz.id;
            }
        }
        else if (gz.shape == GUARD_ZONE_POLYGON && gz.latLons.size() >= 6) {
            // Check if click is inside polygon menggunakan point-in-polygon algorithm
            QPolygon screenPolygon;
            for (int j = 0; j < gz.latLons.size(); j += 2) {
                int px, py;
                if (ecWidget->LatLonToXy(gz.latLons[j], gz.latLons[j+1], px, py)) {
                    screenPolygon.append(QPoint(px, py));
                }
            }

            if (screenPolygon.size() >= 3 && screenPolygon.containsPoint(QPoint(x, y), Qt::OddEvenFill)) {
                return gz.id;
            }
        }
    }

    return -1; // Tidak ada guardzone yang di-click
}

void GuardZoneManager::showGuardZoneContextMenu(const QPoint &pos, int guardZoneId)
{
    if (!ecWidget || guardZoneId == -1) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    // Find guardzone dengan ID yang sesuai
    GuardZone* targetGuardZone = nullptr;
    int guardZoneIndex = -1;

    for (int i = 0; i < guardZones.size(); i++) {
        if (guardZones[i].id == guardZoneId) {
            targetGuardZone = &guardZones[i];
            guardZoneIndex = i;
            break;
        }
    }

    if (!targetGuardZone) return;

    // Create context menu
    QMenu contextMenu(ecWidget);

    // Menu items
    QAction* editAction = contextMenu.addAction(QIcon(), tr("Edit GuardZone"));
    QAction* renameAction = contextMenu.addAction(QIcon(), tr("Rename GuardZone"));
    QAction* changeColorAction = contextMenu.addAction(QIcon(), tr("Change Color"));

    contextMenu.addSeparator();

    QAction* toggleActiveAction;
    if (targetGuardZone->active) {
        toggleActiveAction = contextMenu.addAction(QIcon(), tr("Disable GuardZone"));
    } else {
        toggleActiveAction = contextMenu.addAction(QIcon(), tr("Enable GuardZone"));
    }

    QAction* attachToShipAction;
    if (targetGuardZone->attachedToShip) {
        attachToShipAction = contextMenu.addAction(QIcon(), tr("Detach from Ship"));
    } else {
        attachToShipAction = contextMenu.addAction(QIcon(), tr("Attach to Ship"));
    }

    contextMenu.addSeparator();

    QAction* checkAction = contextMenu.addAction(QIcon(), tr("Check GuardZone"));
    QAction* propertiesAction = contextMenu.addAction(QIcon(), tr("Properties"));

    contextMenu.addSeparator();

    QAction* deleteAction = contextMenu.addAction(QIcon(), tr("Delete GuardZone"));
    deleteAction->setIcon(QIcon(":/images/delete.png")); // Jika ada icon

    // Execute menu
    QAction* selectedAction = contextMenu.exec(ecWidget->mapToGlobal(pos));

    if (!selectedAction) return;

    // Handle selected action
    if (selectedAction == editAction) {
        startEditGuardZone(guardZoneId);
    }
    else if (selectedAction == renameAction) {
        renameGuardZone(guardZoneId);
    }
    else if (selectedAction == changeColorAction) {
        changeGuardZoneColor(guardZoneId);
    }
    else if (selectedAction == toggleActiveAction) {
        toggleGuardZoneActive(guardZoneId);
    }
    else if (selectedAction == attachToShipAction) {
        toggleGuardZoneAttachToShip(guardZoneId);
    }
    else if (selectedAction == checkAction) {
        checkGuardZone(guardZoneId);
    }
    else if (selectedAction == propertiesAction) {
        showGuardZoneProperties(guardZoneId);
    }
    else if (selectedAction == deleteAction) {
        deleteGuardZone(guardZoneId);
    }
}

void GuardZoneManager::drawEditOverlay(QPainter& painter)
{
    if (!editingGuardZone || !ecWidget) return;

    // Find guardzone yang sedang diedit
    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    GuardZone* editingGz = nullptr;

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId) {
            editingGz = &gz;
            break;
        }
    }

    if (!editingGz) return;

    // Draw edit indicators
    painter.setPen(QPen(Qt::yellow, 3, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);

    if (editingGz->shape == GUARD_ZONE_CIRCLE) {
        int centerX, centerY;
        if (ecWidget->LatLonToXy(editingGz->centerLat, editingGz->centerLon, centerX, centerY)) {
            double radiusInPixels = ecWidget->calculatePixelsFromNauticalMiles(editingGz->radius);

            // Draw edit circle outline
            painter.drawEllipse(QPoint(centerX, centerY),
                                static_cast<int>(radiusInPixels) + 5,
                                static_cast<int>(radiusInPixels) + 5);
        }
    }
    else if (editingGz->shape == GUARD_ZONE_POLYGON) {
        QPolygon poly;
        for (int i = 0; i < editingGz->latLons.size(); i += 2) {
            int x, y;
            if (ecWidget->LatLonToXy(editingGz->latLons[i], editingGz->latLons[i+1], x, y)) {
                poly.append(QPoint(x, y));
            }
        }

        if (poly.size() >= 3) {
            // Draw edit polygon outline
            painter.setPen(QPen(Qt::yellow, 3, Qt::DashLine));
            painter.drawPolygon(poly);
        }
    }

    // ========== DRAW EDIT HANDLES ==========
    drawEditHandles(painter);
    // =====================================

    // Draw edit instructions
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QBrush(QColor(255, 255, 0, 200)));

    QRect instructionRect(10, 10, 500, 110); // Increased size for more text
    painter.drawRect(instructionRect);

    painter.setPen(QPen(Qt::black, 2));
    painter.setFont(QFont("Arial", 10, QFont::Bold));

    QString instructionText = tr("ADVANCED EDIT MODE: %1\n").arg(editingGz->name);

    if (editingGz->shape == GUARD_ZONE_CIRCLE) {
        instructionText += tr("• Drag blue handle to move center\n");
        instructionText += tr("• Drag green handle to change radius\n");
    } else if (editingGz->shape == GUARD_ZONE_POLYGON) {
        instructionText += tr("• Drag yellow handles to move vertices\n");
        instructionText += tr("• Left-click cyan circles to add vertex\n");
        instructionText += tr("• Right-click yellow handles to remove vertex\n");
    }

    instructionText += tr("• Right-click for menu options\n");
    instructionText += tr("• Press ESC to finish editing");

    painter.drawText(instructionRect.adjusted(10, 10, -10, -10), Qt::AlignLeft | Qt::AlignTop, instructionText);

    // ========== DRAW STATUS INDICATOR ==========
    // Draw a small status box showing current edit state
    QRect statusRect(ecWidget->width() - 220, 10, 200, 60);

    // Background with gradient
    QLinearGradient gradient(statusRect.topLeft(), statusRect.bottomLeft());
    gradient.setColorAt(0, QColor(0, 150, 255, 220));
    gradient.setColorAt(1, QColor(0, 100, 200, 220));
    painter.fillRect(statusRect, gradient);

    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(statusRect);

    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.setPen(QPen(Qt::white, 1));

    QString statusText = tr("🛡️ EDITING GUARDZONE\n");
    statusText += tr("ID: %1\n").arg(editingGz->id);

    if (editingGz->shape == GUARD_ZONE_CIRCLE) {
        statusText += tr("Type: Circle (Radius: %1 NM)").arg(editingGz->radius, 0, 'f', 2);
    } else if (editingGz->shape == GUARD_ZONE_POLYGON) {
        statusText += tr("Type: Polygon (%1 vertices)").arg(editingGz->latLons.size() / 2);
    }

    painter.drawText(statusRect.adjusted(8, 8, -8, -8), Qt::AlignLeft | Qt::AlignTop, statusText);
    // ========================================

    // ========== DRAW MODIFICATION INDICATOR ==========
    if (hasOriginalBackup) {
        // Show that changes have been made
        QRect modifiedRect(ecWidget->width() - 220, 80, 200, 30);

        painter.fillRect(modifiedRect, QColor(255, 165, 0, 200)); // Orange background
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(modifiedRect);

        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawText(modifiedRect, Qt::AlignCenter, tr("⚠ MODIFIED - Unsaved Changes"));
    }
    // ===============================================

    // ========== DRAW CURRENT DRAG STATE ==========
    if (isDragging && draggedHandleIndex >= 0 && draggedHandleIndex < editHandles.size()) {
        const EditHandle& handle = editHandles[draggedHandleIndex];

        // Draw info about what's being dragged
        QRect dragInfoRect(10, 130, 300, 40);
        painter.fillRect(dragInfoRect, QColor(255, 255, 255, 220));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(dragInfoRect);

        painter.setFont(QFont("Arial", 9, QFont::Bold));
        painter.setPen(QPen(Qt::black, 1));

        QString dragText;
        switch (handle.type) {
        case HANDLE_CENTER:
            dragText = tr("🔵 Dragging: Center Position");
            break;
        case HANDLE_RADIUS:
            dragText = tr("🟢 Dragging: Circle Radius");
            break;
        case HANDLE_VERTEX:
            dragText = tr("🟡 Dragging: Vertex %1").arg(handle.vertexIndex + 1);
            break;
        default:
            dragText = tr("Dragging: Unknown Handle");
            break;
        }

        painter.drawText(dragInfoRect, Qt::AlignCenter, dragText);
    }
    // =============================================
}

bool GuardZoneManager::handleKeyPress(QKeyEvent *e)
{
    if (!editingGuardZone) return false;

    switch (e->key()) {
    case Qt::Key_Escape:
        finishEditGuardZone();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finishEditGuardZone();
        return true;
    default:
        return false;
    }
}

void GuardZoneManager::renameGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            bool ok;
            QString newName = QInputDialog::getText(ecWidget, tr("Rename GuardZone"),
                                                    tr("Enter new name for GuardZone:"),
                                                    QLineEdit::Normal, gz.name, &ok);
            if (ok && !newName.isEmpty()) {
                gz.name = newName;
                ecWidget->saveGuardZones();
                ecWidget->update();
                emit statusMessage(tr("GuardZone renamed to: %1").arg(newName));
            }
            break;
        }
    }
}

void GuardZoneManager::changeGuardZoneColor(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            QColor newColor = QColorDialog::getColor(gz.color, ecWidget,
                                                     tr("Choose Color for %1").arg(gz.name));

            if (newColor.isValid() && newColor != gz.color) {
                gz.color = newColor;
                ecWidget->saveGuardZones();
                ecWidget->update();

                emit statusMessage(tr("GuardZone %1 color changed to %2")
                                       .arg(gz.name).arg(newColor.name()));
            } else {
                emit statusMessage(tr("Color change cancelled"));
            }
            return;
        }
    }
}

void GuardZoneManager::toggleGuardZoneActive(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            gz.active = !gz.active;
            ecWidget->saveGuardZones();
            ecWidget->update();

            QString status = gz.active ? tr("enabled") : tr("disabled");
            emit statusMessage(tr("GuardZone %1 %2").arg(gz.name).arg(status));
            break;
        }
    }
}

void GuardZoneManager::toggleGuardZoneAttachToShip(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            gz.attachedToShip = !gz.attachedToShip;
            ecWidget->saveGuardZones();
            ecWidget->update();

            QString status = gz.attachedToShip ? tr("attached to ship") : tr("detached from ship");
            emit statusMessage(tr("GuardZone %1 %2").arg(gz.name).arg(status));
            break;
        }
    }
}

void GuardZoneManager::checkGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    // Call existing check guardzone functionality
    ecWidget->checkGuardZone();
    emit statusMessage(tr("GuardZone check completed"));
}

void GuardZoneManager::showGuardZoneProperties(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (const GuardZone& gz : guardZones) {
        if (gz.id == guardZoneId) {
            QString properties;
            properties += tr("GuardZone Properties\n");
            properties += tr("═══════════════════\n");
            properties += tr("ID: %1\n").arg(gz.id);
            properties += tr("Name: %1\n").arg(gz.name);
            properties += tr("Shape: %1\n").arg(gz.shape == GUARD_ZONE_CIRCLE ? tr("Circle") : tr("Polygon"));
            properties += tr("Active: %1\n").arg(gz.active ? tr("Yes") : tr("No"));
            properties += tr("Attached to Ship: %1\n").arg(gz.attachedToShip ? tr("Yes") : tr("No"));
            properties += tr("Color: %1\n").arg(gz.color.name());

            if (gz.shape == GUARD_ZONE_CIRCLE) {
                properties += tr("Center: %1, %2\n").arg(gz.centerLat, 0, 'f', 6).arg(gz.centerLon, 0, 'f', 6);
                properties += tr("Radius: %1 NM\n").arg(gz.radius, 0, 'f', 2);
            } else {
                properties += tr("Points: %1\n").arg(gz.latLons.size() / 2);
            }

            QMessageBox::information(ecWidget, tr("GuardZone Properties"), properties);
            break;
        }
    }
}

void GuardZoneManager::deleteGuardZone(int guardZoneId)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (int i = 0; i < guardZones.size(); i++) {
        if (guardZones[i].id == guardZoneId) {
            QString name = guardZones[i].name;

            QMessageBox::StandardButton reply = QMessageBox::question(
                ecWidget, tr("Delete GuardZone"),
                tr("Are you sure you want to delete GuardZone '%1'?").arg(name),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // Clear edit state jika sedang diedit
                if (editingGuardZone && editingGuardZoneId == guardZoneId) {
                    finishEditGuardZone();
                }

                // Hapus dari list
                guardZones.removeAt(i);

                // Update system status
                if (guardZones.isEmpty()) {
                    ecWidget->enableGuardZone(false);
                } else {
                    ecWidget->enableGuardZone(true);
                }

                // PERBAIKAN: Proper sequence - Save → Signal → Update
                ecWidget->saveGuardZones();

                // Emit melalui EcWidget untuk konsistensi
                ecWidget->emitGuardZoneDeleted();

                ecWidget->update();
                QApplication::processEvents();

                emit statusMessage(tr("GuardZone '%1' deleted").arg(name));
            }
            return;
        }
    }
}

// ========== ADVANCED EDIT HELPER METHODS ==========

void GuardZoneManager::updateEditHandles()
{
    clearEditHandles();

    if (!ecWidget || !editingGuardZone) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    GuardZone* editingGz = nullptr;

    // Find the guardzone being edited
    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId) {
            editingGz = &gz;
            break;
        }
    }

    if (!editingGz) return;

    if (editingGz->shape == GUARD_ZONE_CIRCLE) {
        // 1. Center handle
        EditHandle centerHandle;
        centerHandle.type = HANDLE_CENTER;
        centerHandle.lat = editingGz->centerLat;
        centerHandle.lon = editingGz->centerLon;
        centerHandle.visible = true;
        centerHandle.hovered = false;

        int centerX, centerY;
        if (ecWidget->LatLonToXy(editingGz->centerLat, editingGz->centerLon, centerX, centerY)) {
            centerHandle.position = QPoint(centerX, centerY);
            editHandles.append(centerHandle);
        }

        // 2. Radius handle (on the edge of circle)
        EditHandle radiusHandle;
        radiusHandle.type = HANDLE_RADIUS;
        radiusHandle.visible = true;
        radiusHandle.hovered = false;

        // Calculate position on circle edge (to the right of center)
        double radiusLat, radiusLon;
        EcCalculateRhumblinePosition(EC_GEO_DATUM_WGS84,
                                     editingGz->centerLat, editingGz->centerLon,
                                     editingGz->radius, 90.0, // East direction
                                     &radiusLat, &radiusLon);

        radiusHandle.lat = radiusLat;
        radiusHandle.lon = radiusLon;

        int radiusX, radiusY;
        if (ecWidget->LatLonToXy(radiusLat, radiusLon, radiusX, radiusY)) {
            radiusHandle.position = QPoint(radiusX, radiusY);
            editHandles.append(radiusHandle);
        }
    }
    else if (editingGz->shape == GUARD_ZONE_POLYGON && editingGz->latLons.size() >= 6) {
        // Create handles for polygon vertices
        for (int i = 0; i < editingGz->latLons.size(); i += 2) {
            EditHandle vertexHandle;
            vertexHandle.type = HANDLE_VERTEX;
            vertexHandle.vertexIndex = i / 2;
            vertexHandle.lat = editingGz->latLons[i];
            vertexHandle.lon = editingGz->latLons[i + 1];
            vertexHandle.visible = true;
            vertexHandle.hovered = false;

            int x, y;
            if (ecWidget->LatLonToXy(vertexHandle.lat, vertexHandle.lon, x, y)) {
                vertexHandle.position = QPoint(x, y);
                editHandles.append(vertexHandle);
            }
        }
    }
}

void GuardZoneManager::clearEditHandles()
{
    editHandles.clear();
}

int GuardZoneManager::findHandleAt(const QPoint& pos)
{
    for (int i = 0; i < editHandles.size(); i++) {
        if (!editHandles[i].visible) continue;

        double distance = sqrt(pow(pos.x() - editHandles[i].position.x(), 2) +
                               pow(pos.y() - editHandles[i].position.y(), 2));

        if (distance <= HANDLE_DETECT_RADIUS) {
            return i;
        }
    }

    return -1; // No handle found
}

void GuardZoneManager::startDragHandle(int handleIndex, const QPoint& startPos)
{
    if (handleIndex < 0 || handleIndex >= editHandles.size()) return;

    draggedHandleIndex = handleIndex;
    dragStartPos = startPos;
    lastDragPos = startPos;
    isDragging = true;

    const EditHandle& handle = editHandles[handleIndex];

    // Set edit mode based on handle type
    switch (handle.type) {
    case HANDLE_CENTER:
        currentEditMode = EDIT_DRAG_CENTER;
        emit statusMessage(tr("Dragging center of GuardZone"));
        break;
    case HANDLE_RADIUS:
        currentEditMode = EDIT_DRAG_RADIUS;
        emit statusMessage(tr("Dragging radius of GuardZone"));
        break;
    case HANDLE_VERTEX:
        currentEditMode = EDIT_DRAG_VERTEX;
        emit statusMessage(tr("Dragging vertex %1 of GuardZone").arg(handle.vertexIndex + 1));
        break;
    default:
        break;
    }
}

void GuardZoneManager::updateDragHandle(const QPoint& currentPos)
{
    if (!isDragging || draggedHandleIndex < 0 || !ecWidget) return;

    const EditHandle& handle = editHandles[draggedHandleIndex];

    // Convert screen position to lat/lon
    double newLat, newLon;
    if (!ecWidget->XyToLatLon(currentPos.x(), currentPos.y(), newLat, newLon)) {
        return; // Invalid coordinates
    }

    // Update guardzone based on handle type
    switch (handle.type) {
    case HANDLE_CENTER:
        // Move center of circle
        updateCircleCenter(newLat, newLon);
        break;
    case HANDLE_RADIUS:
        // Update radius of circle
        updateCircleRadius(newLat, newLon);
        break;
    case HANDLE_VERTEX:
        // Move polygon vertex
        updatePolygonVertex(handle.vertexIndex, newLat, newLon);
        break;
    default:
        break;
    }

    // Update handle positions
    updateEditHandles();

    lastDragPos = currentPos;
}

void GuardZoneManager::finishDragHandle()
{
    if (!isDragging) return;

    isDragging = false;
    draggedHandleIndex = -1;
    currentEditMode = EDIT_NONE;

    // PERBAIKAN: Save dan emit signal hanya sekali di sini
    if (ecWidget) {
        ecWidget->saveGuardZones();
        // Emit signal melalui EcWidget untuk konsistensi
        ecWidget->emitGuardZoneModified();
    }

    emit statusMessage(tr("GuardZone modified. Drag handles to continue editing."));

    // PERBAIKAN: Emit manager signal terakhir (tidak bentrok)
    emit guardZoneModified(editingGuardZoneId);
}

void GuardZoneManager::updateCircleCenter(double newLat, double newLon)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId && gz.shape == GUARD_ZONE_CIRCLE) {
            gz.centerLat = newLat;
            gz.centerLon = newLon;
            break;
        }
    }

    // PERBAIKAN: Jangan emit signal setiap kali drag, hanya saat selesai
    // Signal akan di-emit di finishDragHandle()
}

void GuardZoneManager::updateCircleRadius(double newLat, double newLon)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId && gz.shape == GUARD_ZONE_CIRCLE) {
            double distance, bearing;
            EcCalculateRhumblineDistanceAndBearing(EC_GEO_DATUM_WGS84,
                                                   gz.centerLat, gz.centerLon,
                                                   newLat, newLon,
                                                   &distance, &bearing);

            if (distance < 0.01) distance = 0.01;
            if (distance > 100) distance = 100;

            gz.radius = distance;
            break;
        }
    }

    // PERBAIKAN: Jangan emit signal setiap kali drag
}

void GuardZoneManager::updatePolygonVertex(int vertexIndex, double newLat, double newLon)
{
    if (!ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId && gz.shape == GUARD_ZONE_POLYGON) {
            int latIndex = vertexIndex * 2;
            int lonIndex = latIndex + 1;

            if (latIndex < gz.latLons.size() && lonIndex < gz.latLons.size()) {
                gz.latLons[latIndex] = newLat;
                gz.latLons[lonIndex] = newLon;
            }
            break;
        }
    }

    // PERBAIKAN: Jangan emit signal setiap kali drag
}


void GuardZoneManager::drawEditHandles(QPainter& painter)
{
    if (!editingGuardZone) return;

    // Draw existing vertex handles
    for (const EditHandle& handle : editHandles) {
        if (!handle.visible) continue;

        int size = handle.hovered ? HANDLE_HOVER_SIZE : HANDLE_SIZE;

        // Choose color based on handle type
        QColor handleColor;
        QColor borderColor = Qt::white;
        switch (handle.type) {
        case HANDLE_CENTER:
            handleColor = Qt::blue;
            break;
        case HANDLE_RADIUS:
            handleColor = Qt::green;
            break;
        case HANDLE_VERTEX:
            handleColor = Qt::yellow;
            borderColor = Qt::black; // Different border for vertex handles
            break;
        default:
            handleColor = Qt::white;
            break;
        }

        // Draw handle with enhanced visual feedback
        painter.setPen(QPen(borderColor, 2));
        painter.setBrush(QBrush(handleColor));
        painter.drawEllipse(handle.position, size, size);

        // Draw inner highlight for vertex handles
        if (handle.type == HANDLE_VERTEX) {
            painter.setPen(QPen(Qt::white, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(handle.position, size - 2, size - 2);

            // Add vertex number
            painter.setPen(QPen(Qt::black, 2));
            painter.setFont(QFont("Arial", 8, QFont::Bold));
            painter.drawText(handle.position + QPoint(-3, 3), QString::number(handle.vertexIndex + 1));
        } else {
            // Regular border for non-vertex handles
            painter.setPen(QPen(borderColor, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(handle.position, size, size);
        }
    }

    // ========== ENHANCED EDGE INDICATORS FOR POLYGON ==========
    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    GuardZone* editingGz = nullptr;

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId) {
            editingGz = &gz;
            break;
        }
    }

    if (editingGz && editingGz->shape == GUARD_ZONE_POLYGON && editingGz->latLons.size() >= 6) {
        // Draw enhanced edge indicators
        QVector<QPoint> screenPoints;
        for (int i = 0; i < editingGz->latLons.size(); i += 2) {
            int x, y;
            if (ecWidget->LatLonToXy(editingGz->latLons[i], editingGz->latLons[i+1], x, y)) {
                screenPoints.append(QPoint(x, y));
            }
        }

        if (screenPoints.size() >= 3) {
            for (int i = 0; i < screenPoints.size(); i++) {
                int nextIndex = (i + 1) % screenPoints.size();
                QPoint p1 = screenPoints[i];
                QPoint p2 = screenPoints[nextIndex];

                // Draw small clickable indicator at midpoint only
                QPoint midPoint = (p1 + p2) / 2;

                // Draw small circle to indicate add vertex point
                painter.setPen(QPen(Qt::cyan, 2));
                painter.setBrush(QBrush(QColor(0, 255, 255, 150)));
                painter.drawEllipse(midPoint, 5, 5);

                // Draw small + to make it clearer
                painter.setPen(QPen(Qt::white, 2));
                painter.drawLine(midPoint.x() - 3, midPoint.y(), midPoint.x() + 3, midPoint.y());
                painter.drawLine(midPoint.x(), midPoint.y() - 3, midPoint.x(), midPoint.y() + 3);
            }
        }
    }
    // =========================================================
}

void GuardZoneManager::refreshHandlePositions()
{
    if (!editingGuardZone || !ecWidget || editHandles.isEmpty()) return;

    // Update screen positions berdasarkan koordinat geografis yang tersimpan
    for (int i = 0; i < editHandles.size(); i++) {
        int x, y;
        if (ecWidget->LatLonToXy(editHandles[i].lat, editHandles[i].lon, x, y)) {
            editHandles[i].position = QPoint(x, y);
        }
    }

    qDebug() << "Handle positions refreshed for" << editHandles.size() << "handles";
}

int GuardZoneManager::findEdgeAt(const QPoint& pos)
{
    if (!editingGuardZone || !ecWidget) return -1;

    // ========== FIRST CHECK: MAKE SURE NOT CLICKING ON HANDLE ==========
    int handleIndex = findHandleAt(pos);
    if (handleIndex >= 0) {
        return -1; // Don't process edge if clicking on handle
    }
    // ==================================================================

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    GuardZone* editingGz = nullptr;

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId) {
            editingGz = &gz;
            break;
        }
    }

    if (!editingGz || editingGz->shape != GUARD_ZONE_POLYGON) return -1;

    // Convert latlon to screen coordinates
    QVector<QPoint> screenPoints;
    for (int i = 0; i < editingGz->latLons.size(); i += 2) {
        int x, y;
        if (ecWidget->LatLonToXy(editingGz->latLons[i], editingGz->latLons[i+1], x, y)) {
            screenPoints.append(QPoint(x, y));
        }
    }

    if (screenPoints.size() < 3) return -1;

    // Check distance to each edge (avoid areas near vertices)
    for (int i = 0; i < screenPoints.size(); i++) {
        int nextIndex = (i + 1) % screenPoints.size();
        QPoint p1 = screenPoints[i];
        QPoint p2 = screenPoints[nextIndex];

        // Calculate distance from point to line segment
        double distance = distanceToLineSegment(pos, p1, p2);

        // Check if click is near middle of edge, not near vertices
        QPoint midPoint = (p1 + p2) / 2;
        double distanceToMid = sqrt(pow(pos.x() - midPoint.x(), 2) + pow(pos.y() - midPoint.y(), 2));

        // Only consider as edge click if:
        // 1. Close to the line (distance <= 15)
        // 2. Reasonably close to middle of edge (not too near vertices)
        if (distance <= 15.0 && distanceToMid <= 30.0) {
            qDebug() << "Edge" << i << "clicked, distance to line:" << distance << "distance to mid:" << distanceToMid;
            return i;
        }
    }

    return -1;
}

void GuardZoneManager::addVertexToPolygon(int edgeIndex, const QPoint& pos)
{
    if (!editingGuardZone || !ecWidget) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId && gz.shape == GUARD_ZONE_POLYGON) {
            double newLat, newLon;
            if (ecWidget->XyToLatLon(pos.x(), pos.y(), newLat, newLon)) {
                int insertIndex = (edgeIndex + 1) * 2;

                if (insertIndex <= gz.latLons.size()) {
                    gz.latLons.insert(insertIndex, newLat);
                    gz.latLons.insert(insertIndex + 1, newLon);

                    // Update handles
                    updateEditHandles();

                    // PERBAIKAN: Save dan emit signal secara controlled
                    ecWidget->saveGuardZones();
                    ecWidget->update();

                    int vertexNumber = (edgeIndex + 1) + 1;
                    emit statusMessage(tr("New vertex added at position %1 (Total: %2 vertices)")
                                           .arg(vertexNumber)
                                           .arg(gz.latLons.size() / 2));

                    // PERBAIKAN: Emit hanya manager signal, bukan EcWidget signal
                    emit guardZoneModified(editingGuardZoneId);

                    qDebug() << "Added vertex to polygon at edge" << edgeIndex
                             << "New vertex count:" << (gz.latLons.size() / 2);
                }
            }
            break;
        }
    }
}

double GuardZoneManager::distanceToLineSegment(const QPoint& point, const QPoint& lineStart, const QPoint& lineEnd)
{
    double A = point.x() - lineStart.x();
    double B = point.y() - lineStart.y();
    double C = lineEnd.x() - lineStart.x();
    double D = lineEnd.y() - lineStart.y();

    double dot = A * C + B * D;
    double lenSq = C * C + D * D;

    if (lenSq == 0) {
        // Line segment is a point
        return sqrt(A * A + B * B);
    }

    double param = dot / lenSq;

    double xx, yy;

    if (param < 0) {
        xx = lineStart.x();
        yy = lineStart.y();
    } else if (param > 1) {
        xx = lineEnd.x();
        yy = lineEnd.y();
    } else {
        xx = lineStart.x() + param * C;
        yy = lineStart.y() + param * D;
    }

    double dx = point.x() - xx;
    double dy = point.y() - yy;
    return sqrt(dx * dx + dy * dy);
}

void GuardZoneManager::removeVertexFromPolygon(int handleIndex)
{
    if (!editingGuardZone || !ecWidget || handleIndex < 0 || handleIndex >= editHandles.size()) return;

    const EditHandle& handle = editHandles[handleIndex];

    // Only allow removal of vertex handles
    if (handle.type != HANDLE_VERTEX) return;

    QList<GuardZone>& guardZones = ecWidget->getGuardZones();

    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId && gz.shape == GUARD_ZONE_POLYGON) {
            // Check minimum vertex count (need at least 3 vertices = 6 coordinates)
            if (gz.latLons.size() <= 6) {
                emit statusMessage(tr("Cannot remove vertex: Polygon needs at least 3 vertices"));
                return;
            }

            // Remove the vertex (2 coordinates: lat and lon)
            int latIndex = handle.vertexIndex * 2;
            int lonIndex = latIndex + 1;

            if (latIndex < gz.latLons.size() && lonIndex < gz.latLons.size()) {
                double removedLat = gz.latLons[latIndex];
                double removedLon = gz.latLons[lonIndex];

                gz.latLons.removeAt(lonIndex); // Remove lon first (higher index)
                gz.latLons.removeAt(latIndex); // Then remove lat

                // Update edit handles
                updateEditHandles();

                // Save changes
                ecWidget->saveGuardZones();

                // Force redraw
                ecWidget->update();

                emit statusMessage(tr("Vertex %1 removed (Remaining: %2 vertices)")
                                       .arg(handle.vertexIndex + 1)
                                       .arg(gz.latLons.size() / 2));
                emit guardZoneModified(editingGuardZoneId);

                ecWidget->emitGuardZoneModified();

                qDebug() << "Removed vertex" << handle.vertexIndex
                         << "at position" << removedLat << removedLon
                         << "Remaining vertices:" << (gz.latLons.size() / 2);
            }
            break;
        }
    }
}

void GuardZoneManager::showEditContextMenu(const QPoint& pos)
{
    if (!editingGuardZone || !ecWidget) return;

    // Create context menu
    QMenu contextMenu(ecWidget);

    // Get current guardzone info
    QList<GuardZone>& guardZones = ecWidget->getGuardZones();
    GuardZone* editingGz = nullptr;
    for (GuardZone& gz : guardZones) {
        if (gz.id == editingGuardZoneId) {
            editingGz = &gz;
            break;
        }
    }

    if (!editingGz) return;

    // ========== MENU ITEMS (STYLE KONSISTEN) ==========
    QAction* finishAction = contextMenu.addAction(QIcon(), tr("Finish Edit"));
    QAction* cancelAction = contextMenu.addAction(QIcon(), tr("Cancel Edit"));

    contextMenu.addSeparator();

    QAction* restoreAction = nullptr;
    if (hasOriginalBackup) {
        restoreAction = contextMenu.addAction(QIcon(), tr("Restore Original"));
    }

    contextMenu.addSeparator();

    // Rename (sama seperti context menu biasa)
    QAction* renameAction = contextMenu.addAction(QIcon(), tr("Rename GuardZone"));

    // Change Color (sama seperti context menu biasa)
    QAction* changeColorAction = contextMenu.addAction(QIcon(), tr("Change Color"));

    contextMenu.addSeparator();

    // Properties
    QAction* propertiesAction = contextMenu.addAction(QIcon(), tr("Properties"));

    // ========== EXECUTE MENU ==========
    QAction* selectedAction = contextMenu.exec(ecWidget->mapToGlobal(pos));

    if (!selectedAction) return;

    // Handle selected action
    if (selectedAction == finishAction) {
        finishEditWithSave();
    }
    else if (selectedAction == cancelAction) {
        cancelEditWithoutSave();
    }
    else if (restoreAction && selectedAction == restoreAction) {
        restoreToOriginalShape();
    }
    else if (selectedAction == renameAction) {
        renameGuardZone(editingGuardZoneId);
    }
    else if (selectedAction == changeColorAction) {
        changeGuardZoneColor(editingGuardZoneId);
    }
    else if (selectedAction == propertiesAction) {
        showGuardZoneProperties(editingGuardZoneId);
    }
}

void GuardZoneManager::finishEditWithSave()
{
    if (!editingGuardZone) return;

    qDebug() << "Finishing edit with save";

    // Save current state
    if (ecWidget) {
        ecWidget->saveGuardZones();
    }

    // Show success feedback (konsisten dengan feedback lainnya)
    emit statusMessage(tr("GuardZone changes saved"));

    // Clear backup
    hasOriginalBackup = false;

    // Finish edit mode
    finishEditGuardZone();
}

void GuardZoneManager::cancelEditWithoutSave()
{
    if (!editingGuardZone) return;

    // Show confirmation dialog (konsisten dengan dialog delete GuardZone)
    QMessageBox::StandardButton reply = QMessageBox::question(
        ecWidget, tr("Cancel Edit"),
        tr("Are you sure you want to cancel editing?\nAll changes will be discarded."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qDebug() << "Canceling edit without save";

        // Restore to original if backup exists
        if (hasOriginalBackup) {
            // Restore tanpa konfirmasi tambahan karena user sudah konfirmasi cancel
            QList<GuardZone>& guardZones = ecWidget->getGuardZones();

            for (GuardZone& gz : guardZones) {
                if (gz.id == editingGuardZoneId) {
                    // Keep current properties but restore shape
                    int currentId = gz.id;
                    QString currentName = gz.name;
                    QColor currentColor = gz.color;
                    bool currentActive = gz.active;
                    bool currentAttachedToShip = gz.attachedToShip;

                    // Restore shape data from backup
                    gz = originalGuardZone;

                    // Preserve current properties
                    gz.id = currentId;
                    gz.name = currentName;
                    gz.color = currentColor;
                    gz.active = currentActive;
                    gz.attachedToShip = currentAttachedToShip;

                    qDebug() << "Restored GuardZone to original shape during cancel";
                    break;
                }
            }
        }

        // Show feedback
        emit statusMessage(tr("Edit cancelled - changes discarded"));

        // Clear backup
        hasOriginalBackup = false;

        // Finish edit mode without saving
        finishEditGuardZone();
    }
}

void GuardZoneManager::restoreToOriginalShape()
{
    if (!editingGuardZone || !hasOriginalBackup || !ecWidget) return;

    // Show confirmation dialog (konsisten dengan style aplikasi)
    QMessageBox::StandardButton reply = QMessageBox::question(
        ecWidget, tr("Restore Original Shape"),
        tr("Are you sure you want to restore to original shape?\nAll current changes will be lost."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QList<GuardZone>& guardZones = ecWidget->getGuardZones();

        // Find and restore the guardzone
        for (GuardZone& gz : guardZones) {
            if (gz.id == editingGuardZoneId) {
                // Keep the current ID and name, but restore shape data
                int currentId = gz.id;
                QString currentName = gz.name;
                QColor currentColor = gz.color;
                bool currentActive = gz.active;
                bool currentAttachedToShip = gz.attachedToShip;

                // Restore shape data from backup
                gz = originalGuardZone;

                // Preserve current properties that shouldn't be restored
                gz.id = currentId;
                gz.name = currentName;
                gz.color = currentColor;
                gz.active = currentActive;
                gz.attachedToShip = currentAttachedToShip;

                qDebug() << "Restored GuardZone to original shape";
                break;
            }
        }

        // Update handles and redraw
        updateEditHandles();
        ecWidget->update();

        emit statusMessage(tr("GuardZone restored to original shape"));
        emit guardZoneModified(editingGuardZoneId);
    }
}
