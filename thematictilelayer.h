#ifndef _thematic_tile_layer_h_
#define _thematic_tile_layer_h_

#include <QObject>
#include <QPixmap>
#include <QMap>
#include <QPoint>
#include <QMutex>
#include <QDir>
#include <QSet>
#include <QStringList>

class ThematicTileLayer : public QObject
{
    Q_OBJECT

public:
    // Tile key structure
    struct TileKey {
        int x, y, z;
        QString layerName;  // Thematic layer name (e.g., "SUNGAI_AR_25K")

        TileKey(int x_ = 0, int y_ = 0, int z_ = 0, const QString &layer = QString())
            : x(x_), y(y_), z(z_), layerName(layer) {}

        bool operator==(const TileKey &other) const {
            return x == other.x && y == other.y && z == other.z && layerName == other.layerName;
        }

        bool operator<(const TileKey &other) const {
            if (layerName != other.layerName) return layerName < other.layerName;
            if (z != other.z) return z < other.z;
            if (x != other.x) return x < other.x;
            return y < other.y;
        }

        friend uint qHash(const TileKey &key, uint seed);
    };

    explicit ThematicTileLayer(QObject *parent = nullptr);
    virtual ~ThematicTileLayer();

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setViewport(double minLat, double maxLat, double minLon, double maxLon, int zoomLevel);
    void setWidgetSize(int width, int height);

    // Layer management
    void setActiveLayers(const QStringList &layers);
    QStringList getActiveLayers() const { return m_activeLayers; }
    void addLayer(const QString &layerName);
    void removeLayer(const QString &layerName);
    void clearLayers();

    // Get available thematic layers from disk
    QStringList getAvailableLayers() const;

    QPixmap getTile(int x, int y, int z, const QString &layerName);
    QPixmap getTileWithFallback(int x, int y, int z, const QString &layerName);

    static QString getTilesDir();
    static qint64 getCacheSize();

    // Tile coordinate conversions (same as satellite)
    static int lonToTileX(double lon, int zoom);
    static int latToTileY(double lat, int zoom);
    static double tileXToLon(int x, int zoom);
    static double tileYToLat(int y, int zoom);
    static int calculateZoomLevel(double scaleLat, double viewportHeight);

    // Web Mercator (EPSG:3857) conversion functions
    static void webMercatorToGeographic(double x, double y, double &lat, double &lon);
    static void geographicToWebMercator(double lat, double lon, double &x, double &y);
    static double tileXToWebMercatorX(int x, int zoom);
    static double tileYToWebMercatorY(int y, int zoom);

signals:
    void tileUpdated(int x, int y, int z, const QString &layerName);
    void allTilesLoaded();

private:
    QString getTilePath(int x, int y, int z, const QString &layerName) const;
    bool loadTileFromDisk(int x, int y, int z, const QString &layerName, QPixmap &pixmap);
    void updateNeededTiles();
    void scanAvailableLayers();

    bool m_enabled;
    QMap<TileKey, QPixmap> m_tileCache;
    QSet<TileKey> m_neededTiles;
    QMutex m_cacheMutex;

    QStringList m_activeLayers;        // Layers currently being displayed
    QStringList m_availableLayers;     // All layers found on disk

    double m_minLat, m_maxLat, m_minLon, m_maxLon;
    int m_zoomLevel;
    int m_widgetWidth, m_widgetHeight;
};

#endif // _thematic_tile_layer_h_
