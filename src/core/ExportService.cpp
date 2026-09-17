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
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransformcontext.h>
#include <qgslayoutmanager.h>

static QgsVectorLayer* layerByName(QgsProject* p, const QString& name) {
  return LayerOps::findByLayerKey(p, name);
}

static bool isSectionSheetComposed(QgsProject* project) {
  if (!project || !project->layoutManager())
    return false;
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

  const QString enc = (encoding.compare(QStringLiteral("EUC-KR"), Qt::CaseInsensitive) == 0
                       || encoding.compare(QStringLiteral("CP949"), Qt::CaseInsensitive) == 0)
                          ? QStringLiteral("CP949")
                          : QStringLiteral("UTF-8");

  if (project) {
    const QStringList names = {
      QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
      QStringLiteral("feature_line"), QStringLiteral("control_points"),
      QStringLiteral("section_line"), QStringLiteral("artifact_point"),
      QStringLiteral("trial_trench")
    };
    const QgsCoordinateReferenceSystem epsg5179(QStringLiteral("EPSG:5179"));
    for (const QString& n : names) {
      QgsVectorLayer* vl = layerByName(project, n);
      if (!vl || vl->featureCount() <= 0) continue;
      // 이름 폴백으로 찾았더라도 명시적인 참조 자료는 제출 SHP에 넣지 않는다.
      // 주변유적의 표시 이름을 도메인 이름으로 바꿔도 사유 자료 역할은 유지된다.
      if (vl->customProperty(QString::fromUtf8(LayerOps::kPropLayerRole)).toString() ==
          QLatin1String(LayerOps::kRoleReference)) continue;
      const QString shp = dir.filePath(n + QStringLiteral(".shp"));
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("ESRI Shapefile");
      opts.fileEncoding = enc;
      if (vl->crs().isValid() && vl->crs() != epsg5179) {
        opts.ct = QgsCoordinateTransform(vl->crs(), epsg5179, project->transformContext());
      }
      QString errMsg, newFn, newLayer;
      const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
          vl, shp, project->transformContext(), opts, &errMsg, &newFn, &newLayer);
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

  // 1. Export user_sheet as 조사도면.pdf if composed
  const bool hasUserSheet = project && LayoutService::isComposedStudioSheet(project, QStringLiteral("user_sheet"));
  if (hasUserSheet) {
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
