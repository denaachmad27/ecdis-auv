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
    void onImportCatalog();
    void onUpdateS63All();
    void onUpdateS63PerTile();

    private:
        struct TileRow {
            QString id;
            QString lastUpdate;
        };

    EcWidget* m_ecw;
    QString m_dencPath;
    QTableWidget* m_table;
    QPushButton* m_refreshBtn;
    QPushButton* m_focusBtn;
    QPushButton* m_importCatalogBtn;
    QPushButton* m_updateS63AllBtn;
    QPushButton* m_updateS63PerTileBtn;
    bool m_catalogAvailable = false;

    QList<TileRow> scanTiles() const;
        void populate();
        bool selectedTile(TileRow& out) const;
    bool hasAnyCatalog() const;
    void updateFocusEnabled();
    QString updatesLogPath() const;
    QMap<QString, QDateTime> readUpdateLog() const;
    bool writeUpdateLog(const QMap<QString, QDateTime>& map) const;
    // No data date parsing needed anymore
};

#endif // CHARTMANAGERPANEL_H
