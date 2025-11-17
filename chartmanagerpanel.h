#ifndef CHARTMANAGERPANEL_H
#define CHARTMANAGERPANEL_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>

class EcWidget;

class ChartManagerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ChartManagerPanel(EcWidget* ecw, const QString& dencPath, QWidget* parent = nullptr);

private slots:
    void onRefresh();
    void onFocusSelected();
    void onDoubleClick(QTableWidgetItem*);
    void onUpdateS63All();
    void onUpdateS63PerTile();
    void onSelectionChanged();

    private:
        struct TileRow {
            QString id;
            QString isdt;
            QString catalogPath;
            QString lastUpdate;
        };

    EcWidget* m_ecw;
    QString m_dencPath;
    QTableWidget* m_table;
    QPushButton* m_refreshBtn;
    QPushButton* m_updateS63AllBtn;
    QPushButton* m_updateS63PerTileBtn;

    QList<TileRow> scanTiles() const;
        void populate();
        bool selectedTile(TileRow& out) const;
        void updateButtonsEnabled();
    QString updatesLogPath() const;
    QMap<QString, QDateTime> readUpdateLog() const;
    bool writeUpdateLog(const QMap<QString, QDateTime>& map) const;
    QString findNearestCatalogFor(const QString& cellPath, const QString& tileId) const;
    QString parseISDTFromCatalog(const QString& catalogPath, const QString& tileId) const;
    QString tileCatalogTargetPath(const QString& tileId) const;
    bool copyCatalogForTile(const QString& tileId, const QString& exchangeSetDir) const;
    // No data date parsing needed anymore
};

#endif // CHARTMANAGERPANEL_H
