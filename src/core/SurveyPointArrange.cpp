#include "SurveyPointArrange.h"

#include "LayerOps.h"
#include "SurveyContourMath.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgspointxy.h>

SurveyReadReport SurveyPointArrange::arrange(const SurveyReadReport& fileOrder, const SurveyArrangeOptions& options) {
  SurveyReadReport out = fileOrder;
  out.swapApplied = options.swapAxes;
  out.crsAuthId = options.targetCrsAuthId;
  out.transformFailed = 0;
  const bool transform = !options.sourceCrsAuthId.isEmpty() && options.sourceCrsAuthId != options.targetCrsAuthId;
  out.sourceCrsAuthId = transform ? options.sourceCrsAuthId : QString();
  if (!out.fatal.isEmpty()) return out;
  std::optional<QgsCoordinateTransform> toWork;
  if (transform) {
    const QgsCoordinateReferenceSystem source(options.sourceCrsAuthId);
    const QgsCoordinateReferenceSystem target(options.targetCrsAuthId);
    if (!source.isValid() || !target.isValid()) {
      out.fatal = QStringLiteral("좌표계 %1 을 읽지 못했습니다.").arg(options.sourceCrsAuthId);
      out.points.clear();
      out.breaklines.clear();
      return out;
    }
    // Worker threads must not read QgsProject; Korean datums need no grid shift.
    toWork.emplace(source, target, QgsCoordinateTransformContext());
  }
  auto place = [&](SurveyPoint* point) {
    if (options.swapAxes) std::swap(point->x, point->y);
    if (!toWork) return true;
    try {
      const QgsPointXY mapped = toWork->transform(QgsPointXY(point->x, point->y));
      point->x = mapped.x();
      point->y = mapped.y();
    } catch (const QgsCsException&) {
      return false;
    }
    return std::isfinite(point->x) && std::isfinite(point->y);
  };
  out.points.clear();
  for (SurveyPoint point : fileOrder.points) {
    if (place(&point)) out.points.push_back(point);
    else ++out.transformFailed;
  }
  out.breaklines.clear();
  for (const SurveyPolyline& line : fileOrder.breaklines) {
    SurveyPolyline moved;
    for (SurveyPoint vertex : line)
      if (place(&vertex)) moved.push_back(vertex);
    if (moved.size() >= 2) out.breaklines.push_back(moved);
  }
  const QgsRectangle korea = LayerOps::koreaExtentForCrs(options.targetCrsAuthId);
  out.outsideCount = 0;
  out.suspiciousCount = 0;
  for (const SurveyPoint& point : out.points) {
    if (!korea.isEmpty() && !korea.contains(QgsPointXY(point.x, point.y))) ++out.outsideCount;
    if (point.suspicious) ++out.suspiciousCount;
  }
  out.outsideKorea = out.outsideCount > 0;
  if (!out.points.isEmpty()) {
    out.minCm = surveyMetersToCm(out.points.first().z);
    out.maxCm = out.minCm;
    for (const SurveyPoint& point : out.points) {
      out.minCm = std::min(out.minCm, surveyMetersToCm(point.z));
      out.maxCm = std::max(out.maxCm, surveyMetersToCm(point.z));
    }
  }
  return out;
}

double SurveyPointArrange::gapToExtent(const QVector<SurveyPoint>& points, const QgsRectangle& reference) {
  if (points.isEmpty() || reference.isNull() || !reference.isFinite()) return 0.0;
  double minX = points.first().x, maxX = minX, minY = points.first().y, maxY = minY;
  for (const SurveyPoint& point : points) {
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }
  const double dx = std::max({0.0, reference.xMinimum() - maxX, minX - reference.xMaximum()});
  const double dy = std::max({0.0, reference.yMinimum() - maxY, minY - reference.yMaximum()});
  return std::hypot(dx, dy);
}

QStringList SurveyPointArrange::notes(const SurveyReadReport& report, const QgsRectangle& reference,
                                      bool otherAxisFits) {
  QStringList out;
  if (report.swapApplied)
    out << (report.swapReason.isEmpty()
                ? QStringLiteral("X를 북쪽, Y를 동쪽으로 읽었습니다.")
                : QStringLiteral("X를 북쪽, Y를 동쪽으로 읽었습니다(%1).").arg(report.swapReason));
  else if (report.swapSuggested)
    out << QStringLiteral("자동 판정은 X=북쪽(%1)이지만 지금은 X를 동쪽으로 읽습니다.").arg(report.swapReason);
  if (report.outsideCount > 0)
    out << QStringLiteral("점 %1개가 %2 좌표계의 한국 범위 밖입니다. %3")
               .arg(report.outsideCount)
               .arg(report.crsAuthId,
                    otherAxisFits ? QStringLiteral("X·Y 순서를 바꾸면 모두 범위 안에 듭니다.")
                                  : QStringLiteral("X·Y 순서나 파일 좌표계를 확인하세요."));
  const double gap = gapToExtent(report.points, reference);
  if (gap >= 1000.0)
    out << QStringLiteral("측량점이 조사구역에서 약 %1 km 떨어져 있습니다. 파일 좌표계(원점)를 확인하세요.")
               .arg(gap / 1000.0, 0, 'f', 1);
  if (report.suspiciousCount > 0)
    out << QStringLiteral("표고가 0이거나 999 m 이상인 의심점 %1개. 오측이면 「의심점 빼기」를 켜세요.")
               .arg(report.suspiciousCount);
  if (report.duplicateGroups > 0)
    out << QStringLiteral("같은 좌표 %1곳은 표고를 평균했습니다.").arg(report.duplicateGroups);
  if (report.skipped > 0) out << QStringLiteral("읽지 못한 행 %1개.").arg(report.skipped);
  if (report.transformFailed > 0)
    out << QStringLiteral("좌표계를 바꾸지 못한 점 %1개는 뺐습니다.").arg(report.transformFailed);
  if (!report.droppedLayers.isEmpty())
    out << QStringLiteral("높이가 없는 CAD 레이어는 뺐습니다: %1").arg(report.droppedLayers.join(QStringLiteral(", ")));
  if (report.nonPointCount > 0)
    out << QStringLiteral("점이 아닌 도형 %1개는 건너뛰었습니다.").arg(report.nonPointCount);
  if (!report.breaklines.isEmpty())
    out << QStringLiteral("높이가 있는 선 %1개는 단절선으로 쓸 수 있습니다.").arg(report.breaklines.size());
  if (report.flatLineCount > 0)
    out << QStringLiteral("높이가 없는 선 %1개는 쓰지 않습니다.").arg(report.flatLineCount);
  if (report.issuesOmitted > 0)
    out << QStringLiteral("그 밖의 알림 %1개는 줄였습니다.").arg(report.issuesOmitted);
  return out;
}
