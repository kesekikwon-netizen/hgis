// KaAlignMapTool 의 벡터 정합 저장. KaAlignMapTool.cpp 가 줄 수 기준을 넘지 않게 이 파일로 나눴다.
#include "KaAlignMapTool.h"
#include "core/CadDrawingLayers.h"
#include "core/LayerOps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <qgsproject.h>
#include <qgsvectorlayer.h>

bool KaAlignMapTool::saveVectorAligned(QString* savedPath, QString* errorOut) {
  auto* vl = qobject_cast<QgsVectorLayer*>(m_layer.data());
  if (!vl) return false;
  // 도면: 같은 변환을 변환본의 모든 표에 쓰고 원본 옆 _aligned.gpkg 는 만들지 않는다. 같은 변환이 두 번
  // 들어가지 않게 여기서 세션을 끝낸다(맞춤 복제본을 빼고 그 도면 레이어를 모두 다시 보인다).
  if (const QString drawing = CadDrawingLayers::drawingIdOf(m_hiddenSource); !drawing.isEmpty()) {
    if (!CadDrawingLayers::saveAlignment(QgsProject::instance(), drawing, m_affine, errorOut)) return false;
    if (savedPath) *savedPath = m_hiddenSource->source().section(QLatin1Char('|'), 0, 0);
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
  if (CadDrawingLayers::drawingIdOf(m_hiddenSource).isEmpty()) return;
  QgsProject* project = QgsProject::instance();
  if (m_layer && m_layer != m_hiddenSource.data()) project->removeMapLayer(m_layer->id());  // 맞춤 복제본
  m_cadHidden << m_hiddenSource->id();
  CadDrawingLayers::showLayers(project, m_cadHidden);
  m_cadHidden.clear();
}
