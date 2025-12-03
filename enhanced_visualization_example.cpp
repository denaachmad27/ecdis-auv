// Example of enhanced POI visualization using EC2007 kernel
// This would replace the current manual drawing in ecwidget.cpp

#include "ecwidget.h"
#include "eckernel.h"  // EC2007 kernel header

void EcWidget::drawEnhancedPOI(QPainter &painter, const PoiEntry &poi, const QPoint &screenPoint)
{
    // Convert QPainter context to EC2007 kernel format
    HDC hdc = painter.device()->nativeHandle(); // Convert to Windows HDC

    // Get the EC2007 view from current chart
    EcView *ecView = getCurrentEcView();
    if (!ecView) return;

    // Map screen coordinates to EC2007 coordinate system
    int ecX = screenPoint.x();
    int ecY = screenPoint.y();

    // Define symbol names based on POI category (S-52 compliant)
    const char* symbolName = getPOISymbolName(poi.category);
    int colorIndex = getPOIColorIndex(poi.category);
    double sizeFactor = getPOISizeFactor(poi.category);

    // Use kernel's advanced symbol drawing with anti-aliasing and proper scaling
    int symbolWidth, symbolHeight;
    Bool result = EcDrawNTDrawSymbolExt(
        ecView,           // EC2007 view context
        hdc,              // Device context
        nullptr,          // No pixmap needed for direct drawing
        symbolName,       // S-52 compliant symbol name
        ecX, ecY,         // Screen coordinates
        0.0,              // No rotation for POIs
        sizeFactor,       // Dynamic size based on zoom and importance
        colorIndex,       // Category-specific color from S-52 palette
        &symbolWidth,     // Output: actual symbol width
        &symbolHeight     // Output: actual symbol height
    );

    if (result) {
        // Draw enhanced label using kernel text rendering
        char labelText[256];
        strncpy(labelText, poi.label.toUtf8().constData(), sizeof(labelText) - 1);

        EcDrawNTDrawPointText(
            ecView,
            hdc,
            labelText,
            ecX + symbolWidth/2 + 5,  // Position label to the right of symbol
            ecY,
            EC_TEXT_ALIGN_LEFT | EC_TEXT_ALIGN_VCENTER,
            colorIndex
        );

        // Add glow effect for critical POIs (Man Overboard)
        if (poi.category == EC_POI_MAN_OVERBOARD) {
            drawCriticalGlow(ecView, hdc, ecX, ecY, symbolWidth, symbolHeight);
        }
    }
}

const char* EcWidget::getPOISymbolName(EcPoiCategory category)
{
    switch (category) {
        case EC_POI_MAN_OVERBOARD:
            return "person_overboard";      // S-52 person in water symbol
        case EC_POI_HAZARD:
            return "danger_symbol";         // S-52 hazard/danger symbol
        case EC_POI_CHECKPOINT:
            return "waypoint_triangle";     // S-52 waypoint symbol
        case EC_POI_SURVEY_TARGET:
            return "survey_marker";         // S-52 survey marker
        case EC_POI_GENERIC:
        default:
            return "landmark";              // S-52 generic landmark symbol
    }
}

int EcWidget::getPOIColorIndex(EcPoiCategory category)
{
    switch (category) {
        case EC_POI_MAN_OVERBOARD:
            return EcDrawGetColorIndex(m_view, "SCTRD13");  // Safety red
        case EC_POI_HAZARD:
            return EcDrawGetColorIndex(m_view, "SCTRD13");  // Hazard red
        case EC_POI_CHECKPOINT:
            return EcDrawGetColorIndex(m_view, "SCTBLU");   // Navigation blue
        case EC_POI_SURVEY_TARGET:
            return EcDrawGetColorIndex(m_view, "SCTORO");   // Survey orange
        case EC_POI_GENERIC:
        default:
            return EcDrawGetColorIndex(m_view, "SCTGRN");   // Generic green
    }
}

void EcWidget::drawCriticalGlow(EcView *view, HDC hdc, int x, int y, int width, int height)
{
    // Pulsing glow effect for Man Overboard using kernel overlay functions
    static double pulsePhase = 0.0;
    pulsePhase += 0.1; // Animation speed

    int glowRadius = (width + height) / 4 + sin(pulsePhase) * 5;
    int glowColor = EcDrawGetColorIndex(view, "SCTRD13"); // Safety red

    // Draw multiple concentric circles for glow effect
    for (int i = 3; i > 0; i--) {
        int alpha = 80 / i; // Fading effect
        EcDrawNTOverlayCircle(
            view, hdc, x, y,
            glowRadius * i,
            glowColor,
            2 - i/2  // Line width decreases with distance
        );
    }
}

// Enhanced AIS target visualization
void EcWidget::drawEnhancedAISTarget(QPainter &painter, const AISTarget &target, const QPoint &screenPoint)
{
    HDC hdc = painter.device()->nativeHandle();
    EcView *ecView = getCurrentEcView();

    // Draw vessel using S-52 vessel symbols
    const char* vesselSymbol = getVesselSymbolName(target.vesselType);
    int vesselColor = getVesselColorIndex(target.riskLevel);

    int vesselWidth, vesselHeight;
    EcDrawNTDrawSymbolExt(
        ecView, hdc, nullptr,
        vesselSymbol,
        screenPoint.x(), screenPoint.y(),
        target.heading,  // Show vessel heading
        1.2,             // Scale factor
        vesselColor,
        &vesselWidth, &vesselHeight
    );

    // Draw predicted path for high-risk targets
    if (target.riskLevel >= HIGH_RISK) {
        drawPredictedPath(ecView, hdc, target);
    }

    // Draw target information label
    char targetLabel[128];
    sprintf(targetLabel, "%s\n%.1f°\n%.2fm",
            target.vesselName.toUtf8().constData(),
            target.heading,
            target.cpaDistance);

    EcDrawNTDrawPointText(
        ecView, hdc, targetLabel,
        screenPoint.x() + vesselWidth/2 + 5,
        screenPoint.y(),
        EC_TEXT_ALIGN_LEFT | EC_TEXT_ALIGN_TOP,
        vesselColor
    );
}

// Advanced collision risk visualization
void EcWidget::drawCollisionRiskZone(EcView *view, HDC hdc, double latitude, double longitude,
                                   double cpaRadius, int riskLevel)
{
    // Convert coordinates
    int x, y;
    if (!LatLonToXy(latitude, longitude, x, y)) return;

    // Get risk-appropriate color
    int riskColor;
    const char* patternName;

    switch (riskLevel) {
        case CRITICAL_RISK:
            riskColor = EcDrawGetColorIndex(view, "SCTRD13");  // Safety red
            patternName = "danger_crosshatch";
            break;
        case HIGH_RISK:
            riskColor = EcDrawGetColorIndex(view, "SCTORA");   // Warning orange
            patternName = "warning_hatch";
            break;
        case MEDIUM_RISK:
            riskColor = EcDrawGetColorIndex(view, "SCTYEL");   // Caution yellow
            patternName = "caution_dots";
            break;
        default:
            return; // No visualization for low/no risk
    }

    // Draw filled danger zone with pattern
    EcDrawNTOverlayCircle(
        view, hdc, x, y,
        static_cast<int>(cpaRadius * pixelsPerNm()),
        riskColor,
        3
    );

    // Add crosshatch pattern for critical zones
    if (riskLevel == CRITICAL_RISK) {
        EcDrawNTOverlayPattern(
            view, hdc, x, y,
            static_cast<int>(cpaRadius * pixelsPerNm()),
            patternName,
            riskColor
        );
    }
}