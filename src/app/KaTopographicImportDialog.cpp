#include "KaCrashGuard.h"
#include "KaTopographicImportDialog.h"
#include "core/LayerOps.h"
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QCryptographicHash>
#include <QEventLoopLocker>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QScopedValueRollback>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>
#include <algorithm>
#include <functional>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeedback.h>
#include <qgsexception.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayerstyle.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgstaskmanager.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgslinesymbol.h>
#include <qgsfillsymbol.h>
#include <qgsmarkersymbol.h>
#include <qgsrulebasedrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>

namespace {
bool initiallySelected(TopographicCatalog::Category category) {
  using C = TopographicCatalog::Category;
  return category == C::Contour || category == C::Road || category == C::Building ||
         category == C::Water || category == C::Boundary;
}
QString geomFamilyName(Qgis::GeometryType type) {
  switch (type) {
    case Qgis::GeometryType::Point: return QStringLiteral("Point");
    case Qgis::GeometryType::Line: return QStringLiteral("Line");
    case Qgis::GeometryType::Polygon: return QStringLiteral("Polygon");
    default: return QString();
  }
}
QString geomWord(Qgis::GeometryType type) {
  switch (type) {
    case Qgis::GeometryType::Line: return QStringLiteral("선");
    case Qgis::GeometryType::Polygon: return QStringLiteral("면");
    case Qgis::GeometryType::Point: return QStringLiteral("점");
    default: return QString();
  }
}
QgsVectorLayer* mergeTopographicLayers(const QList<QgsVectorLayer*>& srcs, const QString& name) {
  if (srcs.isEmpty() || !srcs.first()) return nullptr;
  QgsVectorLayer* first = srcs.first();
  const QString type = first->geometryType() == Qgis::GeometryType::Polygon ? QStringLiteral("Polygon")
      : first->geometryType() == Qgis::GeometryType::Point ? QStringLiteral("Point")
                                                           : QStringLiteral("LineString");
  const QString crs = first->crs().isValid() ? first->crs().authid() : QStringLiteral("EPSG:5186");
  auto* mem = new QgsVectorLayer(QStringLiteral("%1?crs=%2").arg(type, crs), name, QStringLiteral("memory"));
  if (!mem->isValid()) {
    delete mem;
    return nullptr;
  }
  mem->dataProvider()->addAttributes(first->fields().toList());
  mem->updateFields();
  QgsFeatureList outs;
  for (QgsVectorLayer* src : srcs) {
    if (!src || !src->isValid()) continue;
    QgsFeatureIterator it = src->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      QgsFeature o(mem->fields());
      o.setGeometry(f.geometry());
      for (int i = 0; i < mem->fields().size(); ++i) {
        const int srcIdx = src->fields().indexOf(mem->fields().at(i).name());
        if (srcIdx >= 0) o.setAttribute(i, f.attribute(srcIdx));
      }
      outs.append(o);
    }
  }
  mem->dataProvider()->addFeatures(outs);
  mem->updateExtents();
  if (first->renderer()) mem->setRenderer(first->renderer()->clone());
  if (first->labelsEnabled() && first->labeling()) {
    mem->setLabeling(first->labeling()->clone());
    mem->setLabelsEnabled(true);
  }
  mem->setCrs(first->crs());
  mem->setReadOnly(true);
  LayerOps::markReferenceLayer(mem);
  return mem;
}
void styleTopographic(QgsVectorLayer* layer, const TopographicCatalog::Record& record) {
  const auto category = record.category;
  const bool dxf = !record.geometryType.isEmpty();
  // A 1:25,000 index contour is 50 m. z_min/z_max inspect the complete geometry,
  // so a nonlevel line or missing Z never receives an invented elevation label.
  const QString levelZ = QStringLiteral("z_min($geometry) IS NOT NULL AND z_max($geometry) IS NOT NULL AND abs(z_max($geometry)-z_min($geometry)) < 0.001");
  const QString majorZ = QStringLiteral("(%1) AND abs(z_min($geometry)-50*round(z_min($geometry)/50)) < 0.001").arg(levelZ);
  using C = TopographicCatalog::Category;
  const QColor color(128, 128, 128);
  if (layer->geometryType() == Qgis::GeometryType::Line) {
    auto line = QgsLineSymbol::createSimple({{QStringLiteral("line_color"), color.name()},
      {QStringLiteral("line_width"), QStringLiteral("0.2")}, {QStringLiteral("line_width_unit"), QStringLiteral("MM")}});
    layer->setRenderer(new QgsSingleSymbolRenderer(line.release()));
  } else if (layer->geometryType() == Qgis::GeometryType::Polygon) {
    const bool house = category == C::Building;
    layer->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple({
      {QStringLiteral("color"), house ? QColor(128, 128, 128, 70).name(QColor::HexArgb) : color.name()},
      {QStringLiteral("outline_color"), color.name()},
      {QStringLiteral("outline_width"), house ? QStringLiteral("0.25") : QStringLiteral("0.2")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
      {QStringLiteral("style"), house ? QStringLiteral("solid") : QStringLiteral("no")}}).release()));
  } else if (layer->geometryType() == Qgis::GeometryType::Point) {
    layer->setRenderer(new QgsSingleSymbolRenderer(QgsMarkerSymbol::createSimple({
      {QStringLiteral("color"), color.name()}, {QStringLiteral("outline_color"), color.name()},
      {QStringLiteral("outline_width"), QStringLiteral("0.2")}, {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
      {QStringLiteral("size"), QStringLiteral("1.2")}}).release()));
  }
  QString expression;
  if (category == C::Contour && layer->geometryType() == Qgis::GeometryType::Line && layer->fields().indexOf(QStringLiteral("z_valid")) >= 0)
    expression = QStringLiteral("CASE WHEN \"z_valid\" = 1 AND abs(\"elevation\"-50*round(\"elevation\"/50)) < 0.001 THEN format_number(\"elevation\", 0) END");
  else if (category == C::ElevationPoint && layer->geometryType() == Qgis::GeometryType::Point && layer->fields().indexOf(QStringLiteral("z_valid")) >= 0)
    expression = QStringLiteral("CASE WHEN \"z_valid\" = 1 THEN format_number(\"elevation\", 1) END");
  else if (dxf && category == C::Contour && layer->geometryType() == Qgis::GeometryType::Line)
    expression = QStringLiteral("CASE WHEN %1 THEN format_number(z_min($geometry), 0) END").arg(majorZ);
  else if (dxf && category == C::ElevationPoint && layer->geometryType() == Qgis::GeometryType::Point)
    expression = QStringLiteral("CASE WHEN %1 THEN format_number(z_min($geometry), 1) END").arg(levelZ);
  else if (category == C::PlaceName && layer->fields().indexOf(QStringLiteral("Text")) >= 0)
    expression = QStringLiteral("trim(\"Text\")");
  else if (category == C::Contour && layer->fields().indexOf(QStringLiteral("구분")) >= 0 &&
      layer->fields().indexOf(QStringLiteral("등고수치")) >= 0)
    expression = QStringLiteral("CASE WHEN \"구분\" = '계곡선' THEN format_number(\"등고수치\", 0) END");
  else if (category == C::ElevationPoint && layer->fields().indexOf(QStringLiteral("수치")) >= 0)
    expression = QStringLiteral("format_number(\"수치\", 1)");
  if (category == C::ElevationPoint && layer->geometryType() == Qgis::GeometryType::Point &&
      layer->fields().indexOf(QStringLiteral("Layer")) >= 0 && layer->fields().indexOf(QStringLiteral("Text")) >= 0) {
    // NGII F0027132 is a surveyed elevation annotation. Its DXF text insertion
    // Z can be zero even when Text is 119.2; preserve real point Z for other codes.
    const auto geometricLabel=expression.isEmpty()?QStringLiteral("NULL"):expression;
    expression=QStringLiteral("CASE WHEN \"Layer\" = 'F0027132' THEN CASE WHEN "
      "regexp_match(trim(\"Text\"), '^[+-]?[0-9]+([.][0-9]+)?$') = 1 "
      "THEN format_number(try(to_real(trim(\"Text\"))), 1) END ELSE %1 END").arg(geometricLabel);
  }
  if (!expression.isEmpty()) {
    QgsPalLayerSettings labels; labels.fieldName = expression; labels.isExpression = true;
    labels.placement = category == C::Contour ? Qgis::LabelPlacement::Line : Qgis::LabelPlacement::OrderedPositionsAroundPoint;
    QgsTextFormat format; format.setSize(8); format.setColor(color); labels.setFormat(format);
    layer->setLabeling(new QgsVectorLayerSimpleLabeling(labels)); layer->setLabelsEnabled(true);
  }
}
QString uri(const TopographicCatalog::Record& record) {
  QVariantMap parts{{QStringLiteral("path"), record.source}};
  if (!record.layerName.isEmpty()) parts.insert(QStringLiteral("layerName"), record.layerName);
  if (!record.geometryType.isEmpty()) parts.insert(QStringLiteral("geometryType"), record.geometryType);
  return QgsProviderRegistry::instance()->encodeUri(QStringLiteral("ogr"), parts);
}
std::unique_ptr<QgsVectorLayer> openVector(const TopographicCatalog::Record& record) {
  QgsVectorLayer::LayerOptions options(QgsProject::instance()->transformContext());
  options.forceReadOnly = true;
  options.skipCrsValidation = true; // This dialog explicitly confirms CRS before adding anything.
  auto layer = std::make_unique<QgsVectorLayer>(uri(record), record.displayName.isEmpty() ? record.layerName : record.displayName, QStringLiteral("ogr"), options);
  if (!record.geometryType.isEmpty()) {
    QString value = record.cadLayer; value.replace(QLatin1Char('\''), QStringLiteral("''"));
    if (!layer->isValid() || !layer->setSubsetString(QStringLiteral("\"Layer\" = '%1'").arg(value))) return {};
  }
  return layer;
}
QString crsLabel(const QgsCoordinateReferenceSystem& crs) {
  const QHash<QString, QString> labels = {
    {QStringLiteral("EPSG:5185"), QStringLiteral("서부원점(GRS80)")},
    {QStringLiteral("EPSG:5186"), QStringLiteral("중부원점(GRS80)")},
    {QStringLiteral("EPSG:5187"), QStringLiteral("동부원점(GRS80)")},
    {QStringLiteral("EPSG:5188"), QStringLiteral("동해원점(GRS80)")},
    {QStringLiteral("EPSG:5173"), QStringLiteral("서부원점(보정 베셀)")},
    {QStringLiteral("EPSG:5174"), QStringLiteral("중부원점(보정 베셀)")},
    {QStringLiteral("EPSG:5175"), QStringLiteral("제주원점(보정 베셀)")},
    {QStringLiteral("EPSG:5176"), QStringLiteral("동부원점(보정 베셀)")},
    {QStringLiteral("EPSG:5177"), QStringLiteral("동해원점(보정 베셀)")},
    {QStringLiteral("EPSG:5178"), QStringLiteral("UTM-K(베셀)")},
    {QStringLiteral("EPSG:5179"), QStringLiteral("UTM-K(GRS80)")},
    {QStringLiteral("EPSG:4326"), QStringLiteral("경위도(WGS84)")}};
  if (labels.contains(crs.authid())) return labels.value(crs.authid()) + QStringLiteral(" · ") + crs.authid();
  return crs.isValid() ? crs.description() + QStringLiteral(" · ") + crs.authid()
                       : QStringLiteral("좌표계 미확인 — 선택 필요");
}
class CatalogTask final : public QgsTask {
public:
  using Done = std::function<void(const TopographicCatalog::ScanResult&)>;
  CatalogTask(QString folder, Done done) : QgsTask(QStringLiteral("수치지형도 폴더 확인"), CanCancel),
      m_folder(std::move(folder)), m_done(std::move(done)) {}
  bool run() override {
    QgsFeedback feedback;
    connect(&feedback, &QgsFeedback::progressChanged, &feedback, [this](double p) { setProgress(p); });
    try { m_result = TopographicCatalog::scan(m_folder, &feedback, {}, [this] { return isCanceled(); }); }
    catch (...) {
      KaCrashGuard::logLine(QStringLiteral("[except] app/KaTopographicImportDialog.cpp:213")); m_result.error = QStringLiteral("수치지형도 파일을 확인하지 못했습니다. 원본은 변경하지 않았습니다."); }
    return !isCanceled() && m_result.error.isEmpty();
  }
  void finished(bool) override {
    if (isCanceled()) m_result.canceled = true;
    m_done(m_result);
    m_done = {};
  }
private:
  QEventLoopLocker m_quitLock;
  QString m_folder;
  Done m_done;
  TopographicCatalog::ScanResult m_result;
};
}

KaTopographicImportDialog::KaTopographicImportDialog(QgsMapCanvas* canvas, QWidget* parent)
    : QDialog(parent), m_canvas(canvas) {
  setWindowTitle(QStringLiteral("수치지형도 — 좌표계 확인과 지도 추가"));
  resize(1050, 720);
  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(QStringLiteral("도엽의 좌표계를 확인한 뒤 추가하세요. 좌표 숫자만으로 구·신 측지계나 원점대를 확정할 수 없습니다.\n"
      "미리보기에서 도로·건물·하천이 기존 지도와 맞는지 확인하세요. 원본 파일은 변경하지 않습니다."), this);
  hint->setWordWrap(true); layout->addWidget(hint);
  m_table = new QTableWidget(0, 4, this);
  m_table->setObjectName(QStringLiteral("topographicFiles"));
  m_table->setHorizontalHeaderLabels({QStringLiteral("추가 / 레이어"), QStringLiteral("파일"),
                                    QStringLiteral("확인할 원본 좌표계"), QStringLiteral("좌표 범위")});
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_table->setColumnWidth(2, 280); m_table->setColumnWidth(3, 185);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(m_table, 1);
  m_preview = new QgsMapCanvas(this); m_preview->setMinimumHeight(180);
  m_preview->setCanvasColor(Qt::white); layout->addWidget(m_preview, 1);
  auto* controls = new QHBoxLayout;
  auto* enabled = new QCheckBox(QStringLiteral("수치지형도 표시"), this);
  enabled->setChecked(true);
  connect(enabled, &QCheckBox::toggled, this, &KaTopographicImportDialog::setMapsEnabled);
  controls->addWidget(enabled);
  auto* preview = new QPushButton(QStringLiteral("선택 도엽 정합 미리보기"), this);
  m_import = new QPushButton(QStringLiteral("좌표계 확인 완료 · 지도에 추가"), this);
  m_import->setEnabled(false); m_import->setObjectName(QStringLiteral("importTopographic"));
  auto* cancel = new QPushButton(QStringLiteral("색인 취소"), this);
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  for (auto* button : {preview, m_import, cancel, close}) controls->addWidget(button);
  layout->addLayout(controls);
  m_progress = new QProgressBar(this); m_progress->setRange(0, 100); layout->addWidget(m_progress);
  m_status = new QLabel(this); m_status->setObjectName(QStringLiteral("topographicImportStatus"));
  m_status->setWordWrap(true); layout->addWidget(m_status);
  connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
    if (row < 0 || row >= m_records.size()) return;
    const QString reason = m_reviewReasons.value(recordKey(m_records[row]));
    if (!reason.isEmpty()) m_status->setText(QStringLiteral("검토 필요: %1. 자동으로 지도에 추가하지 않습니다.").arg(reason));
  });
  connect(preview, &QPushButton::clicked, this, &KaTopographicImportDialog::previewCurrent);
  connect(m_import, &QPushButton::clicked, this, &KaTopographicImportDialog::importSelected);
  connect(close, &QPushButton::clicked, this, &QDialog::hide);
  connect(cancel, &QPushButton::clicked, this, [this] { if (m_task) m_task->cancel(); });
  m_panTimer = new QTimer(this); m_panTimer->setSingleShot(true); m_panTimer->setInterval(400);
  m_loadTimer = new QTimer(this); m_loadTimer->setInterval(0);
  connect(m_loadTimer, &QTimer::timeout, this, &KaTopographicImportDialog::loadNext);
  if (canvas) {
    connect(canvas, &QgsMapCanvas::extentsChanged, m_panTimer, qOverload<>(&QTimer::start));
    connect(canvas, &QObject::destroyed, this, [this] {
      ++m_coverageGeneration;
      m_panTimer->stop(); m_loadTimer->stop(); m_pending.clear(); m_prepared.clear(); updateAutomaticLoading();
    });
  }
  connect(m_panTimer, &QTimer::timeout, this, &KaTopographicImportDialog::updateCoverage);
  connect(QgsProject::instance(), &QgsProject::aboutToBeCleared, this, [this] {
    ++m_coverageGeneration; m_prepared.clear();
    ++m_generation; if (m_task) m_task->cancel(); m_panTimer->stop(); m_loadTimer->stop(); m_pending.clear();
    m_confirmed.clear(); m_loaded.clear(); m_styles.clear(); m_visibility.clear(); m_userDeleted.clear(); m_reviewReasons.clear();
    m_automaticRecords.clear(); m_manualRecords.clear(); updateAutomaticLoading();
    clearPreview();
    m_records.clear(); m_table->setRowCount(0); m_import->setEnabled(false);
  });
  connect(QgsProject::instance(), qOverload<const QStringList&>(&QgsProject::layersWillBeRemoved), this,
      [this](const QStringList& ids) {
    clearPreview();
    if (m_updatingCoverage) return;
    // A user-deleted layer stays deleted when the map is moved.
    for (auto it = m_loaded.begin(); it != m_loaded.end();) {
      if (!ids.contains(it.value())) { ++it; continue; }
      const QString key = it.key();
      m_userDeleted.insert(key);
      m_confirmed.removeIf([this, &key](const auto& record) { return recordKey(record) == key; });
      m_pending.removeIf([this, &key](const auto& record) { return recordKey(record) == key; });
      it = m_loaded.erase(it);
    }
  });
}
KaTopographicImportDialog::~KaTopographicImportDialog() {
  if (m_task) m_task->cancel();
  clearPreview();
}
void KaTopographicImportDialog::clearPreview() {
  m_preview->stopRendering(); m_preview->setLayers({});
  delete m_previewLayer; m_previewLayer = nullptr;
}
void KaTopographicImportDialog::setMapsEnabled(bool enabled) {
  m_mapsEnabled = enabled;
  updateCoverage();
}
QString KaTopographicImportDialog::recordKey(const TopographicCatalog::Record& record) const {
  return QString::fromLatin1(QCryptographicHash::hash((record.source + QLatin1Char('|') + record.layerName +
      QLatin1Char('|') + record.signature + QLatin1Char('|') + record.cadLayer + QLatin1Char('|') + record.geometryType).toUtf8(), QCryptographicHash::Sha256).toHex());
}
void KaTopographicImportDialog::setAutomaticLoadingEnabled(bool enabled) {
  if (enabled==m_automaticLoadingEnabled) return;
  m_automaticLoadingEnabled=enabled;
  if (enabled) { updateCoverage(); return; }
  m_pending.removeIf([this](const auto& record) { return m_automaticRecords.contains(recordKey(record)); });
  std::erase_if(m_prepared,[this](const auto& item) { return m_automaticRecords.contains(item.key); });
  if (m_pending.isEmpty() && m_prepared.empty()) m_loadTimer->stop();
  else m_loadTimer->start(); // A prepared manual import still has to be published.
  updateAutomaticLoading();
}
void KaTopographicImportDialog::updateAutomaticLoading() {
  int pending=0, loaded=0;
  if (m_automaticLoadingEnabled) for (const auto& record:m_pending) {
    if (m_automaticRecords.contains(recordKey(record))) ++pending;
  }
  for (auto it=m_loaded.cbegin();it!=m_loaded.cend();++it)
    if (m_automaticRecords.contains(it.key()) && QgsProject::instance()->mapLayer(it.value())) ++loaded;
  int prepared=0;
  for (const auto& item:m_prepared) if (m_automaticRecords.contains(item.key)) ++prepared;
  const bool loading=pending+prepared>0;
  if (loading!=m_automaticLoading) {
    m_automaticLoading=loading; emit automaticLoadingChanged(loading);
  }
  if (loaded+prepared!=m_reportedAutomaticLoaded || pending!=m_reportedAutomaticPending) {
    m_reportedAutomaticLoaded=loaded+prepared; m_reportedAutomaticPending=pending;
    emit automaticLoadingProgressChanged(loaded+prepared,pending);
  }
}
void KaTopographicImportDialog::scanFolder(const QString& folder) {
  if (m_task) m_task->cancel();
  const auto generation = ++m_generation;
  m_import->setEnabled(false); m_progress->setValue(0);
  m_status->setText(QStringLiteral("받은 자료를 확인하고 있습니다… 지도 작업을 계속할 수 있습니다."));
  const QPointer<KaTopographicImportDialog> guard(this);
  auto* task = new CatalogTask(folder, [guard, generation](const auto& result) {
    if (!guard || guard->m_generation != generation) return;
    guard->m_task.clear(); guard->populate(result);
  });
  m_task = task;
  connect(task, &QgsTask::progressChanged, this, [this, generation](double p) {
    if (generation == m_generation) m_progress->setValue(qRound(p));
  });
  QgsApplication::taskManager()->addTask(task); show(); raise();
}
void KaTopographicImportDialog::populate(const TopographicCatalog::ScanResult& result) {
  if (result.canceled || !result.error.isEmpty()) {
    m_status->setText(result.canceled ? QStringLiteral("색인을 취소했습니다. 기존 지도는 유지됩니다.") : result.error);
    return;
  }
  QHash<QString, QPair<Qt::CheckState, QString>> pendingChoices;
  for (int row = 0; row < m_records.size() && row < m_table->rowCount(); ++row) {
    const auto* choice = qobject_cast<QComboBox*>(m_table->cellWidget(row, 2));
    if (choice && m_table->item(row, 0))
      pendingChoices.insert(recordKey(m_records[row]), {m_table->item(row, 0)->checkState(), choice->currentData().toString()});
  }
  m_records = result.records; m_table->setRowCount(m_records.size());
  QSettings settings;
  for (int i = 0; i < m_records.size(); ++i) {
    const auto& record = m_records[i];
    const auto pending = pendingChoices.constFind(recordKey(record));
    const auto reason = m_reviewReasons.value(recordKey(record));
    auto* name = new QTableWidgetItem((record.displayName.isEmpty() ? record.layerName : record.displayName) +
        (reason.isEmpty() ? QString() : QStringLiteral(" [검토 필요]")));
    name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    name->setCheckState(pending != pendingChoices.cend() ? pending->first :
        (reason.isEmpty() && initiallySelected(record.category) ? Qt::Checked : Qt::Unchecked));
    name->setToolTip(TopographicCatalog::categoryName(record.category) +
        (reason.isEmpty() ? QString() : QStringLiteral("\n검토 필요: ") + reason)); m_table->setItem(i, 0, name);
    auto* file = new QTableWidgetItem(QFileInfo(record.source).fileName());
    file->setToolTip(record.source); file->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); m_table->setItem(i, 1, file);
    auto* choices = new QComboBox(m_table); choices->setObjectName(QStringLiteral("topographicCrs_%1").arg(i));
    choices->addItem(QStringLiteral("좌표계 미확인 — 선택 필요"), QString());
    const QgsCoordinateReferenceSystem metadata(record.crsWkt);
    if (metadata.isValid()) choices->addItem(QStringLiteral("파일 정보: ") + crsLabel(metadata), metadata.toWkt());
    for (int code : {5185, 5186, 5187, 5188, 5173, 5174, 5175, 5176, 5177, 5178, 5179, 4326}) {
      const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:%1").arg(code));
      if (crs.isValid()) choices->addItem(crsLabel(crs), crs.toWkt());
    }
    const QString remembered = settings.value(QStringLiteral("topographic/crs/") + recordKey(record)).toString();
    int selected = remembered.isEmpty() ? (metadata.isValid() ? 1 : 0) : choices->findData(remembered);
    if (pending != pendingChoices.cend()) selected = choices->findData(pending->second);
    if (selected < 0) { choices->addItem(QStringLiteral("확인한 좌표계"), pending != pendingChoices.cend() ? pending->second : remembered); selected = choices->count() - 1; }
    choices->setCurrentIndex(selected); m_table->setCellWidget(i, 2, choices);
    auto* bounds = new QTableWidgetItem(record.hasExtent ? QStringLiteral("X %1 ~ %2\nY %3 ~ %4")
        .arg(record.extent.xMinimum(), 0, 'f', 1).arg(record.extent.xMaximum(), 0, 'f', 1)
        .arg(record.extent.yMinimum(), 0, 'f', 1).arg(record.extent.yMaximum(), 0, 'f', 1) : QStringLiteral("범위 없음"));
    bounds->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); m_table->setItem(i, 3, bounds);
  }
  m_progress->setValue(100); m_import->setEnabled(!m_records.isEmpty());
  m_status->setText(QStringLiteral("레이어 %1개 · 파일 %2개 확인 · 캐시 %3개. 추가할 항목에 체크하세요.\n%4")
      .arg(m_records.size()).arg(result.filesRead).arg(result.cacheHits).arg(result.warnings.join(QLatin1Char('\n'))));
  if (!m_records.isEmpty()) m_table->selectRow(0);
}
void KaTopographicImportDialog::previewCurrent() {
  const int row = m_table->currentRow();
  if (!m_canvas || row < 0 || row >= m_records.size()) return;
  const auto* choices = qobject_cast<QComboBox*>(m_table->cellWidget(row, 2));
  const QgsCoordinateReferenceSystem crs(choices->currentData().toString());
  if (!crs.isValid()) { m_status->setText(QStringLiteral("먼저 미리 볼 원본 좌표계를 선택하세요.")); return; }
  const auto& record = m_records[row];
  auto layer = openVector(record);
  if (!layer || !layer->isValid()) { m_status->setText(QStringLiteral("이 레이어를 열 수 없습니다.")); return; }
  layer->setCrs(crs); layer->setReadOnly(true); styleTopographic(layer.get(), record);
  try {
    const auto target = m_canvas->mapSettings().destinationCrs();
    QgsCoordinateTransform transform(crs, target, QgsProject::instance());
    transform.setAllowFallbackTransforms(false); transform.setBallparkTransformsAreAppropriate(false);
    auto extent = transform.transformBoundingBox(layer->extent()); extent.scale(1.1);
    if (!extent.isFinite() || extent.isEmpty()) {
      m_status->setText(QStringLiteral("도엽의 유효한 범위를 확인하지 못했습니다.")); return;
    }
    clearPreview();
    m_previewLayer = layer.release();
    m_preview->setDestinationCrs(target);
    auto layers = m_canvas->layers(); layers.prepend(m_previewLayer); m_preview->setLayers(layers);
    m_preview->setExtent(extent); m_preview->refresh();
    m_status->setText(QStringLiteral("선택 좌표계의 미리보기입니다. 위성·지적과 정합을 확인한 뒤 추가하세요."));
  } catch (const QgsCsException&) { m_status->setText(QStringLiteral("선택 좌표계로 지도 위치를 계산하지 못했습니다.")); }
}
void KaTopographicImportDialog::importSelected() {
  QList<TopographicCatalog::Record> selected;
  for (int row = 0; row < m_records.size(); ++row) {
    if (m_table->item(row, 0)->checkState() != Qt::Checked) continue;
    auto record = m_records[row];
    if (!record.hasExtent || !record.extent.isFinite()) {
      m_status->setText(QStringLiteral("유효한 도형 범위가 없는 항목은 지도에 추가할 수 없습니다. 해당 체크를 해제하세요."));
      return;
    }
    auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 2)); record.crsWkt = combo->currentData().toString();
    if (!QgsCoordinateReferenceSystem(record.crsWkt).isValid()) {
      m_status->setText(QStringLiteral("좌표계가 확인되지 않은 레이어가 있습니다. 해당 좌표계를 선택하거나 추가 체크를 해제하세요."));
      return;
    }
    selected.append(record);
  }
  QSettings settings;
  for (const auto& record : selected) {
    const auto key = recordKey(record);
    settings.setValue(QStringLiteral("topographic/crs/") + key, record.crsWkt);
    m_userDeleted.remove(key); // Only an explicit manual selection opts back in.
    m_reviewReasons.remove(key);
    m_automaticRecords.remove(key); // Explicit manual imports are not canceled by the automatic downloader.
    m_manualRecords.insert(key);
    for (int row = 0; row < m_records.size(); ++row) if (recordKey(m_records[row]) == key) {
      if (auto* item = m_table->item(row, 0)) {
        item->setText(record.displayName.isEmpty() ? record.layerName : record.displayName);
        item->setToolTip(TopographicCatalog::categoryName(record.category));
      }
      break;
    }
    m_visibility.insert(key, true); // Explicit user selection also enables nondefault categories.
    if (auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(m_loaded.value(key)))
      node->setItemVisibilityChecked(true);
  }
  mergeConfirmed(selected);
}
void KaTopographicImportDialog::mergeConfirmed(const QList<TopographicCatalog::Record>& records) {
  for (const auto& record : records) {
    const auto key = recordKey(record);
    bool exists = false;
    for (auto& known : m_confirmed) if (recordKey(known) == key) {
      exists = true;
      if (known.crsWkt != record.crsWkt) {
        clearPreview();
        if (m_canvas) m_canvas->stopRendering();
        if (auto* layer = QgsProject::instance()->mapLayer(m_loaded.value(key))) {
          const QScopedValueRollback<bool> updating(m_updatingCoverage, true);
          QgsProject::instance()->removeMapLayer(layer);
        }
        m_loaded.remove(key);
        known = record;
      }
      break;
    }
    if (!exists) m_confirmed.append(record);
  }
  updateCoverage();
}
bool KaTopographicImportDialog::importVerified(const QList<TopographicCatalog::Record>& records, QString* error) {
  if (error) error->clear();
  for (const auto& record : records) {
    if (record.source.isEmpty() || record.layerName.isEmpty() || record.sourceSheet.isEmpty() ||
        !record.hasExtent || !record.extent.isFinite() ||
        !QgsCoordinateReferenceSystem(record.crsWkt).isValid()) {
      const QString message = QStringLiteral("도엽·원본 좌표계·범위가 확인되지 않아 자동으로 추가하지 않았습니다.");
      if (error) *error = message;
      m_status->setText(message); return false;
    }
  }
  ++m_generation;
  if (m_task) { m_task->cancel(); m_task.clear(); }
  // Preserve the complete downloaded code list, including unchecked categories.
  auto combined = m_records;
  for (const auto& record : records) {
    bool exists = false;
    for (auto& known : combined) if (recordKey(known) == recordKey(record)) { known = record; exists = true; break; }
    if (!exists) combined.append(record);
  }
  TopographicCatalog::ScanResult result; result.records = combined; populate(result);
  auto allowed = records;
  allowed.removeIf([this](const auto& record) {
    const auto key = recordKey(record);
    return m_userDeleted.contains(key) || m_reviewReasons.contains(key);
  });
  for (const auto& record:allowed) {
    const auto key=recordKey(record);
    if (!m_manualRecords.contains(key)) m_automaticRecords.insert(key);
  }
  mergeConfirmed(allowed);
  return true;
}
void KaTopographicImportDialog::retainForReview(const QList<TopographicCatalog::Record>& records, const QString& reason) {
  if (records.isEmpty()) return;
  ++m_generation;
  if (m_task) { m_task->cancel(); m_task.clear(); }
  auto combined = m_records;
  QSet<QString> approved;
  for (const auto& record : m_confirmed) approved.insert(recordKey(record));
  const QString explanation = reason.trimmed().isEmpty() ? QStringLiteral("원본 좌표계 또는 도형 범위 미확인") : reason;
  for (const auto& record : records) {
    const auto key = recordKey(record);
    if (approved.contains(key)) continue;
    if (!m_reviewReasons.contains(key)) {
      // Mark a newly held item unchecked even when it was present in a previous
      // folder scan; later refreshes preserve the user's pending manual choices.
      for (int row = 0; row < m_records.size(); ++row)
        if (recordKey(m_records[row]) == key && m_table->item(row, 0))
          m_table->item(row, 0)->setCheckState(Qt::Unchecked);
    }
    m_reviewReasons.insert(key, explanation);
    bool exists = false;
    for (auto& known : combined) if (recordKey(known) == key) { known = record; exists = true; break; }
    if (!exists) combined.append(record);
  }
  TopographicCatalog::ScanResult result; result.records = combined; populate(result);
  if (!records.isEmpty()) m_status->setText(QStringLiteral("검토 필요: %1. 미확인 항목은 목록에 보관하고 자동으로 지도에 추가하지 않습니다.").arg(explanation));
}
void KaTopographicImportDialog::updateCoverage() {
  if (!m_canvas) return;
  ++m_coverageGeneration; m_prepared.clear();
  m_loadTimer->stop(); m_pending.clear(); m_loadErrors.clear();
  const QScopedValueRollback<bool> updating(m_updatingCoverage, true);
  auto* project = QgsProject::instance();
  const auto found = TopographicCatalog::query(m_confirmed,
      TopographicCatalog::coverageBounds(m_canvas->extent()),
      m_canvas->mapSettings().destinationCrs().toWkt(), project->transformContext(),
      nullptr, {}, m_canvas->scale());
  if (found.canceled || !found.error.isEmpty()) {
    m_status->setText(found.error);
    if (!found.error.isEmpty() && m_automaticLoadingEnabled && !m_automaticRecords.isEmpty())
      emit automaticLoadingAttention(found.error);
    updateAutomaticLoading(); return;
  }
  QSet<QString> wanted;
  if (m_mapsEnabled) for (const auto& record : found.matches) wanted.insert(recordKey(record));
  // Explicit project-open may already have restored these reference layers.
  // Adopt them before scheduling datasource opens, preserving the user's stored
  // layer identity, renderer, opacity and visibility instead of adding duplicates.
  QSet<QString> confirmed;
  for (const auto& record : m_confirmed) confirmed.insert(recordKey(record));
  for (auto* raw : project->mapLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(raw);
    if (!layer || !LayerOps::isReferenceLayer(layer)) continue;
    QStringList keys = layer->customProperty(QStringLiteral("ka_hgis/topographic_source_keys")).toStringList();
    if (keys.isEmpty()) {
      const QString key = layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString();
      if (!key.isEmpty()) keys.append(key);
    }
    QgsMapLayerStyle style; style.readFromLayer(layer);
    const auto* node = project->layerTreeRoot()->findLayer(layer);
    const bool vis = node && node->itemVisibilityChecked();
    for (const QString& key : keys) {
      if (key.isEmpty() || !confirmed.contains(key) || m_loaded.contains(key)) continue;
      layer->setReadOnly(true);
      m_loaded.insert(key, layer->id());
      m_styles.insert(key, style.xmlData());
      m_visibility.insert(key, vis);
      if (layer->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString().isEmpty()) {
        for (const auto& rec : m_confirmed) {
          if (recordKey(rec) != key) continue;
          layer->setCustomProperty(QStringLiteral("ka_hgis/topographic_group"),
              QString::number(static_cast<int>(rec.category)) + QLatin1Char('|') +
              geomFamilyName(layer->geometryType()));
          break;
        }
      }
    }
  }
  clearPreview();
  bool stoppedForRemoval=false;
  const auto drawing = m_canvas->layers();
  QSet<QString> lostMembers;
  for (auto it = m_loaded.begin(); it != m_loaded.end();) {
    auto* layer = project->mapLayer(it.value());
    if (!wanted.contains(it.key())) {
      if (layer) {
        QgsMapLayerStyle style; style.readFromLayer(layer);
        m_styles[it.key()] = style.xmlData();
        const auto* node = project->layerTreeRoot()->findLayer(layer);
        m_visibility[it.key()] = node && node->itemVisibilityChecked();
        lostMembers.insert(layer->id());
      }
      it = m_loaded.erase(it);
    } else if (!layer) {
      it = m_loaded.erase(it);
    } else {
      ++it;
    }
  }
  QHash<QString, TopographicCatalog::Record> byKey;
  for (const auto& record : m_confirmed) byKey.insert(recordKey(record), record);
  for (const QString& lid : lostMembers) {
    QStringList remain;
    for (auto it = m_loaded.cbegin(); it != m_loaded.cend(); ++it) {
      if (it.value() == lid) remain.append(it.key());
    }
    auto* layer = project->mapLayer(lid);
    if (!layer) continue;
    if (!stoppedForRemoval && m_canvas->isDrawing() && drawing.contains(layer)) {
      m_canvas->stopRendering();
      stoppedForRemoval = true;
    }
    if (remain.isEmpty()) {
      project->removeMapLayer(layer);
      continue;
    }
    QgsMapLayerStyle style; style.readFromLayer(layer);
    const QString xml = style.xmlData();
    const auto* node = project->layerTreeRoot()->findLayer(layer);
    const bool vis = node && node->itemVisibilityChecked();
    for (const QString& key : remain) {
      m_styles[key] = xml;
      m_visibility[key] = vis;
    }
    project->removeMapLayer(layer);
    for (auto it = m_loaded.begin(); it != m_loaded.end();) {
      if (it.value() == lid) it = m_loaded.erase(it);
      else ++it;
    }
    for (const QString& key : remain) {
      const auto rec = byKey.constFind(key);
      if (rec != byKey.cend() && wanted.contains(key)) m_pending.append(*rec);
    }
  }
  if (m_mapsEnabled) for (const auto& record : found.matches) {
    if (record.category == TopographicCatalog::Category::ElevationPoint) continue;
    const auto key=recordKey(record);
    if (m_loaded.contains(key)) continue;
    bool queued = false;
    for (const auto& pending : m_pending) {
      if (recordKey(pending) == key) { queued = true; break; }
    }
    if (queued) continue;
    if (m_automaticLoadingEnabled || !m_automaticRecords.contains(key)) m_pending.append(record);
  }
  updateAutomaticLoading();
  LayerOps::syncMapCanvas(project, m_canvas, false);
  // Removing a hidden/subset layer can leave the visible list unchanged, so
  // syncMapCanvas need not schedule a replacement for the job we canceled.
  if (stoppedForRemoval && !m_canvas->isDrawing()) m_canvas->refresh();
  if (!m_pending.isEmpty()) m_loadTimer->start();
  else loadNext();
}
void KaTopographicImportDialog::loadNext() {
  if (!m_canvas) { m_loadTimer->stop(); m_pending.clear(); m_prepared.clear(); updateAutomaticLoading(); return; }
  auto* project = QgsProject::instance();
  const QScopedValueRollback<bool> updating(m_updatingCoverage, true);
  // Yield between datasource opens so progress, cancellation and map input work.
  if (!m_pending.isEmpty()) {
    const auto record = m_pending.takeFirst();
    const auto key = recordKey(record);
    const bool automatic=m_automaticRecords.contains(key);
    const auto generation=m_coverageGeneration;
    if (record.category == TopographicCatalog::Category::ElevationPoint) {
      updateAutomaticLoading();
    } else {
    auto layer = openVector(record);
    // A provider may dispatch events while opening. A cancel during that call
    // must not add its result afterwards.
    if (generation!=m_coverageGeneration || (automatic && !m_automaticLoadingEnabled)) {
      updateAutomaticLoading(); return;
    }
    bool failed=!layer || !layer->isValid();
    if (!failed) {
      layer->setCrs(QgsCoordinateReferenceSystem(record.crsWkt));
      layer->setReadOnly(true);
      LayerOps::markReferenceLayer(layer.get());
      layer->setCustomProperty(QStringLiteral("ka_hgis/topographic_source"), key);
      layer->setCustomProperty(QStringLiteral("ka_hgis/topographic_sheet"), record.sourceSheet);
      layer->setCustomProperty(QStringLiteral("ka_hgis/topographic_cad_layer"), record.cadLayer);
      styleTopographic(layer.get(), record);
      {
        const QString word = layer->geometryType() == Qgis::GeometryType::Line ? QStringLiteral("선")
            : layer->geometryType() == Qgis::GeometryType::Polygon ? QStringLiteral("면")
            : layer->geometryType() == Qgis::GeometryType::Point ? QStringLiteral("점") : QString();
        const QString base = layer->name();
        if (!word.isEmpty() && !base.endsWith(word))
          layer->setName(base + QStringLiteral(" · ") + word);
      }
      if (m_styles.contains(key)) QgsMapLayerStyle(m_styles.value(key)).writeToLayer(layer.get());
      // Opening yields between files, but project/legend changes wait for the
      // complete set. Otherwise every file restarts the full map and label pass.
      m_prepared.push_back({key,std::move(layer),m_visibility.value(key,initiallySelected(record.category)),record.category});
    }
    if (failed) {
      m_loadErrors.append(record.layerName);
      if (automatic)
        emit automaticLoadingAttention(QStringLiteral("받은 수치지형도 레이어를 열지 못했습니다: %1. 기존 지도와 받은 원본은 유지됩니다.")
          .arg(record.displayName.isEmpty()?record.layerName:record.displayName));
    }
    }
  }
  if (m_pending.isEmpty()) {
    m_loadTimer->stop();
    publishPrepared();
    LayerOps::syncMapCanvas(project, m_canvas, false); emit referenceLayersChanged();
  }
  m_status->setText(QStringLiteral("현재 화면 레이어 %1개 적재 · 대기 %2개 · 확인한 전체 %3개%4")
      .arg(m_loaded.size()).arg(m_pending.size()).arg(m_confirmed.size())
      .arg(m_loadErrors.isEmpty() ? QString() : QStringLiteral(" · 열기 실패: ") + m_loadErrors.join(QStringLiteral(", "))));
  updateAutomaticLoading();
}
void KaTopographicImportDialog::publishPrepared() {
  if (m_prepared.empty()) return;
  auto* project=QgsProject::instance();
  auto prepared=std::move(m_prepared);
  m_prepared.clear();
  // 수치지형도는 고고학 보고서에 「보이기만」 하면 되는 밑그림이다. 속성도 테마
  // 구분도 필요 없다고 사용자가 정했다. 선만 남겨 한 레이어로 합친다. 점 피처는
  // 속성을 버리면 뜻 없는 점만 남으므로 넣지 않는다.
  std::erase_if(prepared, [](const PreparedLayer& item) {
    if (!item.layer) return true;
    if (item.category == TopographicCatalog::Category::ElevationPoint) return true;
    return item.layer->geometryType() != Qgis::GeometryType::Line;
  });
  if (prepared.empty()) return;

  QMap<QString, QList<int>> groups;
  for (int i = 0; i < static_cast<int>(prepared.size()); ++i) {
    auto* lyr = prepared[static_cast<size_t>(i)].layer.get();
    if (!lyr) continue;
    // 테마·도엽을 가리지 않고 전부 한 덩어리로 묶는다.
    Q_UNUSED(lyr);
    groups[QStringLiteral("topo")].append(i);
  }

  struct PublishItem {
    QStringList keys;
    QgsVectorLayer* layer = nullptr;
    bool visible = false;
  };
  QList<PublishItem> pubs;
  QList<QgsMapLayer*> toAdd;
  for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
    QList<QgsVectorLayer*> srcs;
    QStringList keys, sources;
    bool visible = false;
    const auto cat = prepared[static_cast<size_t>(it.value().first())].category;
    Qgis::GeometryType geom = Qgis::GeometryType::Unknown;
    for (int idx : it.value()) {
      auto* lyr = prepared[static_cast<size_t>(idx)].layer.get();
      if (!lyr) continue;
      srcs.append(lyr);
      keys.append(prepared[static_cast<size_t>(idx)].key);
      sources.append(lyr->source());
      visible = visible || prepared[static_cast<size_t>(idx)].visible;
      geom = lyr->geometryType();
    }
    if (srcs.isEmpty()) continue;
    Q_UNUSED(cat);
    const QString group = QStringLiteral("topo");
    QgsVectorLayer* existing = nullptr;
    for (QgsMapLayer* raw : project->mapLayers()) {
      auto* vl = qobject_cast<QgsVectorLayer*>(raw);
      if (vl && vl->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString() == group) {
        existing = vl;
        break;
      }
    }
    if (existing) {
      srcs.prepend(existing);
      const QStringList oldKeys = existing->customProperty(QStringLiteral("ka_hgis/topographic_source_keys")).toStringList();
      const QStringList oldSources = existing->customProperty(QStringLiteral("ka_hgis/topographic_sources")).toStringList();
      keys = oldKeys + keys;
      sources = oldSources + sources;
    }
    Q_UNUSED(geom);
    const QString name = QStringLiteral("수치지형도");
    QgsVectorLayer* out = nullptr;
    if (srcs.size() == 1) {
      out = srcs.first();
      out->setName(name);
    } else {
      out = mergeTopographicLayers(srcs, name);
      if (!out) continue;
      if (existing) {
        for (auto loaded = m_loaded.begin(); loaded != m_loaded.end();) {
          if (loaded.value() == existing->id()) loaded = m_loaded.erase(loaded);
          else ++loaded;
        }
        project->removeMapLayer(existing);
      }
    }
    out->setCustomProperty(QStringLiteral("ka_hgis/topographic_source"), keys.first());
    out->setCustomProperty(QStringLiteral("ka_hgis/topographic_source_keys"), keys);
    out->setCustomProperty(QStringLiteral("ka_hgis/topographic_sources"), sources);
    out->setCustomProperty(QStringLiteral("ka_hgis/topographic_group"), group);
    toAdd.append(out);
    pubs.append({keys, out, visible});
  }

  const auto added=project->addMapLayers(toAdd);
  for (const auto& pub : pubs) {
    if (!added.contains(pub.layer)) {
      m_loadErrors.append(pub.layer ? pub.layer->name() : QStringLiteral("merge"));
      emit automaticLoadingAttention(QStringLiteral("수치지형도 레이어를 지도에 추가하지 못했습니다: %1")
          .arg(pub.layer ? pub.layer->name() : QStringLiteral("merge")));
      continue;
    }
    for (auto& item : prepared) {
      if (item.layer.get() == pub.layer) item.layer.release();
    }
    for (const QString& key : pub.keys) m_loaded.insert(key, pub.layer->id());
    LayerOps::placeInLegendGroup(project, pub.layer, QStringLiteral("참조 지도"));
    if (auto* node=project->layerTreeRoot()->findLayer(pub.layer))
      node->setItemVisibilityChecked(pub.visible);
  }
}
