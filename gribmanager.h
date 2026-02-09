#ifndef GRIBMANAGER_H
#define GRIBMANAGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include "gribdata.h"

/**
 * @brief Manager class for loading and managing GRIB files
 *
 * This class handles:
 * - Loading GRIB files (using eccodes library when available)
 * - Parsing wave parameters (swh, mpwd, mwp)
 * - Managing time steps for animation
 * - Providing access to loaded data
 */
class GribManager : public QObject
{
    Q_OBJECT

public:
    explicit GribManager(QObject *parent = nullptr);
    ~GribManager();

    /**
     * @brief Load a GRIB file from the given path
     * @param filePath Path to the GRIB file
     * @return true if successful, false otherwise
     */
    bool loadFromFile(const QString& filePath);

    /**
     * @brief Get the loaded GRIB data
     */
    GribData getData() const { return m_data; }

    /**
     * @brief Get the current time step index
     */
    int getCurrentTimeStep() const { return m_currentTimeStep; }

    /**
     * @brief Set the current time step index
     */
    void setCurrentTimeStep(int step);

    /**
     * @brief Get the total number of time steps
     */
    int getTimeStepCount() const { return m_data.getTimeStepCount(); }

    /**
     * @brief Get the current message/time step
     */
    GribMessage getCurrentMessage() const;

    /**
     * @brief Get list of time step labels
     */
    QStringList getTimeStepLabels() const { return m_data.getTimeStepLabels(); }

    /**
     * @brief Get file information as formatted string
     */
    QString getFileInfo() const;

    /**
     * @brief Check if data is loaded
     */
    bool isLoaded() const { return !m_data.isEmpty(); }

    /**
     * @brief Clear loaded data
     */
    void clear();

    /**
     * @brief Get wave data at specific coordinates for current time step
     * @param lat Latitude in degrees
     * @param lon Longitude in degrees
     * @return Wave data at the point (interpolated if needed)
     */
    GribWaveData getWaveDataAt(double lat, double lon) const;

    /**
     * @brief Get min/max wave height in current time step
     */
    void getWaveHeightRange(double& minH, double& maxH) const;

    // Visualization settings
    bool showHeatmap() const { return m_showHeatmap; }
    void setShowHeatmap(bool show) { m_showHeatmap = show; }

    bool showArrows() const { return m_showArrows; }
    void setShowArrows(bool show) { m_showArrows = show; }

    int arrowDensity() const { return m_arrowDensity; }
    void setArrowDensity(int density) { m_arrowDensity = density; }

signals:
    /**
     * @brief Emitted when file is loaded
     */
    void fileLoaded(const QString& fileName);

    /**
     * @brief Emitted when file loading fails
     */
    void loadFailed(const QString& error);

    /**
     * @brief Emitted when time step changes
     */
    void timeStepChanged(int step);

    /**
     * @brief Emitted when data is cleared
     */
    void dataCleared();

private:
    /**
     * @brief Parse GRIB file using eccodes library
     * Note: Requires eccodes to be installed and linked
     */
    bool parseWithEccodes(const QString& filePath);

    /**
     * @brief Create sample data for testing (when eccodes not available)
     */
    bool createSampleData(const QString& filePath);

    /**
     * @brief Extract parameter name from GRIB parameter code
     */
    QString getParameterName(int parameterId, int parameterCategory);

private:
    GribData m_data;
    int m_currentTimeStep;

    // Visualization settings
    bool m_showHeatmap;
    bool m_showArrows;
    int m_arrowDensity;  // Arrow grid density (1 = every point, 2 = every 2nd point, etc.)

    // Eccodes availability flag
    bool m_eccodesAvailable;
};

#endif // GRIBMANAGER_H
