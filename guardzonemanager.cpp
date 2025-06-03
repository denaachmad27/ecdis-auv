#include "guardzonemanager.h"
#include "ecwidget.h"

// Constants
const double GuardZoneManager::VERTEX_THRESHOLD = 15.0;
const double GuardZoneManager::EDGE_THRESHOLD = 10.0;

GuardZoneManager::GuardZoneManager(EcWidget* parent)
    : QObject(parent)
    , ecWidget(parent)
    , editingGuardZone(false)
    , editingGuardZoneId(-1)
{
}

GuardZoneManager::~GuardZoneManager()
{
}

void GuardZoneManager::startEditGuardZone(int guardZoneId)
{
    editingGuardZone = true;
    editingGuardZoneId = guardZoneId;
    emit editModeChanged(true);
    emit statusMessage(tr("Edit mode activated"));
}

void GuardZoneManager::finishEditGuardZone()
{
    editingGuardZone = false;
    editingGuardZoneId = -1;
    emit editModeChanged(false);
    emit statusMessage(tr("Edit mode finished"));
}

bool GuardZoneManager::handleMousePress(QMouseEvent *e)
{
    if (!editingGuardZone) return false;

    // Implementasi sederhana - bisa dikembangkan nanti
    qDebug() << "GuardZoneManager: Mouse press at" << e->pos();
    return true; // Return true berarti event sudah dihandle
}

bool GuardZoneManager::handleMouseMove(QMouseEvent *e)
{
    if (!editingGuardZone) return false;

    // Implementasi sederhana - bisa dikembangkan nanti
    return false; // Return false untuk saat ini
}

bool GuardZoneManager::handleMouseRelease(QMouseEvent *e)
{
    if (!editingGuardZone) return false;

    // Implementasi sederhana - bisa dikembangkan nanti
    return false; // Return false untuk saat ini
}

int GuardZoneManager::getGuardZoneAtPosition(int x, int y)
{
    // Implementasi sederhana - return -1 jika tidak ada guardzone
    Q_UNUSED(x)
    Q_UNUSED(y)
    return -1;
}

void GuardZoneManager::showGuardZoneContextMenu(const QPoint &pos, int guardZoneId)
{
    // Implementasi sederhana - bisa dikembangkan nanti
    Q_UNUSED(pos)
    Q_UNUSED(guardZoneId)
    qDebug() << "GuardZoneManager: Context menu for guardzone" << guardZoneId;
}

void GuardZoneManager::drawEditOverlay(QPainter& painter)
{
    if (!editingGuardZone) return;

    // Implementasi sederhana - gambar text sederhana
    painter.setPen(Qt::red);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(10, 30, tr("Edit Mode Active"));
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
