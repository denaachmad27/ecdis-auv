#include "satellitetilelayer.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QDebug>
#include <QApplication>
#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ESRI World Imagery Tile Server (HTTPS - requires SSL support)
// static const QString ESRI_TILE_URL = "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%1/%2/%3";

// Try HTTP (ESRI might not support anymore, but worth trying)
// static const QString ESRI_TILE_URL = "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%1/%2/%3";

// OSM Standard tiles (HTTP, for testing - not satellite but good for verification)
// static const QString ESRI_TILE_URL = "http://tile.openstreetmap.org/%1/%2/%3.png";

// Test: Generate solid color tiles locally for testing rendering
static QString TEST_TILE_URL = "";  // Empty = generate locally

// Static member definitions
const int SatelliteTileLayer::MAX_ZOOM;
const int SatelliteTileLayer::MIN_ZOOM;
const qint64 SatelliteTileLayer::MAX_CACHE_SIZE;

// Hash function for TileKey (needed for QSet)
uint qHash(const SatelliteTileLayer::TileKey &key, uint seed = 0)
{
    return qHash(key.x, seed) ^ qHash(key.y, seed) ^ qHash(key.z, seed);
}

SatelliteTileLayer::SatelliteTileLayer(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_minLat(-90), m_maxLat(90), m_minLon(-180), m_maxLon(180)
    , m_zoomLevel(2)
    , m_widgetWidth(800), m_widgetHeight(600)
{
    // Create cache directory
    QDir cacheDir(getCacheDir());
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    // Setup cleanup timer
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &SatelliteTileLayer::onCacheCleanup);
    m_cleanupTimer->start(300000); // Check every 5 minutes

    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &SatelliteTileLayer::onTileDownloaded);
}

SatelliteTileLayer::~SatelliteTileLayer()
{
    // Cancel pending requests
    for (auto reply : m_pendingRequests) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_pendingRequests.clear();
}

void SatelliteTileLayer::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) {
            // Cancel all pending requests when disabled
            for (auto reply : m_pendingRequests) {
                if (reply) {
                    reply->abort();
                    reply->deleteLater();
                }
            }
            m_pendingRequests.clear();
            m_neededTiles.clear();
        }
    }
}

void SatelliteTileLayer::setViewport(double minLat, double maxLat, double minLon, double maxLon, int zoomLevel)
{
    m_minLat = qBound(-90.0, minLat, 90.0);
    m_maxLat = qBound(-90.0, maxLat, 90.0);
    m_minLon = minLon;
    m_maxLon = maxLon;
    m_zoomLevel = qBound(MIN_ZOOM, zoomLevel, MAX_ZOOM);

    updateNeededTiles();
}

void SatelliteTileLayer::setWidgetSize(int width, int height)
{
    m_widgetWidth = qMax(100, width);
    m_widgetHeight = qMax(100, height);
}

QPixmap SatelliteTileLayer::getTile(int x, int y, int z)
{
    TileKey key(x, y, z);
    QMutexLocker locker(&m_cacheMutex);

    if (m_tileCache.contains(key)) {
        return m_tileCache[key];
    }

    // Return empty pixmap if not found
    return QPixmap();
}

QPixmap SatelliteTileLayer::getTileWithFallback(int x, int y, int z)
{
    // Try exact zoom first
    QPixmap tile = getTile(x, y, z);
    if (!tile.isNull()) {
        return tile;
    }

    // Fallback: use tile from lower zoom level and scale it up
    // This prevents blank tiles during zoom operations
    for (int fallbackZ = qMax(MIN_ZOOM, z - 1); fallbackZ >= MIN_ZOOM; fallbackZ--) {
        int fallbackX = x >> (z - fallbackZ);
        int fallbackY = y >> (z - fallbackZ);

        TileKey fallbackKey(fallbackX, fallbackY, fallbackZ);

        QMutexLocker locker(&m_cacheMutex);
        if (m_tileCache.contains(fallbackKey)) {
            QPixmap fallbackTile = m_tileCache[fallbackKey];
            // Scale tile to match current zoom level
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

int SatelliteTileLayer::lonToTileX(double lon, int zoom)
{
    return (int)(floor((lon + 180.0) / 360.0 * (1 << zoom)));
}

int SatelliteTileLayer::latToTileY(double lat, int zoom)
{
    double latRad = lat * M_PI / 180.0;
    return (int)(floor((1.0 - asinh(tan(latRad)) / M_PI) / 2.0 * (1 << zoom)));
}

double SatelliteTileLayer::tileXToLon(int x, int zoom)
{
    return x / (double)(1 << zoom) * 360.0 - 180.0;
}

double SatelliteTileLayer::tileYToLat(int y, int zoom)
{
    double n = M_PI - 2.0 * M_PI * y / (double)(1 << zoom);
    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

int SatelliteTileLayer::calculateZoomLevel(double scaleLat, double viewportHeight)
{
    // Approximate zoom level from scale
    // This is a simplified calculation; you may need to adjust based on your coordinate system
    const double earthCircumference = 40075000; // meters
    double metersPerPixel = earthCircumference * cos(scaleLat * M_PI / 180.0) / (256 * (1 << 10));

    if (viewportHeight > 0) {
        metersPerPixel = earthCircumference / (256 * viewportHeight * 2);
    }

    int zoom = (int)floor(log2(earthCircumference / (256 * metersPerPixel)));
    return qBound(MIN_ZOOM, zoom, MAX_ZOOM);
}

// Web Mercator (EPSG:3857) Constants
static const double WEB_MERCATOR_RADIUS = 6378137.0;
static const double WEB_MERCATOR_EXTENT = 20037508.34; // Half earth circumference in meters

void SatelliteTileLayer::webMercatorToGeographic(double x, double y, double &lat, double &lon)
{
    // Web Mercator: x = lon * radius, y = ln(tan(pi/4 + lat/2)) * radius
    lon = x / WEB_MERCATOR_EXTENT * 180.0;
    double yRad = y / WEB_MERCATOR_EXTENT * M_PI;
    lat = (2.0 * atan(exp(yRad)) - M_PI/2.0) * 180.0 / M_PI;
}

void SatelliteTileLayer::geographicToWebMercator(double lat, double lon, double &x, double &y)
{
    x = lon * WEB_MERCATOR_EXTENT / 180.0;
    double latRad = lat * M_PI / 180.0;
    y = log(tan(M_PI/4.0 + latRad/2.0)) / M_PI * WEB_MERCATOR_EXTENT;
}

double SatelliteTileLayer::webMercatorLatToY(double lat)
{
    double latRad = lat * M_PI / 180.0;
    return log(tan(M_PI/4.0 + latRad/2.0)) / M_PI * WEB_MERCATOR_EXTENT;
}

double SatelliteTileLayer::yToWebMercatorLat(double y)
{
    double yRad = y / WEB_MERCATOR_EXTENT * M_PI;
    return (2.0 * atan(exp(yRad)) - M_PI/2.0) * 180.0 / M_PI;
}

double SatelliteTileLayer::tileXToWebMercatorX(int x, int zoom)
{
    // Each tile at zoom level z covers WEB_MERCATOR_EXTENT * 2 / 2^z meters
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return -WEB_MERCATOR_EXTENT + x * tileSize;
}

double SatelliteTileLayer::tileYToWebMercatorY(int y, int zoom)
{
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return WEB_MERCATOR_EXTENT - (y + 1) * tileSize;  // Y is inverted in tile coords
}

double SatelliteTileLayer::webMercatorXToTileX(double x, int zoom)
{
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return (x + WEB_MERCATOR_EXTENT) / tileSize;
}

double SatelliteTileLayer::webMercatorYToTileY(double y, int zoom)
{
    double tileSize = (WEB_MERCATOR_EXTENT * 2.0) / (1 << zoom);
    return (WEB_MERCATOR_EXTENT - y) / tileSize - 1.0;
}

QString SatelliteTileLayer::getCacheDir()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return baseDir + "/satellite-tiles";
}

qint64 SatelliteTileLayer::getCacheSize()
{
    qint64 totalSize = 0;
    QDirIterator it(getCacheDir(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        totalSize += info.size();
    }
    return totalSize;
}

void SatelliteTileLayer::clearCache()
{
    // Clear disk cache (static function - cannot access instance members)
    QDir cacheDir(getCacheDir());
    if (cacheDir.exists()) {
        cacheDir.removeRecursively();
        cacheDir.mkpath(".");
    }
}

QString SatelliteTileLayer::getBundledTilesDir()
{
    // Try exe directory first
    QString appDir = QCoreApplication::applicationDirPath();
    QString tilesPath = appDir + "/tiles";

    // For debug builds, also check parent directory
    if (!QDir(tilesPath).exists()) {
        tilesPath = appDir + "/../tiles";
        // Convert to absolute path
        tilesPath = QDir(tilesPath).absolutePath();
    }

    return tilesPath;
}

QString SatelliteTileLayer::getTileUrl(int x, int y, int z) const
{
    // For test mode, return empty string
    if (TEST_TILE_URL.isEmpty()) {
        return QString();
    }
    // ESRI World Imagery uses {z}/{y}/{x} format
    return TEST_TILE_URL.arg(z).arg(y).arg(x);
}

QString SatelliteTileLayer::getTileCachePath(int x, int y, int z) const
{
    QString dir = QString("%1/%2").arg(getCacheDir(), z);
    QDir().mkpath(dir);
    return QString("%1/%2_%3.png").arg(dir, x).arg(y);
}

bool SatelliteTileLayer::loadTileFromCache(int x, int y, int z, QPixmap &pixmap)
{
    QString cachePath = getTileCachePath(x, y, z);
    QFileInfo info(cachePath);

    if (info.exists()) {
        if (pixmap.load(cachePath)) {
            return true;
        } else {
            // Corrupted file, remove it
            QFile::remove(cachePath);
        }
    }
    return false;
}

bool SatelliteTileLayer::saveTileToCache(int x, int y, int z, const QPixmap &pixmap)
{
    QString cachePath = getTileCachePath(x, y, z);
    return pixmap.save(cachePath, "PNG");
}

QString SatelliteTileLayer::getBundledTilePath(int x, int y, int z) const
{
    // Bundled tiles use structure: tiles/z/x_y.png
    QString dir = getBundledTilesDir() + "/" + QString::number(z);
    return dir + "/" + QString::number(x) + "_" + QString::number(y) + ".png";
}

bool SatelliteTileLayer::loadBundledTile(int x, int y, int z, QPixmap &pixmap)
{
    static bool firstCall = true;
    if (firstCall) {
        qDebug() << "[SATELLITE] Bundled tiles dir:" << getBundledTilesDir();
        firstCall = false;
    }

    QString bundledPath = getBundledTilePath(x, y, z);
    QFileInfo info(bundledPath);

    if (info.exists()) {
        if (pixmap.load(bundledPath)) {
            return true;
        }
    }
    return false;
}

void SatelliteTileLayer::fetchTile(int x, int y, int z)
{
    TileKey key(x, y, z);

    if (m_pendingRequests.contains(key)) {
        return; // Already fetching
    }

    QPixmap pixmap;

    // 1. Try memory cache first
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_tileCache.contains(key)) {
            qDebug() << "[SATELLITE] Tile from memory cache:" << x << y << z;
            emit tileUpdated(x, y, z);
            return;
        }
    }

    // 2. Try bundled tiles (shipped with app)
    if (z <= MAX_BUNDLED_ZOOM && loadBundledTile(x, y, z, pixmap)) {
        QMutexLocker locker(&m_cacheMutex);
        m_tileCache[key] = pixmap;
        emit tileUpdated(x, y, z);
        return;
    }

    // 3. Try disk cache
    if (loadTileFromCache(x, y, z, pixmap)) {
        qDebug() << "[SATELLITE] Tile from disk cache:" << x << y << z;
        QMutexLocker locker(&m_cacheMutex);
        m_tileCache[key] = pixmap;
        emit tileUpdated(x, y, z);
        return;
    }

    // 4. Test mode: Generate tile locally if TEST_TILE_URL is empty
    if (TEST_TILE_URL.isEmpty()) {
        // Generate a test tile with pattern
        QPixmap testTile(256, 256);
        testTile.fill(QColor(60, 120, 60)); // Brighter green (satellite-ish)

        QPainter painter(&testTile);
        painter.setPen(QColor(255, 255, 255));
        painter.drawRect(0, 0, 255, 255);
        painter.fillRect(0, 0, 256, 256, QColor(60, 120, 60, 200)); // Semi-transparent green
        painter.setPen(QColor(255, 255, 0));
        painter.drawText(10, 30, QString("TILE %1,%2 z%3").arg(x).arg(y).arg(z));
        painter.setPen(QColor(255, 200, 200));
        painter.drawLine(0, 0, 256, 256);
        painter.drawLine(256, 0, 0, 256);
        painter.drawRect(10, 10, 236, 236);

        saveTileToCache(x, y, z, testTile);

        QMutexLocker locker(&m_cacheMutex);
        m_tileCache[key] = testTile;
        emit tileUpdated(x, y, z);
        qDebug() << "[SATELLITE] Generated test tile:" << x << y << z;
        return;
    }

    // Fetch from network
    QString url = getTileUrl(x, y, z);
    qDebug() << "[SATELLITE] Fetching from network:" << url;

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ECDIS-AUV/1.0");
    request.setRawHeader("Accept", "image/png");

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("tileX", x);
    reply->setProperty("tileY", y);
    reply->setProperty("tileZ", z);

    m_pendingRequests[key] = reply;
}

void SatelliteTileLayer::updateNeededTiles()
{
    if (!m_enabled) {
        qDebug() << "[SATELLITE] updateNeededTiles: NOT enabled, skipping";
        return;
    }

    m_neededTiles.clear();

    // Add buffer zone - load extra tiles around viewport for smoother panning
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
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            TileKey key(x, y, m_zoomLevel);
            m_neededTiles.insert(key);
            fetchTile(x, y, m_zoomLevel);
            tileCount++;
        }
    }

    qDebug() << "[SATELLITE] updateNeededTiles: zoom=" << m_zoomLevel
             << "bounds: X[" << startX << "-" << endX << "] Y[" << startY << "-" << endY << "]"
             << "total tiles:" << tileCount;
}

void SatelliteTileLayer::cleanupCache()
{
    qint64 cacheSize = getCacheSize();

    if (cacheSize > MAX_CACHE_SIZE) {
        // Remove oldest files
        QMap<QDateTime, QString> files;
        QDirIterator it(getCacheDir(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString path = it.next();
            QFileInfo info(path);
            files[info.lastModified()] = path;
        }

        qint64 toRemove = cacheSize - (MAX_CACHE_SIZE * 9 / 10); // Remove to 90% of max
        qint64 removed = 0;

        for (auto it = files.begin(); it != files.end() && removed < toRemove; ++it) {
            QFileInfo info(it.value());
            removed += info.size();
            QFile::remove(it.value());
        }

        // Clear in-memory cache of removed tiles
        QMutexLocker locker(&m_cacheMutex);
        QMutableMapIterator<TileKey, QPixmap> i(m_tileCache);
        while (i.hasNext()) {
            i.next();
            QString cachePath = getTileCachePath(i.key().x, i.key().y, i.key().z);
            if (!QFileInfo::exists(cachePath)) {
                i.remove();
            }
        }
    }
}

void SatelliteTileLayer::onTileDownloaded(QNetworkReply *reply)
{
    if (!reply) return;

    int x = reply->property("tileX").toInt();
    int y = reply->property("tileY").toInt();
    int z = reply->property("tileZ").toInt();
    TileKey key(x, y, z);

    m_pendingRequests.remove(key);

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        qDebug() << "[SATELLITE] Downloaded tile:" << x << y << z << "size:" << data.size() << "bytes";

        QPixmap pixmap;

        if (pixmap.loadFromData(data)) {
            saveTileToCache(x, y, z, pixmap);

            QMutexLocker locker(&m_cacheMutex);
            m_tileCache[key] = pixmap;
            emit tileUpdated(x, y, z);
            qDebug() << "[SATELLITE] Tile loaded and cached:" << x << y << z;
        } else {
            qWarning() << "[SATELLITE] Failed to load pixmap from data for tile:" << x << y << z;
        }
    } else {
        qWarning() << "[SATELLITE] Failed to download tile:" << x << y << z
                   << "error:" << reply->errorString() << reply->error();
    }

    reply->deleteLater();

    // Check if all tiles are loaded
    bool allLoaded = true;
    for (const auto &key : m_neededTiles) {
        if (!m_tileCache.contains(key) && m_pendingRequests.contains(key)) {
            allLoaded = false;
            break;
        }
    }
    if (allLoaded) {
        qDebug() << "[SATELLITE] All tiles loaded!";
        emit allTilesLoaded();
    }
}

void SatelliteTileLayer::onCacheCleanup()
{
    cleanupCache();
}
