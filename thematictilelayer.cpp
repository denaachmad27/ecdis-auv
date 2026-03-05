#include "thematictilelayer.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDirIterator>
#include <QDebug>
#include <QApplication>
#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Web Mercator (EPSG:3857) Constants
static const double WEB_MERCATOR_RADIUS = 6378137.0;
static const double WEB_MERCATOR_EXTENT = 20037508.34;

// Hash function for TileKey
uint qHash(const ThematicTileLayer::TileKey &key, uint seed = 0)
{
    return qHash(key.x, seed) ^ qHash(key.y, seed) ^ qHash(key.z, seed) ^ qHash(key.layerName, seed);
}

ThematicTileLayer::ThematicTileLayer(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_minLat(-90), m_maxLat(90), m_minLon(-180), m_maxLon(180)
    , m_zoomLevel(10)
    , m_widgetWidth(800), m_widgetHeight(600)
{
    // Scan available layers on startup
    scanAvailableLayers();

    qDebug() << "[THEMATIC] ThematicTileLayer initialized";
    qDebug() << "[THEMATIC] Tiles directory:" << getTilesDir();
    qDebug() << "[THEMATIC] Available layers:" << m_availableLayers;
}

ThematicTileLayer::~ThematicTileLayer()
{
    // Cleanup handled by QObject
}

void ThematicTileLayer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) {
            m_neededTiles.clear();
        }
        qDebug() << "[THEMATIC] Layer" << (enabled ? "enabled" : "disabled");
    }
}

void ThematicTileLayer::setViewport(double minLat, double maxLat, double minLon, double maxLon, int zoomLevel)
{
    m_minLat = qBound(-90.0, minLat, 90.0);
    m_maxLat = qBound(-90.0, maxLat, 90.0);
    m_minLon = minLon;
    m_maxLon = maxLon;
    m_zoomLevel = qBound(0, zoomLevel, 19);

    updateNeededTiles();
}

void ThematicTileLayer::setWidgetSize(int width, int height)
{
    m_widgetWidth = qMax(100, width);
    m_widgetHeight = qMax(100, height);
}

void ThematicTileLayer::setActiveLayers(const QStringList &layers)
{
    m_activeLayers = layers;
    qDebug() << "[THEMATIC] Active layers:" << m_activeLayers;
    updateNeededTiles();
}

void ThematicTileLayer::addLayer(const QString &layerName)
{
    if (!m_activeLayers.contains(layerName)) {
        m_activeLayers.append(layerName);
        qDebug() << "[THEMATIC] Added layer:" << layerName;
        updateNeededTiles();
    }
}

void ThematicTileLayer::removeLayer(const QString &layerName)
{
    m_activeLayers.removeAll(layerName);
    qDebug() << "[THEMATIC] Removed layer:" << layerName;
    updateNeededTiles();
}

void ThematicTileLayer::clearLayers()
{
    m_activeLayers.clear();
    qDebug() << "[THEMATIC] Cleared all layers";
    updateNeededTiles();
}

QStringList ThematicTileLayer::getAvailableLayers() const
{
    return m_availableLayers;
}

QPixmap ThematicTileLayer::getTile(int x, int y, int z, const QString &layerName)
{
    TileKey key(x, y, z, layerName);
    QMutexLocker locker(&m_cacheMutex);

    if (m_tileCache.contains(key)) {
        return m_tileCache[key];
    }

    return QPixmap();
}

QPixmap ThematicTileLayer::getTileWithFallback(int x, int y, int z, const QString &layerName)
{
    // Try exact zoom first
    QPixmap tile = getTile(x, y, z, layerName);
    if (!tile.isNull()) {
        return tile;
    }

    // Fallback: use tile from lower zoom level and scale it up
    for (int fallbackZ = qMax(0, z - 1); fallbackZ >= 0; fallbackZ--) {
        int fallbackX = x >> (z - fallbackZ);
        int fallbackY = y >> (z - fallbackZ);

        TileKey fallbackKey(fallbackX, fallbackY, fallbackZ, layerName);

        QMutexLocker locker(&m_cacheMutex);
        if (m_tileCache.contains(fallbackKey)) {
            QPixmap fallbackTile = m_tileCache[fallbackKey];
            int scale = 1 << (z - fallbackZ);
            QPixmap scaled = fallbackTile.scaled(
                fallbackTile.width() * scale,
                fallbackTile.height() * scale,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            return scaled;
        }
    }

    return QPixmap();
}

QString ThematicTileLayer::getTilesDir()
{
    // Check for bundled tiles directory first
    QString appDir = QCoreApplication::applicationDirPath();
    QString tilesPath = appDir + "/thematic-tiles";

    // For debug builds, also check parent directory
    if (!QDir(tilesPath).exists()) {
        tilesPath = appDir + "/../thematic-tiles";
        tilesPath = QDir(tilesPath).absolutePath();
    }

    return tilesPath;
}

qint64 ThematicTileLayer::getCacheSize()
{
    qint64 totalSize = 0;
    QDirIterator it(getTilesDir(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        totalSize += info.size();
    }
    return totalSize;
}

int ThematicTileLayer::lonToTileX(double lon, int zoom)
{
    return (int)(floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

int ThematicTileLayer::latToTileY(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    return (int)(floor((1.0 - asinh(tan(latRad)) / M_PI) / 2.0 * (1 << zoom)));
}

double ThematicTileLayer::tileXToLon(int x, int zoom)
{
    return x / (double)(1 << zoom) * 360.0 - 180.0;
}

double ThematicTileLayer::tileYToLat(int y, int zoom)
{
    double n = M_PI - 2.0 * M_PI * y / (double)(1 << zoom);
    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

int ThematicTileLayer::calculateZoomLevel(double scaleLat, double viewportHeight)
{
    const double earthCircumference = 40075000;
    double metersPerPixel = earthCircumference * cos(scaleLat * M_PI / 180.0) / (256 * (1 << 10));

    if (viewportHeight > 0) {
        metersPerPixel = earthCircumference / (256 * viewportHeight * 2);
    }

    int zoom = (int)floor(log2(earthCircumference / (256 * metersPerPixel)));
    return qBound(0, zoom, 19);
}

void ThematicTileLayer::webMercatorToGeographic(double x, double y, double &lat, double &lon)
{
    lon = x / WEB_MERCATOR_EXTENT * 180.0;
    double yRad = y / WEB_MERCATOR_EXTENT * M_PI;
    lat = (2.0 * atan(exp(yRad)) - M_PI/2.0) * 180.0 / M_PI;
}

void ThematicTileLayer::geographicToWebMercator(double lat, double lon, double &x, double &y)
{
    x = lon * WEB_MERCATOR_EXTENT / 180.0;
    double latRad = lat * M_PI / 180.0;
    y = log(tan(M_PI/4.0 + latRad/2.0)) / M_PI * WEB_MERCATOR_EXTENT;
}

double ThematicTileLayer::tileXToWebMercatorX(int x, int zoom)
{
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return -WEB_MERCATOR_EXTENT + x * tileSize;
}

double ThematicTileLayer::tileYToWebMercatorY(int y, int zoom)
{
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return WEB_MERCATOR_EXTENT - (y + 1) * tileSize;
}

QString ThematicTileLayer::getTilePath(int x, int y, int z, const QString &layerName) const
{
    // Structure: thematic-tiles/{layer}/{z}/{x}_{y}.png
    QString dir = QString("%1/%2/%3").arg(getTilesDir(), layerName).arg(z);
    return dir + "/" + QString::number(x) + "_" + QString::number(y) + ".png";
}

bool ThematicTileLayer::loadTileFromDisk(int x, int y, int z, const QString &layerName, QPixmap &pixmap)
{
    QString tilePath = getTilePath(x, y, z, layerName);
    QFileInfo info(tilePath);

    if (info.exists()) {
        if (pixmap.load(tilePath)) {
            return true;
        }
    }
    return false;
}

void ThematicTileLayer::scanAvailableLayers()
{
    m_availableLayers.clear();

    QString tilesDir = getTilesDir();
    QDir dir(tilesDir);

    if (!dir.exists()) {
        qDebug() << "[THEMATIC] Tiles directory does not exist:" << tilesDir;
        return;
    }

    // Each subdirectory is a layer name
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subdir : subdirs) {
        // Check if this layer has at least one zoom directory
        QDir layerDir(dir.absoluteFilePath(subdir));
        QStringList zoomDirs = layerDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (!zoomDirs.isEmpty()) {
            m_availableLayers.append(subdir);
        }
    }

    m_availableLayers.sort();
    qDebug() << "[THEMATIC] Found" << m_availableLayers.size() << "layers:" << m_availableLayers;
}

void ThematicTileLayer::updateNeededTiles()
{
    if (!m_enabled || m_activeLayers.isEmpty()) {
        return;
    }

    m_neededTiles.clear();

    // Add buffer zone - load extra tiles around viewport
    const int TILE_BUFFER = 1;

    int startX = lonToTileX(m_minLon, m_zoomLevel) - TILE_BUFFER;
    int endX = lonToTileX(m_maxLon, m_zoomLevel) + TILE_BUFFER;
    int startY = latToTileY(m_maxLat, m_zoomLevel) - TILE_BUFFER;
    int endY = latToTileY(m_minLat, m_zoomLevel) + TILE_BUFFER;

    // Clamp values
    int maxTile = 1 << m_zoomLevel;
    startX = qMax(0, startX);
    endX = qMin(maxTile - 1, endX);
    startY = qMax(0, startY);
    endY = qMin(maxTile - 1, endY);

    int tileCount = 0;

    // Load tiles for all active layers
    for (const QString &layerName : m_activeLayers) {
        for (int x = startX; x <= endX; x++) {
            for (int y = startY; y <= endY; y++) {
                TileKey key(x, y, m_zoomLevel, layerName);

                // Check if already in cache
                {
                    QMutexLocker locker(&m_cacheMutex);
                    if (m_tileCache.contains(key)) {
                        continue;
                    }
                }

                // Try to load from disk
                QPixmap pixmap;
                if (loadTileFromDisk(x, y, m_zoomLevel, layerName, pixmap)) {
                    QMutexLocker locker(&m_cacheMutex);
                    m_tileCache[key] = pixmap;
                    emit tileUpdated(x, y, m_zoomLevel, layerName);
                    tileCount++;
                }
            }
        }
    }

    if (tileCount > 0) {
        qDebug() << "[THEMATIC] Loaded" << tileCount << "tiles for zoom" << m_zoomLevel;
    }

    // Check cache size limit and evict old tiles if needed
    const qint64 MAX_CACHE_SIZE = 100 * 1024 * 1024; // 100MB
    qint64 currentSize = 0;
    {
        QMutexLocker locker(&m_cacheMutex);
        for (const auto &pixmap : m_tileCache) {
            currentSize += pixmap.width() * pixmap.height() * 4; // RGBA
        }
    }

    if (currentSize > MAX_CACHE_SIZE) {
        // Simple eviction: remove tiles not in current viewport
        QMutexLocker locker(&m_cacheMutex);
        QMutableMapIterator<TileKey, QPixmap> i(m_tileCache);
        while (i.hasNext()) {
            i.next();
            const TileKey &key = i.key();
            // Keep if in current zoom and approximate tile range
            if (key.z != m_zoomLevel ||
                key.x < startX || key.x > endX ||
                key.y < startY || key.y > endY) {
                i.remove();
            }
        }
        qDebug() << "[THEMATIC] Cache evicted, new size:" << m_tileCache.size();
    }
}
