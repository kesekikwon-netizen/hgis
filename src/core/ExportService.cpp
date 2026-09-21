#include "ExportService.h"
#include "LayoutService.h"
#include "SectionLayoutService.h"
#include "LayerOps.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDateTime>
#include <QStringConverter>
#include <QCryptographicHash>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsprintlayout.h>
#include <qgslayoutitemmap.h>
#include <QMap>
#include <QSet>
#include <qgsvectorfilewriter.h>
#include <qgsfeatureiterator.h>
#include <qgsfield.h>
#include <qgsgeometry.h>
#include <qgsvectordataprovider.h>
#include <qgswkbtypes.h>
#include <memory>
#include <qgscoordinatetransformcontext.h>
#include <qgslayoutmanager.h>

static bool isSectionSheetComposed(QgsProject* project) {
  if (!project || !project->layoutManager())
    return false;
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(QStringLiteral("section_sheet")));
  if (!ly || ly->itemById(QStringLiteral("empty_hint")))
    return false;
  return LayoutService::isComposedStudioSheet(project, QStringLiteral("section_sheet"));
}

// DBF field names are 10 bytes. These two domain names are 11 characters.
// The aliases are this program's SHP names, not an agency field dictionary.
// https://gdal.org/en/latest/drivers/vector/shapefile.html
static QString shapefileFieldName(const QString& name, QSet<QString>& used) {
  QString mapped = name;
  if (name.toUtf8().size() > 10) {
    if (name == QLatin1String("survey_name")) mapped = QStringLiteral("surv_name");
    else if (name == QLatin1String("artifact_no")) mapped = QStringLiteral("artif_no");
    else mapped = QString::fromUtf8(name.toUtf8().left(10));
  }
  QString candidate = mapped;
  int serial = 2;
  while (used.contains(candidate) || candidate.isEmpty() || candidate.toUtf8().size() > 10) {
    const QString suffix = QString::number(serial++);
    candidate = QString::fromUtf8(mapped.toUtf8().left(qMax(1, 10 - suffix.size()))) + suffix;
    if (serial > 99) break;
  }
  used.insert(candidate);
  return candidate;
}

static std::unique_ptr<QgsVectorLayer> shapefileNamedLayer(QgsVectorLayer* source, const QString& layerKey,
                                                          QStringList* notes, QString* errorOut) {
  if (!source) return nullptr;
  bool needsAlias = false;
  for (const QgsField& field : source->fields()) {
    if (field.name().toUtf8().size() > 10) needsAlias = true;
  }
  if (!needsAlias) return nullptr;
  QSet<QString> used;
  QgsFields fields;
  QStringList fromNames;
  for (const QgsField& field : source->fields()) {
    const QString mapped = shapefileFieldName(field.name(), used);
    if (mapped != field.name() && notes)
      notes->append(QStringLiteral("%1 %2=%3").arg(layerKey, field.name(), mapped));
    QgsField out(mapped, field.type());
    out.setLength(field.length());
    out.setPrecision(field.precision());
    fields.append(out);
    fromNames.append(field.name());
  }
  auto copy = std::make_unique<QgsVectorLayer>(
      QStringLiteral("%1?crs=%2").arg(QgsWkbTypes::displayString(source->wkbType()),
                                      source->crs().isValid() ? source->crs().authid() : QStringLiteral("EPSG:5187")),
      layerKey, QStringLiteral("memory"));
  if (!copy->isValid() || !copy->dataProvider()->addAttributes(fields.toList())) {
    if (errorOut) *errorOut = QStringLiteral("%1 SHP 필드명을 만들지 못했습니다.").arg(layerKey);
    return nullptr;
  }
  copy->updateFields();
  QgsFeatureIterator it = source->getFeatures();
  QgsFeature sourceFeature;
  QgsFeatureList batch;
  while (it.nextFeature(sourceFeature)) {
    QgsFeature feature(copy->fields());
    for (int i = 0; i < fromNames.size(); ++i) {
      const int index = sourceFeature.fields().indexOf(fromNames.at(i));
      if (index >= 0) feature.setAttribute(i, sourceFeature.attribute(index));
    }
    feature.setGeometry(sourceFeature.geometry());
    batch.append(feature);
  }
  if (!batch.isEmpty() && !copy->dataProvider()->addFeatures(batch)) {
    if (errorOut) *errorOut = QStringLiteral("%1 SHP 속성을 복사하지 못했습니다.").arg(layerKey);
    return nullptr;
  }
  copy->updateExtents();
  return copy;
}

bool ExportService::writeSha256Manifest(const QString& dir, QString* errorOut) {
  if (errorOut) errorOut->clear();
  QDir d(dir);
  if (!d.exists()) {
    if (errorOut) *errorOut = QStringLiteral("dir missing");
    return false;
  }
  // Enumerate before opening QSaveFile: its temporary file is not package data.
  const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
  const QString manPath = d.filePath(QStringLiteral("MANIFEST.sha256"));
  QSaveFile man(manPath);
  if (!man.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("cannot write manifest");
    return false;
  }
  QTextStream ts(&man);
  ts.setEncoding(QStringConverter::Utf8);

  constexpr qint64 kChunkSize = 64 * 1024; // 64 KB chunk
  QByteArray buffer(kChunkSize, Qt::Uninitialized);
  char* dataPtr = buffer.data();

  for (const QFileInfo& fi : files) {
    if (fi.fileName() == QLatin1String("MANIFEST.sha256")) continue;
    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
      if (errorOut) *errorOut = QStringLiteral("Cannot open file for hashing: %1").arg(fi.fileName());
      return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
      const qint64 bytesRead = f.read(dataPtr, kChunkSize);
      if (bytesRead < 0) {
        if (errorOut) *errorOut = QStringLiteral("Read error while hashing: %1").arg(fi.fileName());
        return false;
      }
      if (bytesRead == 0) break;
      hash.addData(QByteArrayView(dataPtr, static_cast<qsizetype>(bytesRead)));
    }
    f.close();
    const QByteArray hexDigest = hash.result().toHex();
    ts << QString::fromLatin1(hexDigest) << "  " << fi.fileName() << "\n";
  }
  ts.flush();
  if (ts.status() != QTextStream::Ok || !man.commit()) {
    if (errorOut) *errorOut = QStringLiteral("MANIFEST.sha256 저장 실패: %1").arg(man.errorString());
    return false;
  }
  return true;
}

QString ExportService::writePdfViaLayout(QgsProject* project, const QString& layoutName,
                                         const QString& outPath, QString* errorOut) {
  return LayoutService::exportLayoutPdf(project, layoutName, outPath, errorOut);
}

QString ExportService::exportSubmissionPackage(QgsProject* project,
                                               const QString& outDir,
                                               const QString& encoding,
                                               const QString& checklistSummary,
                                               bool blockOnError,
                                               bool hasChecklistErrors,
                                               QString* errorOut) {
  if (errorOut) errorOut->clear();
  if (blockOnError && hasChecklistErrors) {
    if (errorOut) *errorOut = QStringLiteral("Checklist errors remain; export blocked.");
    return {};
  }
  const QFileInfo destination(outDir);
  const QString finalPath = destination.absoluteFilePath();
  const bool existed = destination.exists();
  const auto hasEntries = [](const QString& path) {
    return !QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System |
                               QDir::NoDotAndDotDot).isEmpty();
  };
  if (outDir.trimmed().isEmpty() || destination.isSymLink() ||
      (existed && (!destination.isDir() || hasEntries(finalPath)))) {
    if (errorOut) *errorOut = QStringLiteral("제출 폴더가 비어 있지 않거나 사용할 수 없습니다. 새 폴더를 선택하세요: %1")
                                .arg(QDir::toNativeSeparators(finalPath));
    return {};
  }
  QDir parent(destination.absolutePath());
  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    if (errorOut) *errorOut = QStringLiteral("제출 위치를 만들 수 없습니다.");
    return {};
  }
  // A sibling keeps finalization on the same filesystem. Failure removes only
  // this operation's staging directory, never a previous submission.
  QTemporaryDir staging(parent.filePath(QStringLiteral(".ka-hgis-export-XXXXXX")));
  if (!staging.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("제출 임시 폴더를 만들 수 없습니다: %1").arg(staging.errorString());
    return {};
  }
  QDir dir(staging.path());

  if (project &&
      !LayoutService::isComposedStudioSheet(project, QStringLiteral("user_sheet"))) {
    if (errorOut)
      *errorOut = QStringLiteral("도면만들기에서 용지를 만든 뒤 다시 보내기 하세요.");
    return {};
  }

  const QString enc = (encoding.compare(QStringLiteral("EUC-KR"), Qt::CaseInsensitive) == 0
                       || encoding.compare(QStringLiteral("CP949"), Qt::CaseInsensitive) == 0)
                          ? QStringLiteral("CP949")
                          : QStringLiteral("UTF-8");
  QStringList shpFieldNotes;

  if (project) {
    const QStringList names = {
      QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
      QStringLiteral("feature_line"), QStringLiteral("control_points"),
      QStringLiteral("section_line"), QStringLiteral("artifact_point"),
      QStringLiteral("trial_trench")
    };
    const QgsCoordinateReferenceSystem epsg5179(QStringLiteral("EPSG:5179"));
    for (const QString& n : names) {
      // 조사구역은 사용자가 레이어를 여러 개 만들 수 있다. 같은 키를 가진 레이어를
      // 모두 모아 하나의 SHP 로 쓴다. 하나만 내보내면 제출물에서 구역이 빠진다.
      QList<QgsVectorLayer*> sources;
      for (QgsVectorLayer* vl : LayerOps::domainLayersForKey(project, n)) {
        if (vl->featureCount() > 0) sources.append(vl);
      }
      if (sources.isEmpty()) continue;

      QgsVectorLayer* primary = sources.first();
      // 레이어가 둘 이상이면 메모리 레이어에 합친다. SHP 덧붙이기는 드라이버에
      // 따라 기존 파일을 덮어써 도형이 사라질 수 있으므로 쓰지 않는다.
      std::unique_ptr<QgsVectorLayer> merged;
      if (sources.size() > 1) {
        merged.reset(new QgsVectorLayer(
            QStringLiteral("%1?crs=%2")
                .arg(QgsWkbTypes::displayString(primary->wkbType()), primary->crs().authid()),
            n, QStringLiteral("memory")));
        if (!merged || !merged->isValid()) {
          if (errorOut) *errorOut = QStringLiteral("%1 레이어를 합치지 못했습니다.").arg(n);
          return {};
        }
        if (!merged->dataProvider()->addAttributes(primary->fields().toList())) {
          if (errorOut) *errorOut = QStringLiteral("%1 속성 구성을 만들지 못했습니다.").arg(n);
          return {};
        }
        merged->updateFields();
        for (QgsVectorLayer* vl : sources) {
          QgsCoordinateTransform toPrimary;
          if (vl->crs().isValid() && primary->crs().isValid() && vl->crs() != primary->crs())
            toPrimary = QgsCoordinateTransform(vl->crs(), primary->crs(), project->transformContext());
          QgsFeatureList batch;
          QgsFeatureIterator it = vl->getFeatures();
          QgsFeature source;
          while (it.nextFeature(source)) {
            QgsFeature copy(merged->fields());
            // 필드는 이름으로 맞춘다. 레이어마다 속성 순서가 다를 수 있다.
            for (const QgsField& field : merged->fields()) {
              const int index = source.fields().indexOf(field.name());
              if (index >= 0) copy.setAttribute(field.name(), source.attribute(index));
            }
            QgsGeometry geometry = source.geometry();
            if (toPrimary.isValid() && !geometry.isNull()) {
              QgsGeometry reprojected = geometry;
              if (reprojected.transform(toPrimary) != Qgis::GeometryOperationResult::Success) {
                if (errorOut) *errorOut = QStringLiteral("%1 좌표 변환에 실패했습니다.").arg(n);
                return {};
              }
              geometry = reprojected;
            }
            copy.setGeometry(geometry);
            batch.append(copy);
          }
          if (!batch.isEmpty() && !merged->dataProvider()->addFeatures(batch)) {
            if (errorOut) *errorOut = QStringLiteral("%1 도형을 합치지 못했습니다.").arg(n);
            return {};
          }
        }
        merged->updateExtents();
      }

      QgsVectorLayer* out = merged ? merged.get() : primary;
      QString aliasError;
      std::unique_ptr<QgsVectorLayer> aliased = shapefileNamedLayer(out, n, &shpFieldNotes, &aliasError);
      if (!aliasError.isEmpty()) {
        if (errorOut) *errorOut = aliasError;
        return {};
      }
      if (aliased) out = aliased.get();
      const QString shp = dir.filePath(n + QStringLiteral(".shp"));
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("ESRI Shapefile");
      opts.fileEncoding = enc;
      if (out->crs().isValid() && out->crs() != epsg5179) {
        opts.ct = QgsCoordinateTransform(out->crs(), epsg5179, project->transformContext());
      }
      QString errMsg, newFn, newLayer;
      const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
          out, shp, project->transformContext(), opts, &errMsg, &newFn, &newLayer);
      if (we != QgsVectorFileWriter::NoError) {
        if (errorOut) *errorOut = errMsg.isEmpty() ? QStringLiteral("SHP failed: %1").arg(n) : errMsg;
        return {};
      }
    }
  }

  QSaveFile encf(dir.filePath(QStringLiteral("encoding.txt")));
  const QByteArray encodingBytes = enc.toUtf8() + '\n';
  if (!encf.open(QIODevice::WriteOnly) || encf.write(encodingBytes) != encodingBytes.size() ||
      !encf.commit()) {
    if (errorOut) *errorOut = QStringLiteral("encoding.txt 저장 실패: %1").arg(encf.errorString());
    return {};
  }

  // 1. 프로젝트가 있으면 조판된 user_sheet만 조사도면.pdf 로 넣는다.
  if (project) {
    QString pdfErr;
    const QString pdfResult = LayoutService::exportLayoutPdf(
        project, QStringLiteral("user_sheet"),
        dir.filePath(QStringLiteral("조사도면.pdf")), &pdfErr);
    if (pdfResult.isEmpty()) {
      if (errorOut) {
        *errorOut = pdfErr.isEmpty()
            ? QStringLiteral("조사도면.pdf 내보내기 실패")
            : QStringLiteral("조사도면.pdf 내보내기 실패: %1").arg(pdfErr);
      }
      return {};
    }
  }

  // 2. Export section_sheet as 단면도.pdf if present and composed
  const bool hasSectionSheet = isSectionSheetComposed(project);
  if (hasSectionSheet) {
    QString secErr;
    const QString secResult = SectionLayoutService::exportSectionPdf(
        project, dir.filePath(QStringLiteral("단면도.pdf")), &secErr);
    if (secResult.isEmpty()) {
      if (errorOut) {
        *errorOut = secErr.isEmpty()
            ? QStringLiteral("단면도.pdf 내보내기 실패")
            : QStringLiteral("단면도.pdf 내보내기 실패: %1").arg(secErr);
      }
      return {};
    }
  }

  // 3. Write README_submit.txt after PDF export so file existence is reported accurately
  const QString readme = dir.filePath(QStringLiteral("README_submit.txt"));
  QSaveFile f(readme);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write README");
    return {};
  }
  QTextStream ts(&f);
  ts.setEncoding(QStringConverter::Utf8);
  ts << "KA-HGIS submission package\n";
  ts << "created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  ts << "crs: EPSG:5179\n";
  ts << "shp_encoding: " << enc << "\n";
  ts << "intranet: upload each domain SHP (feature_poly.shp = one file; merge polygons in-app first if required)\n\n";
  ts << "checklist:\n" << checklistSummary << "\n";
  if (!shpFieldNotes.isEmpty()) {
    ts << "\nshp_field_names:\n";
    for (const QString& note : shpFieldNotes)
      ts << note << "\n";
  }
  ts << "\nSee MANIFEST.sha256 for file hashes.\n";
  ts << QStringLiteral("도면 PDF는 도면만들기 용지(user_sheet 및 section_sheet)를 넣습니다.\n");
  if (QFile::exists(dir.filePath(QStringLiteral("조사도면.pdf")))) {
    ts << QStringLiteral("- 조사도면.pdf (도면만들기 user_sheet)\n");
  } else {
    ts << QStringLiteral("조사도면.pdf 없음: 도면만들기에서 용지를 만든 뒤 다시 보내기 하세요.\n");
  }
  if (QFile::exists(dir.filePath(QStringLiteral("단면도.pdf")))) {
    ts << QStringLiteral("- 단면도.pdf (단면도면만들기 section_sheet)\n");
  }
  ts.flush();
  if (ts.status() != QTextStream::Ok || !f.commit()) {
    if (errorOut) *errorOut = QStringLiteral("README_submit.txt 저장 실패: %1").arg(f.errorString());
    return {};
  }

  QString merr;
  if (!writeSha256Manifest(staging.path(), &merr)) {
    if (errorOut) *errorOut = merr;
    return {};
  }
  const QFileInfo current(finalPath);
  // Recheck after PDF rendering, which can dispatch events. Never replace a
  // directory that appeared while the package was being generated.
  if (current.isSymLink() || (!existed && current.exists()) ||
      (existed && (!current.isDir() || hasEntries(finalPath) || !parent.rmdir(destination.fileName())))) {
    if (errorOut) *errorOut = QStringLiteral("제출 중 결과 폴더가 변경되었거나 잠겨 있습니다. 새 폴더로 다시 시도하세요.");
    return {};
  }
  if (!parent.rename(QFileInfo(staging.path()).fileName(), destination.fileName())) {
    if (errorOut) *errorOut = QStringLiteral("제출 결과 폴더를 확정하지 못했습니다. 쓰기 권한과 다른 프로그램의 사용 여부를 확인하세요.");
    return {};
  }
  staging.setAutoRemove(false);
  return outDir;
}
