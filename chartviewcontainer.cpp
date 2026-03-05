#include "chartviewcontainer.h"  // This already includes ecwidget.h with proper headers

ChartViewContainer::ChartViewContainer(EcDictInfo* dictInfo, QString* libStr, QWidget* parent)
    : QWidget(parent)
    , ecWidget(nullptr)
    , viewType(VIEW_S63)
    , m_title("Chart")
    , dictInfo(dictInfo)
    , libStr(libStr)
{
    setupUI();
}

ChartViewContainer::~ChartViewContainer()
{
    // EcWidget is owned by this container and will be deleted by Qt's parent system
}

void ChartViewContainer::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create the EcWidget
    ecWidget = new EcWidget(dictInfo, libStr, this);
    layout->addWidget(ecWidget);

    // Set initial title
    m_title = getViewTypeName(viewType);
}

void ChartViewContainer::setViewType(ViewType type)
{
    if (viewType != type) {
        viewType = type;
        m_title = getViewTypeName(type);
        configureView();
        emit viewTypeChanged(type);
        emit titleChanged(m_title);
    }
}

void ChartViewContainer::configureView()
{
    if (!ecWidget || !ecWidget->IsInitialized()) {
        return;
    }

    // Configure layers based on view type
    switch (viewType) {
        case VIEW_S63:
            // Standard S-63 chart - disable satellite/thematic overlays
            ecWidget->ShowSatelliteLayer(false);
            ecWidget->ShowThematicLayer(false);
            break;

        case VIEW_SATELLITE:
            // Enable satellite overlay
            ecWidget->ShowSatelliteLayer(true);
            ecWidget->ShowThematicLayer(false);
            break;

        case VIEW_THEMATIC:
            // Enable thematic layers
            ecWidget->ShowSatelliteLayer(false);
            ecWidget->ShowThematicLayer(true);
            break;

        case VIEW_CUSTOM:
            // Custom configuration - can be extended
            break;
    }

    // Trigger redraw to apply changes
    ecWidget->update();
}

void ChartViewContainer::attachSharedDENC(EcDENC* denc)
{
    if (ecWidget) {
        // Set the shared DENC to the EcWidget
        ecWidget->SetDENC(denc);
    }
}

QString ChartViewContainer::getViewTypeName(ViewType type)
{
    switch (type) {
        case VIEW_S63:
            return "S-63 Chart";
        case VIEW_SATELLITE:
            return "Satellite View";
        case VIEW_THEMATIC:
            return "Thematic View";
        case VIEW_CUSTOM:
            return "Custom View";
        default:
            return "Unknown View";
    }
}
