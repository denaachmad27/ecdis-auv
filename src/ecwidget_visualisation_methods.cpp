// Additional methods for EcWidget to support visualization
// This file should be integrated into ecwidget.cpp

// In the EcWidget class, add these methods:

void EcWidget::setShowCurrentArrows(bool show)
{
    m_showCurrentArrows = show;
    update();  // Trigger repaint
}

void EcWidget::setShowTideRectangles(bool show)
{
    m_showTideRectangles = show;
    update();  // Trigger repaint
}

void EcWidget::refreshVisualization()
{
    // Refresh visualization data
    if (m_visualisationPanel) {
        // Update visualization data from panel
        if (m_currentVisualisation) {
            m_currentVisualisation->updateCurrentData(m_visualisationPanel->getCurrentStations());
            m_currentVisualisation->updateTideData(m_visualisationPanel->getTideVisualizations());
        }
    }

    update();  // Trigger repaint
}

// In the EcWidget constructor, add initialization:
// m_currentVisualisation = new CurrentVisualisation(this);
// m_showCurrentArrows = true;
// m_showTideRectangles = true;

// In the EcWidget destructor, add cleanup:
// delete m_currentVisualisation;

// In the EcWidget::drawWorks() method, add this after drawAISCell(p):
/*
// Draw current visualizations
if (m_currentVisualisation) {
    if (m_showCurrentArrows) {
        // Get current stations data and draw arrows
        QList<CurrentStation> currentStations;
        if (m_visualisationPanel) {
            currentStations = m_visualisationPanel->getCurrentStations();
        } else {
            currentStations = m_currentVisualisation->generateSampleCurrentData();
        }
        m_currentVisualisation->drawCurrentArrows(p, currentStations);
    }

    if (m_showTideRectangles) {
        // Get tide visualization data and draw rectangles
        QList<TideVisualization> tideVisualizations;
        if (m_visualisationPanel) {
            tideVisualizations = m_visualisationPanel->getTideVisualizations();
        } else {
            tideVisualizations = m_currentVisualisation->generateSampleTideData();
        }
        m_currentVisualisation->drawTideRectangles(p, tideVisualizations);
    }
}
*/