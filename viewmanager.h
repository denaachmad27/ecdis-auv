#ifndef _view_manager_h_
#define _view_manager_h_

#include <QObject>
#include <QMdiArea>
#include <QList>
#include <QPointer>
#include <QString>

#include "chartviewcontainer.h"

// SevenCs types forward declarations (already available via chartviewcontainer.h)

/**
 * ViewManager - Manages multiple chart views within a QMdiArea
 *
 * Responsibilities:
 * - Create and manage ChartViewContainer instances
 * - Handle shared DENC across all views
 * - Provide view arrangement (tile, cascade, tab)
 */
class ViewManager : public QObject
{
    Q_OBJECT

public:
    explicit ViewManager(QMdiArea* mdiArea, EcDictInfo* dictInfo, QObject* parent = nullptr);
    virtual ~ViewManager();

    // DENC management
    void setSharedDENC(EcDENC* denc);
    void setLibraryString(QString* libStr) { m_libStr = libStr; }

    // View creation
    ChartViewContainer* createView(ChartViewContainer::ViewType type);
    ChartViewContainer* createS63View();
    ChartViewContainer* createSatelliteView();
    ChartViewContainer* createThematicView();

    // View management
    void closeView(ChartViewContainer* view);
    void closeCurrentView();
    void closeAllViews();
    QList<ChartViewContainer*> getViews() const;
    int getViewCount() const { return views.count(); }
    ChartViewContainer* getCurrentView() const;

    // Window arrangement (QMdiArea helpers)
    void tileViews();
    void cascadeViews();
    void tabViews();

    // View activation
    void activateView(ChartViewContainer* view);
    void activateNextView();
    void activatePreviousView();

signals:
    void viewCreated(ChartViewContainer* view);
    void viewClosing(ChartViewContainer* view);
    void viewCountChanged(int count);
    void currentViewChanged(ChartViewContainer* view);

private slots:
    void onSubWindowActivated(QMdiSubWindow* window);

private:
    QMdiArea* mdiArea;
    EcDictInfo* dictInfo;
    EcDENC* sharedDENC;
    QString* m_libStr;
    QList<ChartViewContainer*> views;

    QMdiSubWindow* wrapInSubWindow(ChartViewContainer* container);
    void connectViewSignals(ChartViewContainer* view);
};

#endif // _view_manager_h_
