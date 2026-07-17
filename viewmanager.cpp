#include "viewmanager.h"  // This already includes chartviewcontainer.h which includes ecwidget.h
#include <QMdiSubWindow>
#include <QDebug>

ViewManager::ViewManager(QMdiArea* mdiArea, EcDictInfo* dictInfo, QObject* parent)
    : QObject(parent)
    , mdiArea(mdiArea)
    , dictInfo(dictInfo)
    , sharedDENC(nullptr)
    , m_libStr(nullptr)
{
    // Connect to QMdiArea signal for tracking active view changes
    connect(mdiArea, &QMdiArea::subWindowActivated,
            this, &ViewManager::onSubWindowActivated);
}

ViewManager::~ViewManager()
{
    // Cleanup is handled by Qt's parent system
}

void ViewManager::setSharedDENC(EcDENC* denc)
{
    sharedDENC = denc;
    // Attach shared DENC to all existing views
    for (ChartViewContainer* view : views) {
        if (view) {
            view->attachSharedDENC(denc);
        }
    }
}

QMdiSubWindow* ViewManager::wrapInSubWindow(ChartViewContainer* container)
{
    QMdiSubWindow* subWindow = new QMdiSubWindow();
    subWindow->setWidget(container);
    subWindow->setWindowTitle(container->getTitle());

    // Allow subwindow to be maximized, minimized, closed
    subWindow->setWindowIcon(QIcon(":/icon/samudra_resize.png"));

    return subWindow;
}

void ViewManager::connectViewSignals(ChartViewContainer* view)
{
    if (!view) return;

    connect(view, &ChartViewContainer::titleChanged, this, [this, view](const QString& title) {
        // Find the subwindow containing this view and update its title
        QList<QMdiSubWindow*> windows = mdiArea->subWindowList();
        for (QMdiSubWindow* window : windows) {
            if (window->widget() == view) {
                window->setWindowTitle(title);
                break;
            }
        }
    });

    connect(view, &QObject::destroyed, this, [this, view]() {
        views.removeAll(view);
        emit viewCountChanged(views.count());
    });
}

ChartViewContainer* ViewManager::createView(ChartViewContainer::ViewType type)
{
    if (!dictInfo) {
        qWarning() << "ViewManager: Cannot create view - dictInfo is null";
        return nullptr;
    }

    ChartViewContainer* container = new ChartViewContainer(dictInfo, m_libStr);
    container->setViewType(type);

    // Attach shared DENC if available
    if (sharedDENC) {
        container->attachSharedDENC(sharedDENC);
    }

    // Wrap in MDI subwindow
    QMdiSubWindow* subWindow = wrapInSubWindow(container);
    mdiArea->addSubWindow(subWindow);

    // Track view
    views.append(container);
    connectViewSignals(container);

    // Show the view
    subWindow->show();

    emit viewCreated(container);
    emit viewCountChanged(views.count());

    qDebug() << "ViewManager: Created" << container->getTitle();

    return container;
}

ChartViewContainer* ViewManager::createS63View()
{
    return createView(ChartViewContainer::VIEW_S63);
}

ChartViewContainer* ViewManager::createSatelliteView()
{
    return createView(ChartViewContainer::VIEW_SATELLITE);
}

ChartViewContainer* ViewManager::createThematicView()
{
    return createView(ChartViewContainer::VIEW_THEMATIC);
}

void ViewManager::closeView(ChartViewContainer* view)
{
    if (!view) return;

    QList<QMdiSubWindow*> windows = mdiArea->subWindowList();
    for (QMdiSubWindow* window : windows) {
        if (window->widget() == view) {
            emit viewClosing(view);
            window->close();
            break;
        }
    }
}

void ViewManager::closeCurrentView()
{
    QMdiSubWindow* current = mdiArea->currentSubWindow();
    if (current) {
        ChartViewContainer* view = qobject_cast<ChartViewContainer*>(current->widget());
        if (view) {
            emit viewClosing(view);
        }
        current->close();
    }
}

void ViewManager::closeAllViews()
{
    QList<QMdiSubWindow*> windows = mdiArea->subWindowList();
    for (QMdiSubWindow* window : windows) {
        ChartViewContainer* view = qobject_cast<ChartViewContainer*>(window->widget());
        if (view) {
            emit viewClosing(view);
        }
        window->close();
    }
    views.clear();
    emit viewCountChanged(0);
}

QList<ChartViewContainer*> ViewManager::getViews() const
{
    return views;
}

ChartViewContainer* ViewManager::getCurrentView() const
{
    QMdiSubWindow* current = mdiArea->currentSubWindow();
    if (current) {
        return qobject_cast<ChartViewContainer*>(current->widget());
    }
    return nullptr;
}

void ViewManager::tileViews()
{
    mdiArea->tileSubWindows();
}

void ViewManager::cascadeViews()
{
    mdiArea->cascadeSubWindows();
}

void ViewManager::tabViews()
{
    mdiArea->setViewMode(QMdiArea::TabbedView);
}

void ViewManager::activateView(ChartViewContainer* view)
{
    if (!view) return;

    QList<QMdiSubWindow*> windows = mdiArea->subWindowList();
    for (QMdiSubWindow* window : windows) {
        if (window->widget() == view) {
            mdiArea->setActiveSubWindow(window);
            break;
        }
    }
}

void ViewManager::activateNextView()
{
    mdiArea->activateNextSubWindow();
}

void ViewManager::activatePreviousView()
{
    mdiArea->activatePreviousSubWindow();
}

void ViewManager::onSubWindowActivated(QMdiSubWindow* window)
{
    ChartViewContainer* view = nullptr;
    if (window) {
        view = qobject_cast<ChartViewContainer*>(window->widget());
    }
    emit currentViewChanged(view);
}
