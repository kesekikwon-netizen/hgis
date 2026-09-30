#include "ExportService.h"
#include "LayerOps.h"
#include "LayoutService.h"
#include "SectionLayoutService.h"
#include "SubmitReadme.h"
#include "SubmitShp.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QTextStream>
#include <memory>
#include <qgscoordinatetransform.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgslayoutmanager.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>

static bool isSectionSheetComposed(QgsProject* project) {
  if (!project || !project->layoutManager())
    return false;
  // Section display rasters are layout-owned temp files (not saved with the
  // survey); derive them again after a reopen before judging the sheet.
  SectionLayoutService::restoreDisplayLayers(project);
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      project->layoutManager()->layoutByName(QStringLiteral("section_sheet")));
  if (!ly || ly->itemById(QStringLiteral("empty_hint")))
    return false;
  return LayoutService::isComposedStudioSheet(project, QStringLiteral("section_sheet"));
}

bool ExportService::writeSha256Manifest(const QString& dir, QString* errorOut) {
  if (errorOut) errorOut->clear();
  QDir d(dir);
  if (!d.exists()) {
    if (errorOut)
      *errorOut = QStringLiteral("해시 목록을 만들 폴더가 없습니다: %1. 폴더가 지워졌거나 옮겨졌는지 확인하세요.")
                      .arg(QDir::toNativeSeparators(dir));
    return false;
  }
  // Enumerate before opening QSaveFile: its temporary file is not package data.
  const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
  QSaveFile man(d.filePath(QStringLiteral("MANIFEST.sha256")));
  if (!man.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut)
      *errorOut = QStringLiteral("MANIFEST.sha256을 쓸 수 없습니다(%1). 폴더 쓰기 권한과 남은 공간을 확인하세요.")
                      .arg(man.errorString());
    return false;
  }
  QTextStream ts(&man);
  ts.setEncoding(QStringConverter::Utf8);
  constexpr qint64 kChunkSize = 64 * 1024;
  QByteArray buffer(kChunkSize, Qt::Uninitialized);
  char* dataPtr = buffer.data();
  for (const QFileInfo& fi : files) {
    if (fi.fileName() == QLatin1String("MANIFEST.sha256")) continue;
    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
      if (errorOut)
        *errorOut = QStringLiteral("해시를 계산하려고 %1을(를) 열지 못했습니다. 다른 프로그램이 이 파일을 쓰고 있는지 확인하세요.")
                        .arg(fi.fileName());
      return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
      const qint64 bytesRead = f.read(dataPtr, kChunkSize);
      if (bytesRead < 0) {
        if (errorOut) *errorOut = QStringLiteral("해시를 계산하다 %1 읽기에 실패했습니다. 저장 장치를 확인하세요.").arg(fi.fileName());
        return false;
      }
      if (bytesRead == 0) break;
      hash.addData(QByteArrayView(dataPtr, static_cast<qsizetype>(bytesRead)));
    }
    f.close();
    ts << QString::fromLatin1(hash.result().toHex()) << "  " << fi.fileName() << "\n";
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

QString ExportService::exportSubmissionPackage(QgsProject* project, const QString& outDir,
                                               const QString& encoding, const QString& checklistSummary,
                                               bool blockOnError, bool hasChecklistErrors,
                                               QString* errorOut, const SubmitPackageInfo& info) {
  if (errorOut) errorOut->clear();
  const auto fail = [errorOut](const QString& message) {
    if (errorOut) *errorOut = message;
    return QString();
  };
  if (blockOnError && hasChecklistErrors)
    return fail(QStringLiteral("검수 오류가 남아 있어 제출 꾸러미를 만들지 않았습니다. "
                               "「검수·제출」에서 오류 항목을 고친 뒤 다시 만드세요."));
  const QFileInfo destination(outDir);
  const QString finalPath = destination.absoluteFilePath();
  const bool existed = destination.exists();
  const auto hasEntries = [](const QString& path) {
    return !QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty();
  };
  if (outDir.trimmed().isEmpty() || destination.isSymLink() ||
      (existed && (!destination.isDir() || hasEntries(finalPath))))
    return fail(QStringLiteral("제출 폴더가 비어 있지 않거나 사용할 수 없습니다. 새 폴더를 선택하세요: %1")
                    .arg(QDir::toNativeSeparators(finalPath)));
  QDir parent(destination.absolutePath());
  if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
    return fail(QStringLiteral("제출 위치를 만들 수 없습니다. 쓰기 권한이 있는 다른 폴더를 고르세요: %1")
                    .arg(QDir::toNativeSeparators(parent.absolutePath())));
  // A sibling keeps finalization on the same filesystem. Failure removes only
  // this operation's staging directory, never a previous submission.
  QTemporaryDir staging(parent.filePath(QStringLiteral(".ka-hgis-export-XXXXXX")));
  if (!staging.isValid())
    return fail(QStringLiteral("제출 임시 폴더를 만들 수 없습니다: %1").arg(staging.errorString()));
  QDir dir(staging.path());

  if (project && !LayoutService::isComposedStudioSheet(project, QStringLiteral("user_sheet")))
    return fail(QStringLiteral("도면만들기에서 용지를 만든 뒤 다시 보내기 하세요."));

  const QString enc = (encoding.compare(QStringLiteral("EUC-KR"), Qt::CaseInsensitive) == 0 ||
                       encoding.compare(QStringLiteral("CP949"), Qt::CaseInsensitive) == 0)
                          ? QStringLiteral("CP949")
                          : QStringLiteral("UTF-8");
  const QStringList keys = {QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
                            QStringLiteral("feature_line"), QStringLiteral("control_points"),
                            QStringLiteral("section_line"), QStringLiteral("artifact_point"),
                            QStringLiteral("trial_trench")};
  int done = 0;
  const int total = (project ? keys.size() + 2 : 0) + 4;
  const auto step = [&](const QString& label) {
    if (info.progress && !info.progress(done, total, label)) return false;
    ++done;
    return true;
  };
  const QString canceled = QStringLiteral("제출 꾸러미 만들기를 취소했습니다. 이전 결과는 그대로입니다.");
  SubmitReadme::Inputs readme;
  readme.encoding = enc;
  readme.checklistSummary = checklistSummary;
  readme.surveyName = info.surveyName;
  readme.unsavedEditsIncluded = info.unsavedEditsIncluded;

  if (project) {
    const QgsCoordinateReferenceSystem epsg5179(QStringLiteral("EPSG:5179"));
    for (const QString& n : keys) {
      if (!step(QStringLiteral("%1.shp").arg(n))) return fail(canceled);
      // 같은 키의 레이어를 모두 모아 하나의 SHP 로 쓴다. 하나만 내보내면 구역이 빠진다.
      QList<QgsVectorLayer*> sources;
      for (QgsVectorLayer* vl : LayerOps::domainLayersForKey(project, n)) {
        if (vl->featureCount() <= 0) continue;
        // 좌표계가 없으면 5179로 바꿀 수 없다. 그대로 쓰고 README에 5179라고 적지 않는다.
        if (!vl->crs().isValid())
          return fail(QStringLiteral("「%1」(%2) 레이어에 좌표계가 없어 EPSG:5179로 바꿀 수 없습니다. "
                                     "레이어 좌표계를 지정한 뒤 다시 만드세요.").arg(vl->name(), n));
        sources.append(vl);
      }
      if (sources.isEmpty()) continue;
      QString layerError;
      // SHP 덧붙이기는 드라이버에 따라 기존 파일을 덮어써 도형이 사라질 수 있어 메모리에서 합친다.
      std::unique_ptr<QgsVectorLayer> merged;
      if (sources.size() > 1) {
        merged = SubmitShp::mergeLayers(sources, n, project, &readme.fieldNotes, &layerError);
        if (!merged) return fail(layerError);
      }
      QgsVectorLayer* out = merged ? merged.get() : sources.first();
      std::unique_ptr<QgsVectorLayer> aliased = SubmitShp::withShapefileFieldNames(out, n, &readme.fieldNotes, &layerError);
      if (!layerError.isEmpty()) return fail(layerError);
      if (aliased) out = aliased.get();
      const QString shp = dir.filePath(n + QStringLiteral(".shp"));
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("ESRI Shapefile");
      opts.fileEncoding = enc;
      if (out->crs() != epsg5179)
        opts.ct = QgsCoordinateTransform(out->crs(), epsg5179, project->transformContext());
      QString errMsg, newFn, newLayer;
      const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(out, shp, project->transformContext(), opts,
                                                                 &errMsg, &newFn, &newLayer);
      if (we != QgsVectorFileWriter::NoError)
        return fail(QStringLiteral("%1.shp를 쓰지 못했습니다. 저장 위치의 쓰기 권한과 남은 공간을 확인하세요.%2")
                        .arg(n, errMsg.isEmpty() ? QString() : QStringLiteral("\n") + errMsg));
      // 다시 열어 좌표계와 도형 수가 맞는지 확인한다. 조용히 틀린 파일을 내지 않는다.
      long long expected = static_cast<long long>(out->featureCount());
      if (expected < 0) {
        expected = 0;
        QgsFeatureIterator it = out->getFeatures(QgsFeatureRequest().setNoAttributes().setFlags(Qgis::FeatureRequestFlag::NoGeometry));
        QgsFeature counted;
        while (it.nextFeature(counted)) ++expected;
      }
      if (!SubmitShp::verifyWritten(shp, expected, &layerError)) return fail(layerError);
      readme.layerLines << QStringLiteral("%1.shp features=%2 crs=EPSG:5179 verified").arg(n).arg(expected);
    }
  }

  if (!step(QStringLiteral("encoding.txt"))) return fail(canceled);
  QSaveFile encf(dir.filePath(QStringLiteral("encoding.txt")));
  const QByteArray encodingBytes = enc.toUtf8() + '\n';
  if (!encf.open(QIODevice::WriteOnly) || encf.write(encodingBytes) != encodingBytes.size() || !encf.commit())
    return fail(QStringLiteral("encoding.txt 저장 실패: %1").arg(encf.errorString()));

  // 1. 프로젝트가 있으면 조판된 user_sheet만 조사도면.pdf 로 넣는다.
  if (project) {
    if (!step(QStringLiteral("조사도면.pdf"))) return fail(canceled);
    QString pdfErr;
    if (LayoutService::exportLayoutPdf(project, QStringLiteral("user_sheet"),
                                       dir.filePath(QStringLiteral("조사도면.pdf")), &pdfErr).isEmpty())
      return fail(pdfErr.isEmpty() ? QStringLiteral("조사도면.pdf 내보내기 실패")
                                   : QStringLiteral("조사도면.pdf 내보내기 실패: %1").arg(pdfErr));
    // 2. section_sheet 가 조판돼 있으면 단면도.pdf 로 넣는다.
    if (!step(QStringLiteral("단면도.pdf"))) return fail(canceled);
    if (isSectionSheetComposed(project)) {
      QString secErr;
      if (SectionLayoutService::exportSectionPdf(project, dir.filePath(QStringLiteral("단면도.pdf")), &secErr).isEmpty())
        return fail(secErr.isEmpty() ? QStringLiteral("단면도.pdf 내보내기 실패")
                                     : QStringLiteral("단면도.pdf 내보내기 실패: %1").arg(secErr));
    }
    readme.referenceLines = SubmitReadme::referenceLines(
        project, {QStringLiteral("user_sheet"), QStringLiteral("section_sheet")});
  }

  // 3. README 는 PDF 뒤에 써서 파일 유무를 정확히 적는다.
  if (!step(QStringLiteral("조사 파일 지문"))) return fail(canceled);
  if (!info.surveyPath.isEmpty() && QFileInfo(info.surveyPath).isFile()) {
    readme.surveyFile = QFileInfo(info.surveyPath).fileName();
    QString hashError;
    readme.surveySha256 = SubmitReadme::sha256OfFile(info.surveyPath, &hashError);
    if (readme.surveySha256.isEmpty()) return fail(hashError);
  }
  readme.hasSheetPdf = QFile::exists(dir.filePath(QStringLiteral("조사도면.pdf")));
  readme.hasSectionPdf = QFile::exists(dir.filePath(QStringLiteral("단면도.pdf")));
  QSaveFile f(dir.filePath(QStringLiteral("README_submit.txt")));
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(QStringLiteral("README_submit.txt를 쓸 수 없습니다(%1). 폴더 쓰기 권한과 남은 공간을 확인하세요.")
                    .arg(f.errorString()));
  QTextStream ts(&f);
  ts.setEncoding(QStringConverter::Utf8);
  ts << SubmitReadme::build(readme);
  ts.flush();
  if (ts.status() != QTextStream::Ok || !f.commit())
    return fail(QStringLiteral("README_submit.txt 저장 실패: %1").arg(f.errorString()));

  if (!step(QStringLiteral("MANIFEST.sha256"))) return fail(canceled);
  QString merr;
  if (!writeSha256Manifest(staging.path(), &merr)) return fail(merr);
  if (!step(QStringLiteral("확정"))) return fail(canceled);
  const QFileInfo current(finalPath);
  // Recheck after PDF rendering, which can dispatch events. Never replace a
  // directory that appeared while the package was being generated.
  if (current.isSymLink() || (!existed && current.exists()) ||
      (existed && (!current.isDir() || hasEntries(finalPath) || !parent.rmdir(destination.fileName()))))
    return fail(QStringLiteral("제출 중 결과 폴더가 변경되었거나 잠겨 있습니다. 새 폴더로 다시 시도하세요."));
  if (!parent.rename(QFileInfo(staging.path()).fileName(), destination.fileName()))
    return fail(QStringLiteral("제출 결과 폴더를 확정하지 못했습니다. 쓰기 권한과 다른 프로그램의 사용 여부를 확인하세요."));
  staging.setAutoRemove(false);
  if (info.progress) info.progress(total, total, QStringLiteral("완료"));
  return outDir;
}
