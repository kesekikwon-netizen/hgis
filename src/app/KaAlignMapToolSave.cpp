// KaAlignMapTool 의 벡터 정합 저장. KaAlignMapTool.cpp 가 줄 수 기준을 넘지 않게 이 파일로 나눴다.
#include "KaAlignMapTool.h"
#include "core/CadDrawingLayers.h"
#include "core/LayerOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

// 맞춘 변환은 도면 숫자 → 작업 좌표계 숫자다. 도면 변환본이 다른 좌표계면(도면을 올린 뒤 작업 좌표계를 바꾼 경우)
// 도면 범위 둘레의 세 점을 작업 좌표계에서 도면 좌표계로 옮겨, 도면 좌표계 숫자로 쓰는 변환을 다시 맞춘다.
bool toDrawingCrs(const QList<QgsVectorLayer*>& layers, const QgsCoordinateReferenceSystem& work,
                  GeorefService::Affine* affine, QString* errorOut) {
  if (layers.isEmpty() || !work.isValid() || layers.first()->crs() == work) return true;
  QgsRectangle extent;
  extent.setNull();
  for (QgsVectorLayer* layer : layers) extent.combineExtentWith(layer->extent());
  const double half = std::max({extent.width(), extent.height(), 100.0}) / 2;
  const QgsPointXY c = extent.center();
  QVector<GeorefService::Pair> pairs;
  try {
    const QgsCoordinateTransform toDrawing(work, layers.first()->crs(), QgsProject::instance()->transformContext());
    for (const QgsPointXY& s : {QgsPointXY(c.x() - half, c.y() - half), QgsPointXY(c.x() + half, c.y() - half),
                                QgsPointXY(c.x() - half, c.y() + half)}) {
      double mx = 0, my = 0;
      GeorefService::transform(*affine, s.x(), s.y(), &mx, &my);
      const QgsPointXY m = toDrawing.transform(mx, my);
      pairs.append({s.x(), s.y(), m.x(), m.y()});
    }
  } catch (const QgsCsException&) {
    if (errorOut) *errorOut = QStringLiteral("맞춘 자리를 도면 좌표계로 바꾸지 못했습니다.");
    return false;
  }
  *affine = GeorefService::fromPairs(pairs, false);
  return affine->valid;
}

}  // namespace

bool KaAlignMapTool::saveVectorAligned(QString* savedPath, QString* errorOut) {
  auto* vl = qobject_cast<QgsVectorLayer*>(m_layer.data());
  if (!vl) return false;
  // 도면: 같은 변환을 변환본의 모든 표에 쓰고 원본 옆 _aligned.gpkg 는 만들지 않는다. 같은 변환이 두 번
  // 들어가지 않게 여기서 세션을 끝낸다(맞춤 복제본을 빼고 그 도면 레이어를 모두 다시 보인다).
  if (!m_cadDrawing.isEmpty()) {
    const QList<QgsVectorLayer*> layers = CadDrawingLayers::layersOf(QgsProject::instance(), m_cadDrawing);
    GeorefService::Affine affine = m_affine;
    if (!toDrawingCrs(layers, workCrs(), &affine, errorOut) ||
        !CadDrawingLayers::saveAlignment(QgsProject::instance(), m_cadDrawing, affine, errorOut))
      return false;  // 맞추는 중에 같은 도면을 다시 불러와 레이어가 바뀌었으면 여기서 멈춘다
    if (savedPath) *savedPath = layers.first()->source().section(QLatin1Char('|'), 0, 0);
    endSession();
    return true;
  }
  QString base = vl->name();
  base.replace(QStringLiteral(" 맞춤"), QString());
  QString dir;
  if (m_hiddenSource && !m_hiddenSource->source().isEmpty())
    dir = QFileInfo(m_hiddenSource->source().section(QLatin1Char('|'), 0, 0)).absolutePath();
  if (dir.isEmpty()) dir = QFileInfo(vl->source()).absolutePath();
  if (dir.isEmpty() || dir == QLatin1String(".")) dir = QDir::tempPath();
  // Never delete a file this session did not write: an older <name>_aligned.gpkg gets a
  // numbered sibling instead; saving again in the same session rewrites our own copy.
  if (m_savedVectorPath.isEmpty())
    m_savedVectorPath = GeorefBackup::uniqueOutputPath(
        dir + QLatin1Char('/') + QFileInfo(base).completeBaseName() + QStringLiteral("_aligned.gpkg"));
  const QString out = m_savedVectorPath;
  if (QFile::exists(out)) QFile::remove(out);
  const QString written = GeorefService::saveVectorCopyGpkg(vl, out, workCrs(), errorOut);
  if (written.isEmpty()) return false;
  if (savedPath) *savedPath = written;
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  return true;
}

void KaAlignMapTool::finishDrawingSession() {
  if (m_cadDrawing.isEmpty()) return;
  QgsProject* project = QgsProject::instance();
  if (m_layer && m_layer != m_hiddenSource.data()) project->removeMapLayer(m_layer->id());  // 맞춤 복제본
  if (m_hiddenSource) m_cadHidden << m_hiddenSource->id();
  CadDrawingLayers::showLayers(project, m_cadHidden);
  m_cadHidden.clear();
  m_cadDrawing.clear();
}

void KaAlignMapTool::captureOriginals(QgsVectorLayer* vl) {
  m_originals.clear();
  if (!vl) return;
  QgsFeatureIterator it = vl->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (f.hasGeometry()) m_originals.insert(f.id(), f.geometry());
  }
}
