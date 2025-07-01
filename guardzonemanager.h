#ifndef GUARDZONEMANAGER_H
#define GUARDZONEMANAGER_H

#include <QObject>
#include <QWidget>
#include <QPoint>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QApplication>

// ========== PERBAIKAN INCLUDES ==========
#include "guardzone.h"  // Include GuardZone struct dan enum

// Forward declarations
class EcWidget;

// ========== TAMBAHAN INCLUDE UNTUK ECKERNEL ==========
#ifdef _WIN32
#include <windows.h>
#pragma pack(push, 4)
#include <eckernel.h>
#pragma pack (pop)
#else
#include <eckernel.h>
#endif
// ===================================================

class GuardZoneManager : public QObject
{
    Q_OBJECT

public:
    explicit GuardZoneManager(EcWidget* parent = nullptr);
    ~GuardZoneManager();

    // Fungsi utama
    void startEditGuardZone(int guardZoneId);
    void finishEditGuardZone();

    // Fungsi event handlers
    bool handleMousePress(QMouseEvent *e);
    bool handleMouseMove(QMouseEvent *e);
    bool handleMouseRelease(QMouseEvent *e);

    // Fungsi utility
    int getGuardZoneAtPosition(int x, int y);
    void showGuardZoneContextMenu(const QPoint &pos, int guardZoneId);
    void drawEditOverlay(QPainter& painter);

    // Getters
    bool isEditingGuardZone() const { return editingGuardZone; }
    int getEditingGuardZoneId() const { return editingGuardZoneId; }

    bool handleKeyPress(QKeyEvent *e);

    // Context menu methods
    void renameGuardZone(int guardZoneId);
    void changeGuardZoneColor(int guardZoneId);
    void toggleGuardZoneActive(int guardZoneId);
    void toggleGuardZoneAttachToShip(int guardZoneId);
    void checkGuardZone(int guardZoneId);
    void showGuardZoneProperties(int guardZoneId);
    void deleteGuardZone(int guardZoneId);
    void refreshHandlePositions();

public slots:
    void refreshGuardZoneList();           // Refresh semua guardzone
    void updateGuardZoneInList(int id);    // Update guardzone tertentu

signals:
    void editModeChanged(bool isEditing);
    void guardZoneModified(int guardZoneId);
    void statusMessage(const QString &message);

private:
    // ========== ADVANCED EDIT FEATURES ==========
    enum EditMode {
        EDIT_NONE,
        EDIT_DRAG_CENTER,     // Drag center of circle
        EDIT_DRAG_RADIUS,     // Drag radius of circle
        EDIT_DRAG_VERTEX,     // Drag vertex of polygon
        EDIT_DRAG_EDGE        // Drag edge of polygon (future)
    };

    enum HandleType {
        HANDLE_NONE,
        HANDLE_CENTER,        // Center point of circle
        HANDLE_RADIUS,        // Radius handle of circle
        HANDLE_VERTEX,         // Vertex of polygon
        HANDLE_EDGE           // Edge of polygon
    };

    struct EditHandle {
        HandleType type;
        QPoint position;      // Screen coordinates
        int vertexIndex;      // For polygon vertices
        double lat, lon;      // Geographic coordinates
        bool visible;
        bool hovered;

        EditHandle() : type(HANDLE_NONE), vertexIndex(-1), lat(0), lon(0), visible(false), hovered(false) {}
    };

    // Edit state variables
    EditMode currentEditMode;
    QList<EditHandle> editHandles;
    int draggedHandleIndex;
    QPoint dragStartPos;
    QPoint lastDragPos;
    bool isDragging;

    // Visual settings
    static const int HANDLE_SIZE;
    static const int HANDLE_HOVER_SIZE;
    static const double HANDLE_DETECT_RADIUS;

    // Methods for advanced editing
    void updateEditHandles();
    void clearEditHandles();
    int findHandleAt(const QPoint& pos);
    void startDragHandle(int handleIndex, const QPoint& startPos);
    void updateDragHandle(const QPoint& currentPos);
    void finishDragHandle();
    void drawEditHandles(QPainter& painter);
    void updateCircleCenter(double newLat, double newLon);
    void updateCircleRadius(double newLat, double newLon);
    void updatePolygonVertex(int vertexIndex, double newLat, double newLon);
    void removeVertexFromPolygon(int handleIndex);
    void showEditContextMenu(const QPoint& pos);
    void finishEditWithSave();
    void cancelEditWithoutSave();
    void restoreToOriginalShape();
    // =============================================

    EcWidget* ecWidget;
    bool editingGuardZone;
    int editingGuardZoneId;

    // ========== ORIGINAL SHAPE BACKUP ==========
    GuardZone originalGuardZone;  // Backup of original guardzone before editing
    bool hasOriginalBackup;
    // ==========================================

    // Constants
    static const double VERTEX_THRESHOLD;
    static const double EDGE_THRESHOLD;

    int findEdgeAt(const QPoint& pos);
    void addVertexToPolygon(int edgeIndex, const QPoint& pos);
    double distanceToLineSegment(const QPoint& point, const QPoint& lineStart, const QPoint& lineEnd);
};

#endif // GUARDZONEMANAGER_H
