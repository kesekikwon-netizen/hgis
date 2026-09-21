#include "KaTopographicScopePanel.h"
#include "KaTopographicBrowser.h"
#include "KaTopographicImportDialog.h"
#include "core/TopographicArchive.h"
#include "core/TopographicCatalog.h"
#include "core/TopographicSheets.h"
#include "core/TopographicSourceCrs.h"
#include "core/TopographicShapefileCache.h"
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoopLocker>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QVBoxLayout>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrubberband.h>
#include <qgstaskmanager.h>
#include <qgsvectorlayer.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr double kRadiusKm = 5.;
using Record = TopographicCatalog::Record;
using PreparationProgress = std::function<void(const QString&,const QString&)>;
struct Prepared {
  QList<Record> records;
  QList<Record> reviewRecords;
  QStringList available;
  QMap<QString,QString> reviewMessages;
  QStringList crsNames;
  QStringList warnings;
  QString error;
  QString evidence;
};
Prepared prepareOne(const QString& source,const QJsonObject& metadata,const QString& library,
                    const QgsCoordinateTransformContext& context,const std::function<bool()>& canceled,
                    bool remember,const PreparationProgress& progress);
QString crsName(const QString& authId) {
  QString origin;
  const int code=authId.section(QLatin1Char(':'),1).toInt();
  switch (code) {
    case 5180: case 5185: origin=QStringLiteral("서부원점"); break;
    case 5181: case 5186: origin=QStringLiteral("중부원점"); break;
    case 5182: origin=QStringLiteral("제주원점"); break;
    case 5183: case 5187: origin=QStringLiteral("동부원점"); break;
    case 5184: case 5188: origin=QStringLiteral("동해원점"); break;
    default: return authId;
  }
  return QStringLiteral("%1(GRS80) · %2").arg(origin,authId);
}
QgsRectangle officialBounds(const QJsonObject& metadata) {
  for (const auto* key : {"minx","miny","maxx","maxy"})
    if (!metadata.value(QLatin1String(key)).isDouble() || !std::isfinite(metadata.value(QLatin1String(key)).toDouble())) return {};
  const double minx=metadata.value("minx").toDouble(), miny=metadata.value("miny").toDouble();
  const double maxx=metadata.value("maxx").toDouble(), maxy=metadata.value("maxy").toDouble();
  if (maxx<=minx || maxy<=miny) return {};
  return QgsRectangle(minx,miny,maxx,maxy);
}
QJsonObject publicMetadata(const QJsonObject& input) {
  QJsonObject result;
  for (const auto* key : {"source","num","name","scale","fileExt","projection","makeYer","historyNo","minx","miny","maxx","maxy"})
    if (input.contains(QLatin1String(key))) result.insert(QLatin1String(key),input.value(QLatin1String(key)));
  return result;
}
QString sheetLabel(const QJsonObject& metadata) {
  return QStringLiteral("%1 %2").arg(metadata.value("num").toString(),metadata.value("name").toString()).trimmed();
}
QByteArray sourceDigest(const QString& path,const std::function<bool()>& canceled) {
  QFile source(path);
  if (!source.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!source.atEnd()) {
    if (canceled()) return {};
    const auto bytes=source.read(1024*1024);
    if (bytes.isEmpty() && source.error()!=QFile::NoError) return {};
    hash.addData(bytes);
  }
  return canceled()?QByteArray():hash.result().toHex();
}
QString receiptSource(const QString& library,const QString& relative) {
  // Receipts only describe downloaded originals within this survey library.
  // Canonical containment rejects traversal, absolute paths and link escapes.
  if (relative.isEmpty() || !QDir::isRelativePath(relative) || QDir::cleanPath(relative)!=relative) return {};
  const QString root=QFileInfo(library).canonicalFilePath();
  const QFileInfo file(QDir(root).filePath(relative));
  const QString path=file.canonicalFilePath();
  if (root.isEmpty() || !file.isFile() || path.isEmpty() || file.isSymLink() || file.isJunction()) return {};
  const QString checked=QDir(root).relativeFilePath(path);
  if (!QDir::isRelativePath(checked) || checked==QLatin1String("..") || checked.startsWith(QLatin1String("../"))) return {};
  return path;
}
bool fileNamesMatchSheet(const QStringList& names,const QString& number) {
  if (names.isEmpty()) return false;
  static const QRegularExpression pattern(QStringLiteral("(?:^|_)([0-9]{6})(?=_|$)"));
  for (const auto& name:names) {
    if (QFileInfo(name).fileName()!=name || QFileInfo(name).suffix().compare(QLatin1String("dxf"),Qt::CaseInsensitive)!=0) return false;
    const auto match=pattern.match(QFileInfo(name).completeBaseName());
    if (!match.hasMatch() || match.captured(1)!=number) return false;
  }
  return true;
}
bool rememberReview(const QString& source,const QJsonObject& metadata,const QString& library,
                    const QStringList& dataNames,const QString& reason,const std::function<bool()>& canceled) {
  const QString relative=QDir(library).relativeFilePath(QFileInfo(source).canonicalFilePath());
  const auto path=receiptSource(library,relative);
  if (path.isEmpty() || !fileNamesMatchSheet(dataNames,metadata.value("num").toString())) return false;
  const auto digest=sourceDigest(path,canceled);
  if (digest.isEmpty()) return false;
  const QJsonObject item{{"schema",1},{"solverVersion",TopographicSourceCrs::kSolverVersion},
    {"state","review_required"},{"source",relative},
    {"sourceSha256",QString::fromLatin1(digest)},{"official",publicMetadata(metadata)},
    {"dataFiles",QJsonArray::fromStringList(dataNames)},{"reason",reason}};
  const auto bytes=QJsonDocument(item).toJson(QJsonDocument::Compact);
  const auto name=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+QStringLiteral(".json");
  const QDir directory(QDir(library).filePath(QStringLiteral("receipts")));
  if (canceled() || !QDir().mkpath(directory.path())) return false;
  QSaveFile output(directory.filePath(name));
  return output.open(QIODevice::WriteOnly) && output.write(bytes)==bytes.size() && !canceled() && output.commit();
}
void restoreReviewReceipts(Prepared& result,const QString& library,const QgsPointXY& center5179,double radius,
                          const QgsCoordinateTransformContext& context,const std::function<bool()>& canceled,
                          const PreparationProgress& progress) {
  const QDir directory(QDir(library).filePath(QStringLiteral("receipts")));
  const auto center=QgsGeometry::fromPointXY(center5179);
  for (const auto& file:directory.entryInfoList({QStringLiteral("*.json")},QDir::Files|QDir::NoSymLinks)) {
    if (canceled()) return;
    const QString invalid=QStringLiteral("검토 도엽의 보관 정보 또는 원본이 변경되어 다시 확인합니다: %1").arg(file.fileName());
    QFile input(file.absoluteFilePath());
    if (file.size()>65536 || !input.open(QIODevice::ReadOnly)) { result.warnings.append(invalid); continue; }
    const auto bytes=input.readAll();
    const auto item=QJsonDocument::fromJson(bytes).object();
    const auto metadata=item.value("official").toObject();
    const auto bounds=officialBounds(metadata);
    const auto number=metadata.value("num").toString();
    const auto payloadHash=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(item).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    if (item.value("schema").toInt()!=1 || item.value("state").toString()!=QLatin1String("review_required") ||
        file.completeBaseName()!=payloadHash || metadata!=publicMetadata(metadata) ||
        metadata.value("source").toString()!=QLatin1String("NGII suchiQuery.do") ||
        metadata.value("scale").toString()!=QLatin1String("25000") ||
        metadata.value("fileExt").toString().compare(QLatin1String("dxf"),Qt::CaseInsensitive)!=0 ||
        !TopographicSheets::resolve(number) || bounds.isEmpty() || item.value("reason").toString().trimmed().isEmpty()) {
      result.warnings.append(invalid); continue;
    }
    if (QgsGeometry::fromRect(bounds).distance(center)>radius || result.available.contains(number)) continue;
    const auto source=receiptSource(library,item.value("source").toString());
    QStringList names;
    for (const auto& name:item.value("dataFiles").toArray()) names.append(name.toString());
    const auto suffix=QFileInfo(source).suffix().toLower();
    if (source.isEmpty() || (suffix!=QLatin1String("dxf") && suffix!=QLatin1String("zip")) ||
        !fileNamesMatchSheet(names,number) ||
        (suffix==QLatin1String("dxf") && !fileNamesMatchSheet({QFileInfo(source).fileName()},number))) {
      result.warnings.append(invalid); continue;
    }
    const auto actual=sourceDigest(source,canceled);
    if (canceled()) return;
    if (actual.isEmpty() || actual!=item.value("sourceSha256").toString().toLatin1()) { result.warnings.append(invalid); continue; }
    const auto version=item.value("solverVersion");
    if (!version.isUndefined() && (!version.isDouble() || version.toDouble()!=version.toInt() ||
        version.toInt()<1 || version.toInt()>TopographicSourceCrs::kSolverVersion)) {
      result.warnings.append(invalid); continue;
    }
    if (version.toInt()!=TopographicSourceCrs::kSolverVersion) {
      // A solver update must re-evaluate the authenticated local original,
      // rather than request an already received sheet again or trust old CRS results.
      auto prepared=prepareOne(source,metadata,library,context,canceled,true,progress);
      if (canceled()) return;
      result.records.append(prepared.records); result.available.append(prepared.available);
      result.reviewRecords.append(prepared.reviewRecords);
      for(auto review=prepared.reviewMessages.cbegin();review!=prepared.reviewMessages.cend();++review)
        result.reviewMessages.insert(review.key(),review.value());
      result.crsNames.append(prepared.crsNames); result.crsNames.removeDuplicates();
      result.warnings.append(prepared.warnings);
      if(!prepared.error.isEmpty())result.warnings.append(prepared.error);
      if(!prepared.evidence.isEmpty())result.evidence=prepared.evidence;
      // Retire only this validated, generated receipt after its replacement
      // review receipt or successful local index has been committed.
      if(prepared.available.contains(number) || prepared.reviewMessages.contains(number)) {
        if(canceled())return;
        input.close();
        if(!QFile::remove(file.absoluteFilePath()))
          result.warnings.append(QStringLiteral("이전 검토 기록을 정리하지 못했습니다: %1").arg(number));
      } else {
        // Preparation/storage failure does not invalidate the verified received
        // original. Keep it excluded from new requests and retry locally later.
        result.reviewMessages.insert(number,item.value("reason").toString()+
          (prepared.error.isEmpty()?QString():QStringLiteral("\n")+prepared.error));
      }
      continue;
    }
    result.reviewMessages.insert(number,item.value("reason").toString());
  }
}
Prepared prepareOne(const QString& source, const QJsonObject& metadata, const QString& library,
                    const QgsCoordinateTransformContext& context, const std::function<bool()>& canceled,
                    bool remember, const PreparationProgress& progress) {
  Prepared result;
  const QString number=metadata.value("num").toString();
  progress(QStringLiteral("좌표계 확인"),QStringLiteral("받은 파일의 공식 도엽 정보를 확인하고 있습니다."));
  const auto bounds=officialBounds(metadata);
  if (metadata.value("source").toString()!=QLatin1String("NGII suchiQuery.do") ||
      metadata.value("scale").toString()!=QLatin1String("25000") ||
      metadata.value("fileExt").toString().compare(QLatin1String("dxf"),Qt::CaseInsensitive)!=0 ||
      !TopographicSheets::resolve(number) || bounds.isEmpty()) {
    result.error=QStringLiteral("받은 파일과 공식 1:25,000 DXF 도엽 정보를 연결하지 못했습니다. 자동 적재하지 않았습니다."); return result;
  }
  const bool compressed=QFileInfo(source).suffix().compare(QLatin1String("zip"),Qt::CaseInsensitive)==0;
  progress(compressed?QStringLiteral("압축 해제"):QStringLiteral("보관 자료 확인"),QFileInfo(source).fileName());
  const auto archive=TopographicArchive::prepare(source,QDir(library).filePath(QStringLiteral("files")),{},canceled);
  if (archive.canceled || canceled()) return result;
  if (!archive.error.isEmpty()) { result.error=archive.error; return result; }
  progress(QStringLiteral("좌표계 확인"),QStringLiteral("도엽 %1의 도형과 좌표 범위를 읽고 있습니다.").arg(number));
  const auto catalog=TopographicCatalog::scan(archive.directory,nullptr,QDir(library).filePath(QStringLiteral("catalog")),canceled);
  if (catalog.canceled || canceled()) return result;
  if (!catalog.error.isEmpty()) { result.error=catalog.error; return result; }
  result.warnings=catalog.warnings;
  QList<Record> records;
  for (const auto& record:catalog.records) {
    if (!archive.files.contains(record.source) || QFileInfo(record.source).suffix().compare(QLatin1String("dxf"),Qt::CaseInsensitive)!=0) continue;
    if (!record.sourceSheet.isEmpty() && record.sourceSheet!=number) {
      result.error=QStringLiteral("신청한 도엽과 받은 DXF의 도엽이 다릅니다. 자동 적재하지 않았습니다."); return result;
    }
    records.append(record);
  }
  progress(QStringLiteral("좌표계 확인"),QStringLiteral("도엽 %1의 좌표와 공식 도곽 위치를 대조하고 있습니다.").arg(number));
  const auto resolved=TopographicSourceCrs::resolve(number,metadata.value("projection").toString(),records,context,bounds);
  if (!resolved.error.isEmpty()) {
    const bool frameOnly=!records.isEmpty() && std::all_of(records.cbegin(),records.cend(),[](const Record& record) {
      return record.cadLayer==QLatin1String("H0017334");
    });
    result.error=QStringLiteral("%1 · %2: %3").arg(sheetLabel(metadata),
      frameOnly?QStringLiteral("도곽만 있음 · 위치 검토 필요"):QStringLiteral("검토 필요"),resolved.error);
    result.reviewRecords=records;
    if (remember && !records.isEmpty()) {
      QStringList names;
      for (const auto& record:records) names.append(QFileInfo(record.source).fileName());
      names.removeDuplicates(); names.sort();
      if (rememberReview(source,metadata,library,names,result.error,canceled)) result.reviewMessages.insert(number,result.error);
      else if (!canceled()) result.error+=QStringLiteral(" 검토 기록을 저장하지 못해 다음 요청 때 다시 확인합니다. 받은 원본은 유지됩니다.");
    }
    return result;
  }
  if (!resolved.warning.isEmpty()) result.warnings.append(resolved.warning);
  result.evidence=resolved.evidence;
  result.crsNames.append(crsName(resolved.authId));
  for (int i=0;i<records.size();++i) {
    if (resolved.unverifiedRecordIndexes.contains(i)) {
      auto record=records[i]; record.sourceSheet=number;
      result.reviewRecords.append(record);
      result.warnings.append(QStringLiteral("도곽 밖 또는 범위 미확인 항목: %1").arg(record.displayName)); continue;
    }
    auto record=records[i];
    // The initial DXF scan has no CRS metadata. Once the official sheet bounds
    // have confirmed its CRS, that preliminary warning no longer needs review.
    result.warnings.removeAll(QStringLiteral("좌표계 확인 필요: %1 · %2").arg(record.source,record.layerName));
    record.crsWkt=resolved.crsWkt; record.sourceSheet=number; result.records.append(record);
  }
  if (result.records.isEmpty()) { result.error=QStringLiteral("도엽 위치가 확인된 지도 항목이 없습니다."); return result; }
  if (canceled()) { result.records.clear(); return result; }
  progress(QStringLiteral("SHP 변환"),QStringLiteral("도엽 %1의 지도 항목 %2개를 변환하거나 보관된 SHP를 확인하고 있습니다.")
    .arg(number).arg(result.records.size()));
  const auto converted=TopographicShapefileCache::prepare(result.records,
      QDir(library).filePath(QStringLiteral("SHP")),context,canceled);
  if (converted.canceled || canceled()) { result.records.clear(); return result; }
  if (!converted.error.isEmpty()) { result.error=converted.error; result.records.clear(); return result; }
  result.records=converted.records;
  result.warnings.append(converted.warnings);
  progress(QStringLiteral("보관 자료 확인"),QStringLiteral("도엽 %1의 SHP 결과와 보관 정보를 정리하고 있습니다.").arg(number));
  if (remember) {
    const QString index=QDir(library).filePath(QStringLiteral("index"));
    const QString id=QString::fromLatin1(QCryptographicHash::hash(
        (QFileInfo(source).absoluteFilePath()+QLatin1Char('|')+number).toUtf8(),QCryptographicHash::Sha256).toHex());
    QSaveFile output(QDir(index).filePath(id+QStringLiteral(".json")));
    const auto json=QJsonDocument(QJsonObject{{"schema",1},{"source",QFileInfo(source).absoluteFilePath()},
      {"crsAuthId",resolved.authId},{"layerCrsWkt",resolved.crsWkt},
      {"official",publicMetadata(metadata)}}).toJson(QJsonDocument::Compact);
    if (!QDir().mkpath(index) || !output.open(QIODevice::WriteOnly) || output.write(json)!=json.size() || !output.commit()) {
      result.error=QStringLiteral("도엽 보관 정보를 저장하지 못했습니다. 받은 원본은 유지됩니다."); result.records.clear(); return result;
    }
  }
  result.available.append(number);
  return result;
}
Prepared restoreNearby(const QString& library,const QgsPointXY& center5179,double radius,
                       const QgsCoordinateTransformContext& context,const std::function<bool()>& canceled,
                       const PreparationProgress& progress) {
  Prepared result;
  progress(QStringLiteral("보관 자료 확인"),QStringLiteral("조사 폴더에 보관된 수치지형도를 확인하고 있습니다."));
  const QDir index(QDir(library).filePath(QStringLiteral("index")));
  const auto point=QgsGeometry::fromPointXY(center5179);
  const auto files=index.entryInfoList({QStringLiteral("*.json")},QDir::Files|QDir::NoSymLinks,QDir::Time);
  for (const auto& file:files) {
    if (canceled()) return {};
    if (file.size()>65536) { result.warnings.append(QStringLiteral("보관 정보가 너무 커서 확인하지 못했습니다.")); continue; }
    QFile input(file.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) { result.warnings.append(QStringLiteral("보관 정보를 읽지 못했습니다.")); continue; }
    const auto item=QJsonDocument::fromJson(input.readAll()).object();
    const auto metadata=item.value("official").toObject();
    const auto bounds=officialBounds(metadata);
    const auto number=metadata.value("num").toString();
    if (item.value("schema").toInt()!=1 || bounds.isEmpty() || result.available.contains(number)) continue;
    if (QgsGeometry::fromRect(bounds).distance(point)>radius) continue;
    auto prepared=prepareOne(item.value("source").toString(),metadata,library,context,canceled,false,progress);
    if (!prepared.error.isEmpty()) { result.warnings.append(prepared.error); continue; }
    result.records.append(prepared.records); result.available.append(prepared.available);
    result.reviewRecords.append(prepared.reviewRecords); result.crsNames.append(prepared.crsNames); result.crsNames.removeDuplicates();
    result.warnings.append(prepared.warnings);
    if (!prepared.evidence.isEmpty()) result.evidence=prepared.evidence;
  }
  restoreReviewReceipts(result,library,center5179,radius,context,canceled,progress);
  return result;
}
class LibraryTask final:public QgsTask {
public:
  using Work=std::function<Prepared(const std::function<bool()>&,const PreparationProgress&)>;
  LibraryTask(Work work,std::function<void(const Prepared&,bool)> done,PreparationProgress progress)
    :QgsTask(QStringLiteral("수치지형도 준비"),QgsTask::CanCancel),m_work(std::move(work)),m_done(std::move(done)),
      m_progress(std::move(progress)) {}
protected:
  bool run() override {
    m_result=m_work([this]{return isCanceled();},[this](const QString& phase,const QString& detail) {
      // The task manager retains this task while run() executes. Queuing on the
      // task avoids dereferencing a possibly destroyed UI object on this thread.
      QMetaObject::invokeMethod(this,[this,phase,detail] {
        if (m_progress) m_progress(phase,detail);
      },Qt::QueuedConnection);
    });
    return !isCanceled();
  }
  void finished(bool success) override { m_progress={}; m_done(m_result,success); }
private:
  QEventLoopLocker m_quitLock;
  Work m_work;
  std::function<void(const Prepared&,bool)> m_done;
  PreparationProgress m_progress;
  Prepared m_result;
};
}

KaTopographicScopePanel::KaTopographicScopePanel(QgsMapCanvas* canvas, KaTopographicBrowser* browser,
    KaTopographicImportDialog* importer, QWidget* parent, const QString& directory)
    : QWidget(parent), m_canvas(canvas), m_browser(browser), m_importer(importer) {
  setObjectName(QStringLiteral("topographicScopePanel"));
  if (m_importer) {
    m_importer->setAutomaticLoadingEnabled(true);
    m_importer->setMapsEnabled(true);
    m_importLoading=m_importer->isAutomaticLoading();
    connect(m_importer,&KaTopographicImportDialog::automaticLoadingChanged,this,[this](bool loading) {
      m_importLoading=loading; updateProcessing();
    });
    connect(m_importer,&KaTopographicImportDialog::automaticLoadingProgressChanged,this,[this](int loaded,int pending) {
      m_importDetail=QStringLiteral("지도 레이어 %1개 준비 · %2개 대기").arg(loaded).arg(pending);
      updatePreparationProgress();
    });
    connect(m_importer,&KaTopographicImportDialog::automaticLoadingAttention,this,[this](const QString& message) {
      if (m_enabled->isChecked()) setAttention(message);
    });
    connect(m_importer,&QObject::destroyed,this,[this] { m_importLoading=false; updateProcessing(); });
  }
  m_libraryDirectory=directory.isEmpty()?QString():QDir(directory).absolutePath();
  if (browser && !m_libraryDirectory.isEmpty())
    browser->setDownloadRoot(QDir(m_libraryDirectory).filePath(QStringLiteral("원본")));
  auto* layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
  auto* row=new QHBoxLayout;
  m_enabled=new QCheckBox(QStringLiteral("수치지형도 자동 받기"),this); m_enabled->setChecked(true);
  auto* radius=new QLabel(QStringLiteral("%1 km").arg(kRadiusKm,0,'f',0),this); radius->setObjectName(QStringLiteral("topographicRadiusKm"));
  row->addWidget(m_enabled); row->addWidget(new QLabel(QStringLiteral("반경"),this)); row->addWidget(radius);
  m_count=new QLabel(this); m_count->setObjectName(QStringLiteral("topographicSheetCount")); row->addWidget(m_count,1);
  auto* map=new QPushButton(QStringLiteral("지도에서 도엽 경계 보기"),this); row->addWidget(map);
  connect(map,&QPushButton::clicked,this,&KaTopographicScopePanel::mapRequested);
  layout->addLayout(row);
  m_status=new QLabel(this); m_status->setObjectName(QStringLiteral("topographicScopeStatus")); m_status->setWordWrap(true); layout->addWidget(m_status);
  m_scopeTimer=new QTimer(this); m_scopeTimer->setSingleShot(true); m_scopeTimer->setInterval(350);
  connect(m_scopeTimer,&QTimer::timeout,this,&KaTopographicScopePanel::refreshScope);
  connect(m_enabled,&QCheckBox::toggled,this,[this](bool enabled){
    // Canceling a download or changing its destination must preserve maps
    // which have already been imported into the current survey.
    if (m_importer) {
      m_importer->setAutomaticLoadingEnabled(enabled);
      if (enabled) m_importer->setMapsEnabled(true);
    }
    if (!enabled) {
      ++m_generation; m_downloads.clear(); m_restorePending=false; m_scopeTimer->stop();
      m_taskPhase.clear(); m_taskDetail.clear();
      if (m_task) m_task->cancel(); if (m_browser) m_browser->stopAutomatic(); clearBoundaries();
      m_scopeKey.clear(); setStatus(QStringLiteral("자동 받기를 중지했습니다. 보관한 자료는 유지됩니다."));
      updateProcessing();
    } else refreshScope();
  });
  if (canvas) {
    // Only a real pan/zoom can change the 5 km sheet set. A map refresh after
    // importing SHP must not restart the official order.
    connect(canvas,&QgsMapCanvas::extentsChanged,m_scopeTimer,qOverload<>(&QTimer::start));
    connect(canvas,&QObject::destroyed,this,[this]{
      for (auto& boundary:m_boundaries) boundary.release(); // The canvas scene owns deletion during its destruction.
      m_boundaries.clear(); m_canvas.clear(); m_enabled->setChecked(false);
    });
  }
  connect(QgsProject::instance(),&QgsProject::aboutToBeCleared,this,[this]{m_enabled->setChecked(false);m_available.clear();m_reviewMessages.clear();});
  if (browser) {
    connect(browser,&KaTopographicBrowser::automationNeedsInput,this,&KaTopographicScopePanel::setStatus);
    connect(browser,&KaTopographicBrowser::selectionUpdated,this,[this](const QJsonObject& metadata){
      if (!m_canvas || !m_enabled->isChecked()) return;
      const auto items=metadata.value("items").toArray();
      m_count->setProperty("officialCount",items.size()); updateCount();
      clearBoundaries();
      const QgsCoordinateReferenceSystem portal(QStringLiteral("EPSG:5179"));
      for (const auto& item:items) {
        const auto bounds=officialBounds(item.toObject()); if (bounds.isEmpty()) continue;
        auto band=std::make_unique<QgsRubberBand>(m_canvas,Qgis::GeometryType::Polygon);
        band->setColor(QColor(32,137,193,210)); band->setFillColor(QColor(32,137,193,18)); band->setWidth(2);
        band->setToGeometry(QgsGeometry::fromRect(bounds),portal); m_boundaries.push_back(std::move(band));
      }
    });
  }
  QTimer::singleShot(0,this,&KaTopographicScopePanel::refreshScope);
}
KaTopographicScopePanel::~KaTopographicScopePanel() {
  ++m_generation; if (m_task) m_task->cancel(); clearBoundaries();
}
void KaTopographicScopePanel::activate() {
  emit attentionRequired({});
  m_enabled->setChecked(true); refreshScope();
}
void KaTopographicScopePanel::stop() { m_enabled->setChecked(false); }
bool KaTopographicScopePanel::isActive() const { return m_enabled->isChecked(); }
void KaTopographicScopePanel::setStatus(const QString& message) {
  m_status->setText(message); emit statusChanged(message);
}
void KaTopographicScopePanel::setAttention(const QString& message) {
  setStatus(message); emit attentionRequired(message);
}
void KaTopographicScopePanel::updateCount() {
  const int needed=m_count->property("officialCount").isValid()?m_count->property("officialCount").toInt():m_wanted.size();
  m_count->setText(QStringLiteral("필요 도엽 %1장 · 지도 준비 %2장 · 검토 필요 %3장")
    .arg(needed).arg(m_available.size()).arg(m_reviewMessages.size()));
}
void KaTopographicScopePanel::updateProcessing() {
  const bool processing=m_enabled->isChecked() && m_canvas && (m_task || m_restorePending || !m_downloads.isEmpty() || m_importLoading);
  updatePreparationProgress();
  if (processing==m_processing) return;
  m_processing=processing;
  emit processingChanged(processing);
}
void KaTopographicScopePanel::updatePreparationProgress() {
  QString phase,detail;
  if (m_enabled && m_enabled->isChecked() && m_canvas) {
    if (m_task && !m_taskPhase.isEmpty()) { phase=m_taskPhase; detail=m_taskDetail; }
    else if (m_importLoading) {
      phase=QStringLiteral("지도에 올리기");
      detail=m_importDetail.isEmpty()?QStringLiteral("수치지형도 레이어를 불러오고 있습니다."):m_importDetail;
    }
  }
  if (phase==m_reportedPhase && detail==m_reportedDetail) return;
  m_reportedPhase=phase; m_reportedDetail=detail;
  emit preparationProgressChanged(phase,detail);
}
void KaTopographicScopePanel::setLibraryDirectory(const QString& directory) {
  const QString path=directory.isEmpty()?QString():QDir(directory).absolutePath();
  if (path==m_libraryDirectory) return;
  m_enabled->setChecked(false);
  m_available.clear(); m_reviewMessages.clear(); m_libraryDirectory=path;
  if (m_browser && !path.isEmpty()) m_browser->setDownloadRoot(QDir(path).filePath(QStringLiteral("원본")));
}
void KaTopographicScopePanel::clearBoundaries() { m_boundaries.clear(); }
void KaTopographicScopePanel::refreshScope() {
  if (!m_canvas || !m_enabled->isChecked()) return;
  if (m_libraryDirectory.isEmpty()) {
    m_enabled->setChecked(false);
    setAttention(QStringLiteral("새 조사를 만들거나 조사를 열어 지형도 저장 폴더를 지정하세요.")); return;
  }
  auto* project=QgsProject::instance();
  const auto work=m_canvas->mapSettings().destinationCrs();
  const QgsPointXY center=m_canvas->extent().center();
  try {
    const auto selected=TopographicSheets::select(center,work,kRadiusKm,project->transformContext());
    if (!selected.error.isEmpty()) {
      m_enabled->setChecked(false);
      setAttention(selected.error);
      return;
    }
    QStringList wanted;
    for (const auto& sheet:selected.sheets) wanted.append(sheet.number);
    wanted.sort();
    const QString key=QStringLiteral("%1|%2").arg(work.authid(), wanted.join(QLatin1Char(',')));
    if (key==m_scopeKey) return;
    QgsCoordinateTransform toPortal(work,QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),project);
    toPortal.setAllowFallbackTransforms(false); toPortal.setBallparkTransformsAreAppropriate(false);
    const auto portal=toPortal.transform(center); m_easting5179=portal.x(); m_northing5179=portal.y();
    m_scopeKey=key; m_searchArea=selected.searchArea; m_wanted=wanted; clearBoundaries();
    auto circle=std::make_unique<QgsRubberBand>(m_canvas,Qgis::GeometryType::Polygon);
    circle->setColor(QColor(32,137,193,210)); circle->setFillColor(QColor(32,137,193,15)); circle->setWidth(2);
    circle->setLineStyle(Qt::DashLine); circle->setToGeometry(m_searchArea,work); m_boundaries.push_back(std::move(circle));
    m_count->setProperty("candidateCount",m_wanted.size());
    m_count->setProperty("officialCount",QVariant());
    m_count->setText(QStringLiteral("예상 도엽 %1장 · 공식 반경 검색으로 확인합니다").arg(m_wanted.size()));
    m_count->setToolTip(m_wanted.join(QStringLiteral(", ")));
    setStatus(QStringLiteral("현재 화면 중심 반경 %1km의 보관 자료와 공식 도엽을 확인하고 있습니다.").arg(kRadiusKm,0,'f',0));
    m_restorePending=true; startNext();
  } catch (const QgsCsException&) {
    m_enabled->setChecked(false);
    setAttention(QStringLiteral("현재 위치를 수치지형도 검색 좌표로 변환하지 못했습니다."));
  }
}
void KaTopographicScopePanel::acceptDownload(const QString& path,const QJsonObject& metadata) {
  if (!m_enabled->isChecked()) return;
  for (const auto& queued:m_downloads) if (queued.first==path) return;
  m_downloads.enqueue({path,publicMetadata(metadata)}); startNext();
}
void KaTopographicScopePanel::startNext() {
  updateProcessing();
  if (m_task || !m_enabled->isChecked() || !m_canvas) return;
  const bool download=!m_downloads.isEmpty();
  if (!download && !m_restorePending) return;
  const auto generation=m_generation;
  const auto serial=++m_taskSerial;
  const auto context=QgsProject::instance()->transformContext();
  const auto library=m_libraryDirectory;
  LibraryTask::Work work;
  if (download) {
    const auto item=m_downloads.dequeue();
    work=[item,library,context](const auto& canceled,const auto& progress){
      auto prepared=prepareOne(item.first,item.second,library,context,canceled,true,progress);
      const auto label=sheetLabel(item.second);
      if (!label.isEmpty() && !prepared.error.isEmpty() && !prepared.error.startsWith(label))
        prepared.error=label+QStringLiteral(" · ")+prepared.error;
      return prepared;
    };
    setStatus(QStringLiteral("받은 자료 압축 해제 · 좌표 확인 · SHP 변환 중입니다…"));
  } else {
    m_restorePending=false;
    const QgsPointXY center(m_easting5179,m_northing5179); const double radius=kRadiusKm*1000.;
    work=[library,center,radius,context](const auto& canceled,const auto& progress){return restoreNearby(library,center,radius,context,canceled,progress);};
  }
  const QPointer<KaTopographicScopePanel> guard(this);
  auto* task=new LibraryTask(std::move(work),[guard,generation,download](const Prepared& prepared,bool success){
    if (!guard) return;
    guard->m_task.clear();
    guard->m_taskPhase.clear(); guard->m_taskDetail.clear();
    if (guard->m_generation!=generation || !success) { guard->startNext(); return; }
    if (!download) guard->m_reviewMessages=prepared.reviewMessages;
    else for (auto it=prepared.reviewMessages.cbegin();it!=prepared.reviewMessages.cend();++it)
      guard->m_reviewMessages.insert(it.key(),it.value());
    QString error=prepared.error;
    if (guard->m_importer && !prepared.reviewRecords.isEmpty())
      guard->m_importer->retainForReview(prepared.reviewRecords,
        error.isEmpty()?QStringLiteral("도곽 밖이거나 좌표 범위를 확인하지 못했습니다. 미리보기 후 추가하세요."):error);
    // 도엽 하나가 「검토 필요」라고 나머지까지 버리지 않는다. 예전에는 error 가
    // 비어 있을 때만 importVerified 를 불러서, 도곽만 있는 도엽 한 장 때문에
    // 검증에 성공한 다른 도엽들도 지도에 올라가지 못하고 멈췄다.
    // 검증된 것은 올리고, 문제가 있는 도엽만 「확인 필요」로 알린다.
    const QString sheetReview=error;
    if (!prepared.records.isEmpty() && guard->m_importer) {
      QString importError;
      guard->m_importer->importVerified(prepared.records,&importError);
      if (!importError.isEmpty())
        error=importError;  // 적재 자체가 실패한 것은 진짜 오류다
    }
    if (error.isEmpty() || (error==sheetReview && !prepared.records.isEmpty())) {
      if (!download) guard->m_available=prepared.available;
      else for (const auto& number:prepared.available) if (!guard->m_available.contains(number)) guard->m_available.append(number);
      for (const auto& number:guard->m_available) guard->m_reviewMessages.remove(number);
      if (!prepared.records.isEmpty()) {
        guard->setStatus(QStringLiteral("도엽 %1장 보관. 화면 중심 반경 10km에 겹치는 SHP %2개를 지도에 올립니다 · %3")
          .arg(prepared.available.size()).arg(prepared.records.size()).arg(prepared.crsNames.join(QStringLiteral(" / "))));
        guard->m_status->setToolTip(prepared.evidence);
      }
      QStringList warnings=prepared.warnings;
      warnings.append(guard->m_reviewMessages.values()); warnings.removeDuplicates();
      if (!warnings.isEmpty()) {
        guard->setStatus(guard->m_status->text()+QStringLiteral(" 확인이 필요한 항목 %1개.").arg(warnings.size()));
        guard->m_status->setToolTip(prepared.evidence+QLatin1Char('\n')+warnings.join(QLatin1Char('\n')));
        emit guard->attentionRequired(warnings.join(QLatin1Char('\n')));
      }
      // 검토가 필요한 도엽이 섞여 있었으면 그 사실도 함께 알린다.
      if (!sheetReview.isEmpty()) guard->setAttention(sheetReview);
      if (!download && !guard->m_restorePending) guard->refreshBrowser();
    } else {
      QStringList warnings=guard->m_reviewMessages.values(); warnings.append(error); warnings.removeDuplicates();
      guard->setAttention(warnings.join(QLatin1Char('\n')));
    }
    guard->updateCount();
    if (download) emit guard->importFinished(error.isEmpty() && !prepared.records.isEmpty());
    guard->startNext();
  },[guard,generation,serial](const QString& phase,const QString& detail) {
    if (!guard || !guard->m_enabled->isChecked() || guard->m_generation!=generation ||
        guard->m_taskSerial!=serial || !guard->m_task) return;
    guard->m_taskPhase=phase; guard->m_taskDetail=detail; guard->updatePreparationProgress();
  });
  m_task=task; QgsApplication::taskManager()->addTask(task);
}
void KaTopographicScopePanel::refreshBrowser() {
  if (m_browser && m_enabled->isChecked()) {
    auto received=m_available; received.append(m_reviewMessages.keys()); received.removeDuplicates();
    m_browser->prepareSheets(m_easting5179,m_northing5179,kRadiusKm*1000.,m_wanted,received);
  }
}
