#ifndef _chart_view_container_h_
#define _chart_view_container_h_

#include <QWidget>
#include <QVBoxLayout>
#include <QString>

// Forward declarations
class EcWidget;

// Include ecwidget.h which has proper Windows/eckernel includes
// We need this to get the EcDictInfo and EcDENC types properly defined
// Since we only use these types as pointers, there's no circular dependency issue
#include "ecwidget.h"

/**
 * ChartViewContainer - Wrapper widget for EcWidget with multi-view support
 *
 * This container encapsulates an EcWidget and manages its view type
 * (S-63 Chart, Satellite, Thematic, etc.)
 */
class ChartViewContainer : public QWidget
{
    Q_OBJECT

public:
    enum ViewType {
        VIEW_S63,       // Standard S-63 chart
        VIEW_SATELLITE, // Satellite overlay enabled
        VIEW_THEMATIC,  // Thematic layers enabled
        VIEW_CUSTOM     // Future extensions
    };

    explicit ChartViewContainer(EcDictInfo* dictInfo, QString* libStr, QWidget* parent = nullptr);
    virtual ~ChartViewContainer();

    // Get the embedded EcWidget
    EcWidget* getEcWidget() const { return ecWidget; }

    // View type management
    void setViewType(ViewType type);
    ViewType getViewType() const { return viewType; }

    // Title management
    void setTitle(const QString& title) { m_title = title; }
    QString getTitle() const { return m_title; }

    // Get display name for view type
    static QString getViewTypeName(ViewType type);

    // Configure view based on type (enables/disables layers)
    void configureView();

    // Attach shared DENC (called by ViewManager)
    void attachSharedDENC(EcDENC* denc);

signals:
    void viewTypeChanged(ViewType newType);
    void titleChanged(const QString& newTitle);

private:
    EcWidget* ecWidget;
    ViewType viewType;
    QString m_title;
    EcDictInfo* dictInfo;
    QString* libStr;

    void setupUI();
};

#endif // _chart_view_container_h_
