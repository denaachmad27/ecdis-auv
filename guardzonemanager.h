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

// Forward declarations only
class EcWidget;
struct GuardZone;

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

signals:
    void editModeChanged(bool isEditing);
    void guardZoneModified(int guardZoneId);
    void statusMessage(const QString &message);

private:
    EcWidget* ecWidget;
    bool editingGuardZone;
    int editingGuardZoneId;

    // Constants
    static const double VERTEX_THRESHOLD;
    static const double EDGE_THRESHOLD;
};

#endif // GUARDZONEMANAGER_H
