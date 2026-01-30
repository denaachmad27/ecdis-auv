#ifndef _satellite_tile_layer_h_
#define _satellite_tile_layer_h_

#include <QObject>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QPoint>
#include <QMutex>
#include <QDir>
#include <QTimer>

class SatelliteTileLayer : public QObject
{
    Q_OBJECT

public:
    // Tile key structure (public for qHash access)
    struct TileKey {
        int x, y, z;

        TileKey(int x_ = 0, int y_ = 0, int z_ = 0) : x(x_), y(y_), z(z_) {}

        bool operator==(const TileKey &other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator<(const TileKey &other) const {
            if (z != other.z) return z < other.z;
            if (x != other.x) return x < other.x;
            return y < other.y;
        }

        friend uint qHash(const TileKey &key, uint seed);
    };

    explicit SatelliteTileLayer(QObject *parent = nullptr);
    virtual ~SatelliteTileLayer();

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setViewport(double minLat, double maxLat, double minLon, double maxLon, int zoomLevel);
    void setWidgetSize(int width, int height);

    QPixmap getTile(int x, int y, int z);
    QPixmap getTileWithFallback(int x, int y, int z);

    static QString getCacheDir();
    static QString getBundledTilesDir();
    static qint64 getCacheSize();
    static void clearCache();

    static const int MAX_ZOOM = 19;
    static const int MIN_ZOOM = 0;
    static const qint64 MAX_CACHE_SIZE = 500 * 1024 * 1024; // 500MB
    static const int MAX_BUNDLED_ZOOM = 16; // Max zoom for bundled tiles

    // Tile coordinate conversions
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);
    static double tileXToLon(int x, int zoom);
    static double tileYToLat(int y, int zoom);
    static int calculateZoomLevel(double scaleLat, double viewportHeight);

    // Web Mercator (EPSG:3857) conversion functions
    static void webMercatorToGeographic(double x, double y, double &lat, double &lon);
    static void geographicToWebMercator(double lat, double lon, double &x, double &y);
    static double webMercatorLatToY(double lat);
    static double yToWebMercatorLat(double y);
    static double tileXToWebMercatorX(int x, int zoom);
    static double tileYToWebMercatorY(int y, int zoom);
    static double webMercatorXToTileX(double x, int zoom);
    static double webMercatorYToTileY(double y, int zoom);

signals:
    void tileUpdated(int x, int y, int z);
    void allTilesLoaded();

private slots:
    void onTileDownloaded(QNetworkReply *reply);
    void onCacheCleanup();

private:
    QString getTileUrl(int x, int y, int z) const;
    QString getTileCachePath(int x, int y, int z) const;
    QString getBundledTilePath(int x, int y, int z) const;
    bool loadTileFromCache(int x, int y, int z, QPixmap &pixmap);
    bool loadBundledTile(int x, int y, int z, QPixmap &pixmap);
    bool saveTileToCache(int x, int y, int z, const QPixmap &pixmap);
    void fetchTile(int x, int y, int z);
    void updateNeededTiles();
    void cleanupCache();

    bool m_enabled;
    QNetworkAccessManager *m_networkManager;

    QMap<TileKey, QPixmap> m_tileCache;
    QMap<TileKey, QNetworkReply*> m_pendingRequests;

    double m_minLat, m_maxLat, m_minLon, m_maxLon;
    int m_zoomLevel;
    int m_widgetWidth, m_widgetHeight;

    QSet<TileKey> m_neededTiles;
    QMutex m_cacheMutex;

    QTimer *m_cleanupTimer;
};

#endif // _satellite_tile_layer_h_
