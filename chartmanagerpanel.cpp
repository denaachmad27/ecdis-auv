#include "chartmanagerpanel.h"
#include "ecwidget.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QDirIterator>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGridLayout>

static QString fmtTime(const QDateTime& dt) {
    if (!dt.isValid()) return QString("-");
    return dt.toString("yyyy-MM-dd HH:mm");
}

ChartManagerPanel::ChartManagerPanel(EcWidget* ecw, const QString& dencPath, QWidget* parent)
    : QWidget(parent), m_ecw(ecw), m_dencPath(dencPath)
{
    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels(QStringList() << "Tile ID" << "Last Update");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_refreshBtn = new QPushButton("Refresh", this);
    m_importCatalogBtn = new QPushButton("Import Catalog", this);
    m_focusBtn = new QPushButton("Focus", this);
    m_updateS63PerTileBtn = new QPushButton("Update Selected", this);
    m_updateS63AllBtn = new QPushButton("Update All", this);

    // Unify button sizing and behavior
    const int btnMinH = 30;
    for (QPushButton* b : { m_refreshBtn, m_importCatalogBtn, m_focusBtn,
                            m_updateS63PerTileBtn, m_updateS63AllBtn }) {
        b->setMinimumHeight(btnMinH);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    // Arrange buttons into a clean 2x3 grid
    auto grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    // Row 1
    grid->addWidget(m_refreshBtn,        0, 0);
    grid->addWidget(m_importCatalogBtn,  0, 1);
    grid->addWidget(m_focusBtn,          0, 2);
    // Row 2
    grid->addWidget(m_updateS63PerTileBtn,  1, 0);
    grid->addWidget(m_updateS63AllBtn,      1, 1);

    // Make columns expand evenly
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_table);
    lay->addLayout(grid);
    // lay->addLayout(row3);
    setLayout(lay);

    connect(m_refreshBtn, &QPushButton::clicked, this, &ChartManagerPanel::onRefresh);
    connect(m_focusBtn, &QPushButton::clicked, this, &ChartManagerPanel::onFocusSelected);
    connect(m_importCatalogBtn, &QPushButton::clicked, this, &ChartManagerPanel::onImportCatalog);
    connect(m_updateS63AllBtn, &QPushButton::clicked, this, &ChartManagerPanel::onUpdateS63All);
    connect(m_updateS63PerTileBtn, &QPushButton::clicked, this, &ChartManagerPanel::onUpdateS63PerTile);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &ChartManagerPanel::onDoubleClick);

    m_catalogAvailable = hasAnyCatalog();
    updateFocusEnabled();
    populate();
}

bool ChartManagerPanel::hasAnyCatalog() const
{
    if (m_dencPath.isEmpty()) return false;
    QDirIterator it(m_dencPath, QStringList() << "CATALOG.031", QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext();
}

void ChartManagerPanel::updateFocusEnabled()
{
    m_focusBtn->setEnabled(m_catalogAvailable);
}

QList<ChartManagerPanel::TileRow> ChartManagerPanel::scanTiles() const
{
    QList<TileRow> rows;
    if (m_dencPath.isEmpty()) return rows;
    const QDir cellsDir(m_dencPath + "/CELLS");
    const auto logMap = readUpdateLog();
    if (cellsDir.exists()) {
        QDirIterator it(cellsDir.absolutePath(), QStringList() << "*.000", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString base = it.next();
            QFileInfo baseFi(base);
            TileRow r;
            r.id = baseFi.completeBaseName();
            if (logMap.contains(r.id)) r.lastUpdate = fmtTime(logMap.value(r.id));
            else r.lastUpdate = QString("-");
            rows.push_back(r);
        }
    }

    if (rows.isEmpty()) {
        // Fallback: scan entire DENC path for plausible ENC IDs with allowed suffix (.000, .7CB)
        QRegularExpression idPattern("^[A-Z]{2}[0-9A-Z]{5,6}$");
        QMap<QString, QFileInfo> bestById;
        QDirIterator anyIt(m_dencPath, QDir::Files, QDirIterator::Subdirectories);
        while (anyIt.hasNext()) {
            const QString fp = anyIt.next();
            QFileInfo fi(fp);
            // Skip Trash and non-cells utility dirs
            const QString ap = fi.absoluteFilePath();
            if (ap.contains("/_TRASH/") || ap.contains("\\_TRASH\\") || ap.contains("/CRTS/") || ap.contains("/CATALOGS/")
                || ap.contains("\\CRTS\\") || ap.contains("\\CATALOGS\\")) continue;
            const QString baseName = fi.completeBaseName();
            const QString suffix = fi.suffix().toLower();
            if (!idPattern.match(baseName).hasMatch()) continue;
            if (!(suffix == "000" || suffix == "7cb")) continue;
            if (!bestById.contains(baseName) || suffix == "000") bestById.insert(baseName, fi);
        }
        for (auto it = bestById.constBegin(); it != bestById.constEnd(); ++it) {
            TileRow r; r.id = it.key();
            if (logMap.contains(r.id)) r.lastUpdate = fmtTime(logMap.value(r.id));
            else r.lastUpdate = QString("-");
            rows.push_back(r);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const TileRow& a, const TileRow& b){ return a.id < b.id; });
    return rows;
}

void ChartManagerPanel::populate()
{
    const auto rows = scanTiles();
    m_table->setRowCount(rows.size());
    int row=0; for (const auto& r : rows) {
        m_table->setItem(row, 0, new QTableWidgetItem(r.id));
        m_table->setItem(row, 1, new QTableWidgetItem(r.lastUpdate));
        row++;
    }
    m_table->resizeColumnsToContents();
}

bool ChartManagerPanel::selectedTile(TileRow& out) const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return false;
    int row = items.first()->row();
    out.id = m_table->item(row, 0)->text();
    out.lastUpdate = m_table->item(row, 1)->text();
    return true;
}

void ChartManagerPanel::onRefresh()
{
    m_catalogAvailable = hasAnyCatalog();
    updateFocusEnabled();
    populate();
}

void ChartManagerPanel::onDoubleClick(QTableWidgetItem*)
{
    if (!m_catalogAvailable) {
        QMessageBox::information(this, tr("Chart Manager"), tr("You need to import CATALOG.031 first!"));
        return;
    }
    onFocusSelected();
}

void ChartManagerPanel::onFocusSelected()
{
    if (!m_catalogAvailable) {
        QMessageBox::information(this, tr("Chart Manager"), tr("You need to import CATALOG.031 first!"));
        return;
    }
    TileRow sel; if (!selectedTile(sel)) return; if (!m_ecw) return;
    if (!m_ecw->focusTileById(sel.id)) {
        QMessageBox::information(this, tr("Focus Tile"), tr("Tidak dapat menentukan extent untuk %1").arg(sel.id));
    }
}

void ChartManagerPanel::onImportCatalog()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Choose CATALOG.031"), QString(), tr("CATALOG (CATALOG.031);;All Files (*.*)"));
    if (file.isEmpty()) return;
    QFileInfo fi(file);
    const QString srcDirName = fi.absoluteDir().dirName();
    QDir targetBase(m_dencPath + "/CATALOGS");
    if (!targetBase.exists()) targetBase.mkpath(".");
    QDir targetDir(targetBase.absolutePath() + "/" + srcDirName);
    if (!targetDir.exists()) targetBase.mkpath(srcDirName);
    const QString targetCat = targetDir.absolutePath() + "/CATALOG.031";
    if (QFile::exists(targetCat)) QFile::remove(targetCat);
    if (!QFile::copy(file, targetCat)) {
        QMessageBox::critical(this, tr("Import Catalog"), tr("Import CATALOG.031 failed"));
        return;
    }
    m_catalogAvailable = true;
    updateFocusEnabled();
    QMessageBox::information(this, tr("Import Catalog"), tr("CATALOG.031 imported."));
}

QString ChartManagerPanel::updatesLogPath() const
{
    return QDir(m_dencPath).filePath("tile_updates.json");
}

QMap<QString, QDateTime> ChartManagerPanel::readUpdateLog() const
{
    QMap<QString, QDateTime> map;
    QFile f(updatesLogPath());
    if (!f.exists()) return map;
    if (!f.open(QIODevice::ReadOnly)) return map;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return map;
    const auto obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        map.insert(it.key(), QDateTime::fromString(it.value().toString(), Qt::ISODate));
    }
    return map;
}

bool ChartManagerPanel::writeUpdateLog(const QMap<QString, QDateTime>& map) const
{
    QJsonObject obj;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        obj.insert(it.key(), it.value().toString(Qt::ISODate));
    }
    QJsonDocument doc(obj);
    QFile f(updatesLogPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(doc.toJson(QJsonDocument::Compact));
    f.close();
    return true;
}



// onUpdateS57Selected removed per request

void ChartManagerPanel::onUpdateS63All()
{
    TileRow sel; if (!selectedTile(sel)) return; if (!m_ecw) return;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pilih root Exchange Set (Update All)"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;
    bool ok = m_ecw->ImportS63ExchangeSet(dir);
    if (ok) {
        m_ecw->ApplyUpdate();
        // Persist last update time for selected tile
        auto logMap = readUpdateLog();
        logMap.insert(sel.id, QDateTime::currentDateTime());
        writeUpdateLog(logMap);
        QMessageBox::information(this, tr("Update"), tr("Berhasil update dari: %1").arg(dir));
        onRefresh();
    } else {
        QMessageBox::critical(this, tr("Update"), tr("Import gagal: %1").arg(dir));
    }
}

void ChartManagerPanel::onUpdateS63PerTile()
{
    TileRow sel; if (!selectedTile(sel)) return; if (!m_ecw) return;
    // Pilih ENC ROOT
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pilih root Exchange Set (Per-Tile)"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    // Baca permit sumber dari DENC path
    const QString permitSrc = QDir(m_dencPath).filePath("S63permits.txt");
    if (!QFile::exists(permitSrc)) {
        QMessageBox::critical(this, tr("Update"), tr("S63permits.txt tidak ditemukan di: %1").arg(m_dencPath));
        return;
    }

    QFile in(permitSrc);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Update"), tr("Gagal membuka S63permits.txt"));
        return;
    }
    const QString text = QString::fromUtf8(in.readAll());
    in.close();

    const QStringList lines = text.split(QRegExp("\r?\n"), Qt::KeepEmptyParts);
    QStringList header;
    QStringList encLines;
    bool inEnc = false;
    for (const QString& ln : lines) {
        if (ln.trimmed().isEmpty()) continue;
        if (ln.startsWith(":ENC")) { inEnc = true; continue; }
        if (ln.startsWith(":ECS")) { inEnc = false; break; }
        if (!inEnc) {
            if (ln.startsWith(":DATE") || ln.startsWith(":VERSION")) header << ln;
        } else {
            if (ln.startsWith(sel.id)) encLines << ln;
        }
    }

    if (encLines.isEmpty()) {
        QMessageBox::critical(this, tr("Update"), tr("Permit untuk tile %1 tidak ditemukan di S63permits.txt").arg(sel.id));
        return;
    }

    if (header.isEmpty()) {
        header << QString(":DATE %1").arg(QDateTime::currentDateTime().toString("yyyyMMdd HH:mm"));
        header << ":VERSION 2";
    }

    // Tulis permit terfilter ke file sementara
    const QString filteredPermit = QDir::tempPath() + "/ecdis_s63_permits_" + sel.id + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".txt";
    QFile out(filteredPermit);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Update"), tr("Gagal menulis permit terfilter"));
        return;
    }
    QTextStream ts(&out);
    ts.setCodec("UTF-8");
    for (const QString& h : header) ts << h << "\n";
    ts << ":ENC\n";
    for (const QString& e : encLines) ts << e << "\n";
    ts << ":ECS\n";
    out.close();

    // Import dengan permit terfilter
    bool ok = m_ecw->ImportS63ExchangeSetWithPermitFile(dir, filteredPermit);
    if (ok) {
        m_ecw->ApplyUpdate();
        auto logMap = readUpdateLog();
        logMap.insert(sel.id, QDateTime::currentDateTime());
        writeUpdateLog(logMap);
        QMessageBox::information(this, tr("Update"), tr("Berhasil update tile %1 dari: %2").arg(sel.id).arg(dir));
        onRefresh();
    } else {
        QMessageBox::critical(this, tr("Update"), tr("Import gagal untuk tile %1 dari: %2").arg(sel.id).arg(dir));
    }
}

// Delete Tile feature removed per request
