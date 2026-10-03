#include "KaCadImport.h"

#include "KaBlockingTask.h"
#include "KaCadImportNotice.h"
#include "KaUserError.h"
#include "core/CadCrsGuess.h"
#include "core/CadCrsHints.h"
#include "core/CadDrawingLayers.h"
#include "core/CadDrawingReader.h"
#include "core/CadDrawingStore.h"
#include "core/CadDwgConverter.h"
#include "core/CadPendingCopies.h"
#include "core/FileCleanup.h"
#include "core/KaSessionLog.h"
#include "core/LayerOps.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeedback.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgsvectorlayer.h>

namespace KaCadImport {
namespace {

const QString kTitle = QStringLiteral("도면 불러오기");
const QString kCanceled = QStringLiteral("도면 불러오기를 취소했습니다.");

QString sha256Of(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(&file);
  return QString::fromLatin1(hash.result().toHex());
}

void status(const Hooks& hooks, const QString& text) {
  if (hooks.window) hooks.window->statusBar()->showMessage(text, 10000);
}

// 변환·읽기 실패를 한국어로 알린다. GDAL·LibreDWG 의 영어 원문은 자세히에만 둔다.
void warnRead(const Hooks& hooks, bool converting, const QString& error, const QString& details) {
  QString why = QStringLiteral("DXF 형식이 아니거나 손상되었습니다.");
  QString how = QStringLiteral("CAD 프로그램에서 다시 DXF로 저장해 보세요.");
  if (converting && error == QStringLiteral("DWG 변환 도구(LibreDWG)를 찾지 못했습니다.")) {
    why = QStringLiteral("앱 폴더의 tools/libredwg 가 비어 있습니다.");
    how = QStringLiteral("앱을 다시 설치하거나 CAD에서 DXF로 저장해 넣어 주세요.");
  } else if (converting) {
    why = QStringLiteral("DWG 형식이 새롭거나 일부가 손상되었을 수 있습니다.");
    how = QStringLiteral("CAD 프로그램에서 DXF(2000 형식)로 저장해 다시 넣어 주세요.");
  } else if (error == QStringLiteral("도면에 모형 공간 도형이 없습니다.")) {
    why = QStringLiteral("도면 내용이 모두 종이 공간(배치)에 있습니다.");
    how = QStringLiteral("CAD에서 모형 공간에 그린 도면을 넣어 주세요.");
  }
  KaUserError::warn(hooks.window, {kTitle, error, why, how, QString(), details});
}

QString sourceFile(const QgsMapLayer* layer) {
  return QgsProviderRegistry::instance()
      ->decodeUri(QStringLiteral("ogr"), layer->source())
      .value(QStringLiteral("path"))
      .toString();
}

// 지도 화면 범위를 작업 좌표계로. 바꿀 수 없으면 빈 사각형.
QgsRectangle viewInWork(const QgsMapCanvas* canvas, const QgsCoordinateReferenceSystem& workCrs,
                        const QgsCoordinateTransformContext& context) {
  if (!canvas) return {};
  const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
  if (!canvasCrs.isValid() || canvasCrs == workCrs) return canvas->extent();
  try {
    return QgsCoordinateTransform(canvasCrs, workCrs, context).transformBoundingBox(canvas->extent());
  } catch (const QgsCsException&) {
    return {};
  }
}

}  // namespace

bool run(const Hooks& hooks, const QString& path, const QString& forcedAuthId) {
  QgsProject* project = QgsProject::instance();
  const QgsCoordinateTransformContext context = project->transformContext();
  const QgsCoordinateReferenceSystem workCrs =
      project->crs().isValid() ? project->crs() : QgsCoordinateReferenceSystem(hooks.workCrs);
  const QFileInfo source(path);
  const bool isDwg = source.suffix().compare(QStringLiteral("dwg"), Qt::CaseInsensitive) == 0;
  const QString sha = sha256Of(path);

  // 1. 읽기. DWG 는 앱 관리 임시 폴더에서 먼저 DXF 로 바꾼다(원본은 읽기만 한다).
  const QString convertRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                              QStringLiteral("/cad-convert");
  QDir().mkpath(convertRoot);
  QTemporaryDir workDir(convertRoot + QStringLiteral("/XXXXXX"));
  CadDrawing drawing;
  QString error;
  QString details;
  bool converting = false;
  bool canceled = false;
  const bool read = KaBlockingTask::run(hooks.window, QStringLiteral("도면을 읽는 중…"), [&](QgsFeedback* feedback) {
    const auto stop = [feedback] { return feedback->isCanceled(); };
    QString dxf = path;
    bool ok = true;
    if (isDwg) {
      converting = true;
      const CadDwgConverter::Result converted =
          CadDwgConverter::convert(path, workDir.path(), CadDwgConverter::bundledTool(), 120000, stop);
      ok = converted.ok;
      error = converted.error;
      details = converted.details;
      dxf = converted.dxfPath;
    }
    if (ok) {
      converting = false;
      ok = CadDrawingReader::read(dxf, &drawing, &error, &details, stop);
    }
    canceled = feedback->isCanceled();
    return ok;
  });
  if (!read) {
    if (canceled || error.isEmpty()) status(hooks, kCanceled);
    else warnRead(hooks, converting, error, details);
    return false;
  }

  // 2. 좌표계. 사용자에게 묻지 않는다(사용자는 도면 좌표계를 모른다, 2026-10-03). 같은 원본을 전에 제자리에 올렸으면
  //    그 좌표계, 아니면 도면이 밝힌 좌표계 → 조사 위치 → 지도 화면 → 지명 → 최근 좌표계 → 기본 순서(CadCrsGuess).
  QStringList texts;
  for (const CadEntity& entity : drawing.entities)
    if (entity.kind == CadKind::Text && !entity.text.isEmpty()) texts << entity.text;
  const CadCrsClues clues{CadCrsHints::stated(path, texts), CadCrsGuess::siteLocation(project),
                          CadCrsGuess::trustedView(project, viewInWork(hooks.canvas, workCrs, context)),
                          CadCrsHints::provinces(QStringList{source.completeBaseName(),
                                                             QFileInfo(hooks.surveyPath).completeBaseName()} + texts),
                          CadCrsHints::recent()};
  const CadCrsResult guess = CadCrsGuess::guess(drawing.robustExtent, workCrs, clues, context);
  QString authId;  // 빈 값 = 좌표 없는 도면
  QString verdict = QStringLiteral("지정");
  const QString remembered = CadCrsHints::remembered(sha);
  bool remember = false;  // 지도에 다 올리면 authId 를 이 원본의 좌표계로 기억한다(빈 값이면 기억을 지운다)
  bool likely = false;    // 단서로 정하지 못해 가장 그럴듯한 자리에 올렸다
  if (forcedAuthId.isEmpty() && !remembered.isEmpty()) {
    verdict = QStringLiteral("기억");
    authId = remembered;
  } else if (forcedAuthId.isEmpty()) {
    verdict = guess.verdict == CadCrsVerdict::Certain  ? QStringLiteral("확실 · ") + guess.reason
              : guess.verdict == CadCrsVerdict::Likely ? QStringLiteral("가장 그럴듯함 · ") + guess.reason
                                                        : QStringLiteral("좌표 없음");
    if (guess.verdict != CadCrsVerdict::NoCrs) authId = guess.candidates.first().authId;
    remember = guess.verdict == CadCrsVerdict::Certain;
    likely = guess.verdict == CadCrsVerdict::Likely;
  } else {
    if (forcedAuthId != QLatin1String(kNoCrs)) authId = forcedAuthId;
    remember = true;  // 알림에서 고른 자리. 「좌표 없는 도면으로 보기」면 기억을 지운다
  }

  // 3. 변환본 GPKG. 조사가 열려 있으면 조사 폴더의 가져온자료/도면, 아니면 앱 관리 폴더.
  const QString surveyDir = hooks.surveyPath.isEmpty() ? QString() : QFileInfo(hooks.surveyPath).absolutePath();
  const QString out = CadDrawingStore::outputPathFor(path, surveyDir);
  const CadStoreInfo info{path, sha, authId, isDwg ? CadDwgConverter::converterLabel() : QString()};
  error.clear();
  canceled = false;
  bool stored = false;
  const bool written = KaBlockingTask::run(hooks.window, QStringLiteral("도면을 저장하는 중…"), [&](QgsFeedback* feedback) {
    stored = CadDrawingStore::write(drawing, info, workCrs, context, out, &error,
                                    [feedback] { return feedback->isCanceled(); });
    canceled = feedback->isCanceled();
    return stored;
  });
  if (!written) {
    if (stored) FileCleanup::removeWhenFree(out);  // 다 쓴 뒤에 취소했으면 변환본을 남기지 않는다
    if (canceled || error.isEmpty()) {
      status(hooks, kCanceled);
    } else {
      KaUserError::warn(hooks.window, {kTitle, error, QDir::toNativeSeparators(out),
                                       QStringLiteral("그 폴더에 쓸 수 있는지, 같은 이름의 파일을 연 프로그램이 없는지 확인해 주세요.")});
    }
    return false;
  }

  // 4. 지도에 올린다. 같은 원본에서 올린 묶음은 새것으로 바꾸고(이름만 같은 다른 도면은 「(도면 2)」로 따로 둔다),
  //    앱 폴더의 옛 변환본과 한 번도 저장되지 않은 옛 변환본은 지울 수 있으면 지운다. 저장된 조사가 쓰는 것은 남긴다.
  const QString title = CadDrawingLayers::titleFor(project, path);
  QStringList oldFiles;
  for (QgsVectorLayer* layer : CadDrawingLayers::layersOf(project, CadDrawingLayers::drawingIdOfGroup(project, title)))
    if (!oldFiles.contains(sourceFile(layer))) oldFiles << sourceFile(layer);
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QList<QgsVectorLayer*> added = CadDrawingLayers::addToProject(project, out, title, id, &error);
  if (added.isEmpty()) {
    FileCleanup::removeWhenFree(out);
    KaUserError::warn(hooks.window, {kTitle, error, QDir::toNativeSeparators(out),
                                     QStringLiteral("변환본 파일을 다른 프로그램이 열고 있지 않은지 확인해 주세요.")});
    return false;
  }
  if (!surveyDir.isEmpty()) CadPendingCopies::add(out);  // 작업공간에 저장되기 전까지는 지워도 되는 변환본
  for (QgsVectorLayer* layer : added)  // 가장 그럴듯한 자리에 둔 도면은 다음 도면의 위치 단서가 아니다
    layer->setCustomProperty(QString::fromLatin1(CadDrawingLayers::kPropUnsure), likely);
  const QString appDrawings = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                              QStringLiteral("/cad-drawings/");
  for (const QString& file : oldFiles) {
    const QString clean = QFileInfo(file).absoluteFilePath();
    if (clean == QFileInfo(out).absoluteFilePath() || !(clean.startsWith(appDrawings) || CadPendingCopies::isPending(clean)))
      continue;
    FileCleanup::removeWhenFree(clean);  // 지도가 아직 읽고 있으면 잠시 뒤 다시 지운다(R89)
  }
  if (remember) CadCrsHints::remember(sha, authId);

  // 5. 판단용 범위로 화면을 옮기고 알린다. 좌표 없는 도면은 정합 화면에서 보므로 지도 화면을 그대로 둔다.
  LayerOps::ensureOtfEnabled(project, hooks.canvas, workCrs.authid());
  if (hooks.canvas) {
    LayerOps::syncMapCanvas(project, hooks.canvas, false);
    if (!authId.isEmpty()) {
      QgsRectangle view = drawing.robustExtent;
      try {
        view = QgsCoordinateTransform(QgsCoordinateReferenceSystem(authId), workCrs, context).transformBoundingBox(view);
      } catch (const QgsCsException&) {
      }
      view.grow(std::max({view.width(), view.height(), 10.0}) * 0.1);
      hooks.canvas->setExtent(view);
    }
    LayerOps::refreshCanvasIfIdle(hooks.canvas);
  }
  int counts[4] = {0, 0, 0, 0};
  for (const CadEntity& entity : drawing.entities) ++counts[static_cast<int>(entity.kind)];
  const QString used = authId.isEmpty() ? QStringLiteral("좌표 없음")
                                        : QStringLiteral("%1(%2)").arg(CadCrsGuess::label(authId), authId);
  // 이름은 마지막에 한 번에 넣는다: 파일 이름의 「%숫자」가 다음 arg 에 바뀌지 않게.
  status(hooks, QStringLiteral("도면 「%1」 올림 · 도형 %2개 · %3")
                    .arg(source.completeBaseName(), QString::number(drawing.entities.size()), used));
  const QString tally = QStringLiteral("선 %1 면 %2 점 %3 글자 %4 · 종이 공간 %5 · GDAL 경고 %6")
                            .arg(counts[0])
                            .arg(counts[1])
                            .arg(counts[2])
                            .arg(counts[3])
                            .arg(drawing.paperSpaceSkipped)
                            .arg(drawing.gdalWarnings);
  KaSessionLog::line(QStringLiteral("[cad] %1 · %2 · %3 · %4")
                         .arg(QDir::toNativeSeparators(path), verdict,
                              authId.isEmpty() ? QStringLiteral("없음") : authId, tally));

  // 6. 좌표가 없으면 그 도면의 선 레이어로 정합을 바로 시작하고, 있으면 바꿀 수 있게 알림을 둔다.
  dropNotices(hooks.messageBar, title);
  if (authId.isEmpty()) {
    if (hooks.startAlign) hooks.startAlign(CadDrawingLayers::alignLayerOf(project, id));
  } else {
    showNotice(hooks, title, id, path, guess, authId, workCrs.authid(), likely);
  }
  return true;
}

}  // namespace KaCadImport
