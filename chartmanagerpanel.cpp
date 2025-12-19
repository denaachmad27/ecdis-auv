#include "chartmanagerpanel.h"
#include "ecwidget.h"
#include "SettingsManager.h"

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
#include <QDateTime>
#include <QDate>
#include <QDate>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QBrush>
#include <QColor>

namespace {
struct UpdateAllResult {
    QString error;
    int updatedCount = 0;
};

struct UpdateSelectedResult {
    QString error;
    bool tileFoundInCatalog = false;
    bool updated = false;
};

static QString readFileAll(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

static bool catalogContainsTileInContent(const QString& content, const QString& tileId) {
    if (content.isEmpty()) return false;
    const QString needle1 = tileId;
    const QString needle2 = tileId + ".000";
    return content.contains(needle1, Qt::CaseInsensitive) || content.contains(needle2, Qt::CaseInsensitive);
}
}

static QString fmtTime(const QDateTime& dt) {
    if (!dt.isValid()) return QString("-");
    return dt.toString("yyyy-MM-dd HH:mm");
}

ChartManagerPanel::ChartManagerPanel(EcWidget* ecw, const QString& dencPath, QWidget* parent)
    : QWidget(parent), m_ecw(ecw), m_dencPath(dencPath), m_sortOrder(Qt::AscendingOrder), m_sortColumn(SortByISDT)
{
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << "Tile ID" << "ISDT" << "Last Update" << "Catalog Path");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(false);

    m_refreshBtn = new QPushButton("Refresh", this);
    m_updateS63PerTileBtn = new QPushButton("Update Selected", this);
    m_updateS63AllBtn = new QPushButton("Update All", this);

    // Unify button sizing and behavior
    const int btnMinH = 30;
    for (QPushButton* b : { m_refreshBtn,
                            m_updateS63PerTileBtn, m_updateS63AllBtn }) {
        b->setMinimumHeight(btnMinH);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    // Arrange buttons in a single horizontal row
    auto row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    row->addWidget(m_refreshBtn);
    row->addWidget(m_updateS63PerTileBtn);
    row->addWidget(m_updateS63AllBtn);
    row->addStretch(1);

    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_table);
    lay->addLayout(row);
    // lay->addLayout(row3);
    setLayout(lay);

    connect(m_refreshBtn, &QPushButton::clicked, this, &ChartManagerPanel::onRefresh);
    connect(m_updateS63AllBtn, &QPushButton::clicked, this, &ChartManagerPanel::onUpdateS63All);
    connect(m_updateS63PerTileBtn, &QPushButton::clicked, this, &ChartManagerPanel::onUpdateS63PerTile);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &ChartManagerPanel::onDoubleClick);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ChartManagerPanel::onSelectionChanged);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &ChartManagerPanel::onHeaderClicked);
    populate();
    updateButtonsEnabled();
}

// Focus button removed; double-click still focuses selected tile

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
            const QString cat = findNearestCatalogFor(base, r.id);
            r.isdt = parseISDTFromCatalog(cat, r.id);
            r.catalogPath = cat;
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
            const QString cat = findNearestCatalogFor(it.value().absoluteFilePath(), r.id);
            r.isdt = parseISDTFromCatalog(cat, r.id);
            r.catalogPath = cat;
            if (logMap.contains(r.id)) r.lastUpdate = fmtTime(logMap.value(r.id));
            else r.lastUpdate = QString("-");
            rows.push_back(r);
        }
    }

    // Sort based on current sort column and order
    std::sort(rows.begin(), rows.end(), [this](const TileRow& a, const TileRow& b) {
        switch (m_sortColumn) {
        case SortByTileId:
            return m_sortOrder == Qt::AscendingOrder ? a.id < b.id : a.id > b.id;
        case SortByISDT:
            return compareISDT(a.isdt, b.isdt, m_sortOrder);
        case SortByLastUpdate:
            return compareDateTime(a.lastUpdate, b.lastUpdate, m_sortOrder);
        case SortByCatalogPath:
            return m_sortOrder == Qt::AscendingOrder ? a.catalogPath < b.catalogPath : a.catalogPath > b.catalogPath;
        default:
            return a.id < b.id;
        }
    });
    return rows;
}

void ChartManagerPanel::populate()
{
    const auto rows = scanTiles();
    m_table->setRowCount(rows.size());

    const QDate currentDate = QDate::currentDate();

    // Get ISDT expiration days from settings (default: 7)
    int isdtExpirationDays = 7;
    try {
        isdtExpirationDays = SettingsManager::instance().data().isdtExpirationDays;
        if (isdtExpirationDays <= 0) isdtExpirationDays = 7; // Fallback to default if invalid
    } catch (...) {
        // If settings are not accessible, use default
        isdtExpirationDays = 7;
    }

    int row=0; for (const auto& r : rows) {
        // Create table items
        QTableWidgetItem* idItem = new QTableWidgetItem(r.id);
        QTableWidgetItem* isdtItem = new QTableWidgetItem(r.isdt.isEmpty() ? "-" : r.isdt);
        QTableWidgetItem* updateItem = new QTableWidgetItem(r.lastUpdate);
        QTableWidgetItem* catalogItem = new QTableWidgetItem(r.catalogPath.isEmpty() ? "-" : r.catalogPath);

        // Check if ISDT is expired (using configurable expiration days)
        bool isExpired = false;
        if (!r.isdt.isEmpty() && r.isdt != "-") {
            QDate isdtDate = QDate::fromString(r.isdt, "yyyy-MM-dd");
            if (isdtDate.isValid()) {
                QDate expiryDate = isdtDate.addDays(isdtExpirationDays);
                isExpired = currentDate > expiryDate;
            }
        }

        // Apply styling for expired rows
        if (isExpired) {
            // Light red background for expired rows
            QColor redColor(255, 220, 220); // Very light red
            QBrush expiredBackground(redColor);
            idItem->setBackground(expiredBackground);
            isdtItem->setBackground(expiredBackground);
            updateItem->setBackground(expiredBackground);
            catalogItem->setBackground(expiredBackground);

            // Set text color to black for better contrast
            QBrush blackText(Qt::black);
            idItem->setForeground(blackText);
            isdtItem->setForeground(blackText);
            updateItem->setForeground(blackText);
            catalogItem->setForeground(blackText);
        }

        // Add items to table
        m_table->setItem(row, 0, idItem);
        m_table->setItem(row, 1, isdtItem);
        m_table->setItem(row, 2, updateItem);
        m_table->setItem(row, 3, catalogItem);
        row++;
    }
    m_table->resizeColumnsToContents();

    // Update header text with sort indicator
    QStringList headers;
    switch (m_sortColumn) {
    case SortByTileId:
        headers = QStringList() << (QString("Tile ID ") + (m_sortOrder == Qt::AscendingOrder ? "▲" : "▼"))
                               << "ISDT" << "Last Update" << "Catalog Path";
        break;
    case SortByISDT:
        headers = QStringList() << "Tile ID" << (QString("ISDT ") + (m_sortOrder == Qt::AscendingOrder ? "▲" : "▼"))
                               << "Last Update" << "Catalog Path";
        break;
    case SortByLastUpdate:
        headers = QStringList() << "Tile ID" << "ISDT"
                               << (QString("Last Update ") + (m_sortOrder == Qt::AscendingOrder ? "▲" : "▼"))
                               << "Catalog Path";
        break;
    case SortByCatalogPath:
        headers = QStringList() << "Tile ID" << "ISDT" << "Last Update"
                               << (QString("Catalog Path ") + (m_sortOrder == Qt::AscendingOrder ? "▲" : "▼"));
        break;
    default:
        headers = QStringList() << "Tile ID" << "ISDT" << "Last Update" << "Catalog Path";
        break;
    }
    m_table->setHorizontalHeaderLabels(headers);
}

bool ChartManagerPanel::selectedTile(TileRow& out) const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return false;
    int row = items.first()->row();
    out.id = m_table->item(row, 0)->text();
    // Column 2 holds Last Update after reordering columns
    out.lastUpdate = m_table->item(row, 2) ? m_table->item(row, 2)->text() : QString();
    return true;
}

void ChartManagerPanel::refreshChartManager()
{
    populate();
    updateButtonsEnabled();
}

void ChartManagerPanel::onRefresh()
{
    populate();
    updateButtonsEnabled();
}

void ChartManagerPanel::onDoubleClick(QTableWidgetItem*)
{
    onFocusSelected();
}

void ChartManagerPanel::onFocusSelected()
{
    TileRow sel; if (!selectedTile(sel)) return; if (!m_ecw) return;
    if (!m_ecw->focusTileById(sel.id)) {
        QMessageBox::information(this, tr("Focus Tile"), tr("Tidak dapat menentukan extent untuk %1").arg(sel.id));
    }
}

// Import catalog removed; catalogs are resolved per-tile automatically

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
    if (!m_ecw) return;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pilih root Exchange Set (Update All)"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    m_updateS63AllBtn->setEnabled(false);
    m_updateS63PerTileBtn->setEnabled(false);
    QProgressDialog* prog = new QProgressDialog(tr("Mengimpor S-63 (All)..."), QString(), 0, 0, this);
    prog->setCancelButton(nullptr);
    prog->setWindowModality(Qt::ApplicationModal);
    prog->show();

    auto task = [this, dir]() -> UpdateAllResult {
        UpdateAllResult res;
        QString err;
        if (!m_ecw->ImportS63ExchangeSetSilent(dir, &err)) { res.error = err; return res; }

        // Load catalog content once
        const QString catPath = dir + "/CATALOG.031";
        const QString catContent = readFileAll(catPath);
        if (catContent.isEmpty()) { res.error = QString("Failed to read %1").arg(catPath); return res; }

        // Associate CATALOG.031 only for tiles present in catalog
        QDirIterator it(dir, QStringList() << "*.000", QDir::Files, QDirIterator::Subdirectories);
        QSet<QString> seen;
        while (it.hasNext()) {
            QFileInfo fi(it.next());
            const QString tileId = fi.completeBaseName();
            if (seen.contains(tileId)) continue;
            seen.insert(tileId);
        }
        auto logMap = readUpdateLog();
        const QDateTime now = QDateTime::currentDateTime();
        for (const QString& tid : seen) {
            if (catalogContainsTileInContent(catContent, tid)) {
                copyCatalogForTile(tid, dir);
                logMap.insert(tid, now);
                res.updatedCount++;
            }
        }
        writeUpdateLog(logMap);
        return res;
    };

    auto watcher = new QFutureWatcher<UpdateAllResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, prog, dir]() {
        prog->close();
        prog->deleteLater();
        const UpdateAllResult res = watcher->future().result();
        watcher->deleteLater();
        if (res.error.isEmpty()) {
            m_ecw->ApplyUpdate();
            if (res.updatedCount > 0) {
                QMessageBox::information(this, tr("Update"), tr("Berhasil update dari: %1").arg(dir));
            } else {
                QMessageBox::information(this, tr("Update"), tr("Tidak ada tile yang ter-update dari: %1").arg(dir));
            }
        } else {
            QMessageBox::critical(this, tr("Update"), tr("Import gagal: %1\n%2").arg(dir, res.error));
        }
        onRefresh();
        updateButtonsEnabled();
    });
    watcher->setFuture(QtConcurrent::run(task));
}

void ChartManagerPanel::onUpdateS63PerTile()
{
    TileRow sel; if (!selectedTile(sel)) return; if (!m_ecw) return;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Pilih root Exchange Set (Per-Tile)"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    m_updateS63AllBtn->setEnabled(false);
    m_updateS63PerTileBtn->setEnabled(false);
    QProgressDialog* prog = new QProgressDialog(tr("Mengimpor S-63 (Selected)..."), QString(), 0, 0, this);
    prog->setCancelButton(nullptr);
    prog->setWindowModality(Qt::ApplicationModal);
    prog->show();

    auto task = [this, dir, sel]() -> UpdateSelectedResult {
        UpdateSelectedResult res;
        // Read permits
        const QString permitSrc = QDir(m_dencPath).filePath("S63permits.txt");
        if (!QFile::exists(permitSrc)) { res.error = QString("S63permits.txt not found: %1").arg(m_dencPath); return res; }
        QFile in(permitSrc);
        if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) { res.error = QString("Failed to open S63permits.txt"); return res; }
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
        if (encLines.isEmpty()) { res.error = QString("Permit for tile not found in S63permits.txt"); return res; }
        if (header.isEmpty()) {
            header << QString(":DATE %1").arg(QDateTime::currentDateTime().toString("yyyyMMdd HH:mm"));
            header << ":VERSION 2";
        }
        const QString filteredPermit = QDir::tempPath() + "/ecdis_s63_permits_" + sel.id + "_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".txt";
        QFile out(filteredPermit);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { res.error = QString("Failed to write filtered permit"); return res; }
        QTextStream ts(&out);
        ts.setCodec("UTF-8");
        for (const QString& h : header) ts << h << "\n";
        ts << ":ENC\n";
        for (const QString& e : encLines) ts << e << "\n";
        ts << ":ECS\n";
        out.close();

        QString err;
        if (!m_ecw->ImportS63ExchangeSetWithPermitFileSilent(dir, filteredPermit, &err)) { res.error = err; return res; }

        // Check catalog contains selected tile before replacing
        const QString catContent = readFileAll(dir + "/CATALOG.031");
        res.tileFoundInCatalog = catalogContainsTileInContent(catContent, sel.id);
        if (!res.tileFoundInCatalog) {
            res.updated = false;
            return res;
        }

        // Attach catalog and update log
        copyCatalogForTile(sel.id, dir);
        auto logMap = readUpdateLog();
        logMap.insert(sel.id, QDateTime::currentDateTime());
        writeUpdateLog(logMap);
        res.updated = true;
        return res;
    };

    auto watcher = new QFutureWatcher<UpdateSelectedResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, prog, dir, sel]() {
        prog->close();
        prog->deleteLater();
        const UpdateSelectedResult res = watcher->future().result();
        watcher->deleteLater();
        if (!res.error.isEmpty()) {
            QMessageBox::critical(this, tr("Update"), tr("Import gagal untuk tile %1 dari: %2\n%3").arg(sel.id).arg(dir).arg(res.error));
        } else if (!res.tileFoundInCatalog) {
            QMessageBox::information(this, tr("Update"), tr("Pada update ini tidak ada tile %1 pada CATALOG, tidak ada perubahan.").arg(sel.id));
        } else if (res.updated) {
            m_ecw->ApplyUpdate();
            QMessageBox::information(this, tr("Update"), tr("Berhasil update tile %1 dari: %2").arg(sel.id).arg(dir));
        } else {
            QMessageBox::information(this, tr("Update"), tr("Tidak ada perubahan untuk tile %1").arg(sel.id));
        }
        onRefresh();
        updateButtonsEnabled();
    });
    watcher->setFuture(QtConcurrent::run(task));
}

// Delete Tile feature removed per request

QString ChartManagerPanel::findNearestCatalogFor(const QString& cellPath, const QString& tileId) const
{
    // 1) Prefer per-tile catalog if present
    const QString perTile = tileCatalogTargetPath(tileId);
    if (QFile::exists(perTile)) return perTile;

    // 2) Otherwise, search upwards from the cell path
    QFileInfo fi(cellPath);
    QDir dir = fi.absoluteDir();
    const QString stopRoot = QDir(m_dencPath).absolutePath();
    for (int i = 0; i < 8; ++i) {
        const QString cat = dir.filePath("CATALOG.031");
        if (QFile::exists(cat)) return cat;
        if (!dir.cdUp()) break;
        if (dir.absolutePath().length() < stopRoot.length()) break;
    }
    // 3) Fallback: if any catalog exists under CATALOGS/<tileId>, use it
    const QString fallbackPerTile = QDir(m_dencPath + "/CATALOGS/" + tileId).filePath("CATALOG.031");
    if (QFile::exists(fallbackPerTile)) return fallbackPerTile;

    // 4) Last resort: first catalog found anywhere under CATALOGS (non-deterministic)
    QDir dencRoot(m_dencPath + "/CATALOGS");
    if (dencRoot.exists()) {
        QDirIterator it2(dencRoot.absolutePath(), QStringList() << "CATALOG.031", QDir::Files, QDirIterator::Subdirectories);
        if (it2.hasNext()) return it2.next();
    }
    return QString();
}

QString ChartManagerPanel::parseISDTFromCatalog(const QString& catalogPath, const QString& tileId) const
{
    if (catalogPath.isEmpty()) return QString();
    QFile f(catalogPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QString content = QString::fromUtf8(f.readAll());
    f.close();
    int pos = content.indexOf(tileId, 0, Qt::CaseInsensitive);
    if (pos < 0) pos = content.indexOf(tileId + ".000", 0, Qt::CaseInsensitive);
    if (pos < 0) return QString();
    const int window = 800;
    int start = qMax(0, pos - window);
    int end = qMin(content.size(), pos + window);
    QString snippet = content.mid(start, end - start);
    QRegularExpression reISDT("ISDT\\s*=?\\s*([0-9]{8})");
    auto m = reISDT.match(snippet);
    if (m.hasMatch()) {
        const QString yyyymmdd = m.captured(1);
        QDate d = QDate::fromString(yyyymmdd, "yyyyMMdd");
        if (d.isValid()) return d.toString("yyyy-MM-dd");
        return yyyymmdd;
    }
    return QString();
}

QString ChartManagerPanel::tileCatalogTargetPath(const QString& tileId) const
{
    QDir catalogsRoot(m_dencPath + "/CATALOGS");
    if (!catalogsRoot.exists()) catalogsRoot.mkpath(".");
    QDir tileDir(catalogsRoot.filePath(tileId));
    if (!tileDir.exists()) catalogsRoot.mkpath(tileId);
    return tileDir.filePath("CATALOG.031");
}

bool ChartManagerPanel::copyCatalogForTile(const QString& tileId, const QString& exchangeSetDir) const
{
    // Try direct and recursive search for CATALOG.031 inside exchange set
    QString srcCatalog;
    QFileInfo direct(exchangeSetDir + "/CATALOG.031");
    if (direct.exists()) {
        srcCatalog = direct.absoluteFilePath();
    } else {
        QDirIterator it(exchangeSetDir, QStringList() << "CATALOG.031", QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) srcCatalog = it.next();
    }
    if (srcCatalog.isEmpty()) return false;
    const QString target = tileCatalogTargetPath(tileId);
    if (QFile::exists(target)) QFile::remove(target);
    return QFile::copy(srcCatalog, target);
}

void ChartManagerPanel::onSelectionChanged()
{
    updateButtonsEnabled();
}

void ChartManagerPanel::updateButtonsEnabled()
{
    const bool hasSelection = !m_table->selectedItems().isEmpty();
    const bool hasRows = m_table->rowCount() > 0;
    if (m_updateS63PerTileBtn) m_updateS63PerTileBtn->setEnabled(hasSelection);
    if (m_updateS63AllBtn) m_updateS63AllBtn->setEnabled(hasRows);
}

void ChartManagerPanel::onHeaderClicked(int column)
{
    if (column < 0 || column >= 4) return;

    if (m_sortColumn == column) {
        // Toggle sort order if same column clicked
        m_sortOrder = (m_sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        // New column, set to ascending order
        m_sortColumn = column;
        m_sortOrder = Qt::AscendingOrder;
    }

    populate();
}

bool ChartManagerPanel::compareISDT(const QString& a, const QString& b, Qt::SortOrder order) const
{
    if (a.isEmpty() && b.isEmpty()) return false;
    if (a.isEmpty() || a == "-") return order == Qt::AscendingOrder;
    if (b.isEmpty() || b == "-") return order == Qt::DescendingOrder;

    // Both dates are in "yyyy-MM-dd" format, so string comparison works
    return order == Qt::AscendingOrder ? a < b : a > b;
}

bool ChartManagerPanel::compareDateTime(const QString& a, const QString& b, Qt::SortOrder order) const
{
    if (a.isEmpty() && b.isEmpty()) return false;
    if (a.isEmpty()) return order == Qt::AscendingOrder;
    if (b.isEmpty()) return order == Qt::DescendingOrder;

    // Format is "yyyy-MM-dd HH:mm", string comparison works for this format too
    return order == Qt::AscendingOrder ? a < b : a > b;
}
