#include "SurveyContourBuilder.h"

#include "SurveyContourMath.h"
#include "SurveyContourWrite.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

namespace {

bool nearlyCollinear(const QVector<SurveyPoint>& points) {
  double span = 0;
  double maxCross = 0;
  const SurveyPoint& origin = points.first();
  int far = 1;
  for (int i = 1; i < points.size(); ++i) {
    const double distance = std::hypot(points[i].x - origin.x, points[i].y - origin.y);
    if (distance > span) {
      span = distance;
      far = i;
    }
  }
  if (span < 1e-6) return true;
  const SurveyPoint& axis = points[far];
  for (const SurveyPoint& point : points) {
    maxCross = std::max(maxCross, std::abs((axis.x - origin.x) * (point.y - origin.y) -
                                           (axis.y - origin.y) * (point.x - origin.x)));
  }
  return maxCross < 0.05 * span;
}

}  // namespace

QString SurveyContourBuilder::rejection(const QVector<SurveyPoint>& points) {
  if (points.size() < 3)
    return QStringLiteral("등고선을 만들려면 점이 3개 이상 필요합니다.");
  if (nearlyCollinear(points))
    return QStringLiteral("점이 일직선이라 삼각망을 만들 수 없습니다.");
  return {};
}

double SurveyContourBuilder::autoCellSize(const QVector<SurveyPoint>& points) {
  const int count = points.size();
  if (count < 2) return 0.5;
  const int sample = std::min(count, 300);
  const int step = std::max(1, count / sample);
  QVector<double> nearest;
  for (int i = 0; i < count; i += step) {
    double best = 1e300;
    for (int j = 0; j < count; ++j) {
      if (i == j) continue;
      const double distance = std::hypot(points[i].x - points[j].x, points[i].y - points[j].y);
      if (distance > 1e-6 && distance < best) best = distance;
    }
    if (best < 1e299) nearest.push_back(best);
  }
  if (nearest.isEmpty()) return 0.5;
  std::sort(nearest.begin(), nearest.end());
  return std::clamp(nearest[nearest.size() / 2] * 0.5, 0.05, 1.0);
}

int SurveyContourBuilder::autoBandIntervalCm(int minCm, int maxCm) {
  return std::max(10, qRound(static_cast<double>(std::max(10, maxCm - minCm)) / 100.0) * 10);
}

bool SurveyContourBuilder::installFiles(const QString& fromDir, const QString& toDir, QString* error) {
  if (!QDir().mkpath(toDir)) {
    if (error) *error = QStringLiteral("결과 폴더를 만들지 못했습니다.");
    return false;
  }
  for (const char* name : {"contours.gpkg", "surface.tif"}) {
    const QString source = QDir(fromDir).filePath(QString::fromUtf8(name));
    const QString dest = QDir(toDir).filePath(QString::fromUtf8(name));
    if (!QFileInfo::exists(source)) continue;
    QFile::remove(dest);
    QFile::remove(dest + QStringLiteral("-wal"));
    QFile::remove(dest + QStringLiteral("-shm"));
    if (!QFile::rename(source, dest) && !(QFile::copy(source, dest) && QFile::remove(source))) {
      if (error) *error = QStringLiteral("결과 파일을 조사 폴더에 두지 못했습니다.");
      return false;
    }
  }
  return true;
}

SurveyContourResult SurveyContourBuilder::build(const QVector<SurveyPoint>& points, const SurveyContourJob& job,
                                                const std::function<bool()>& cancel,
                                                const std::function<void(double)>& progress) {
  SurveyContourResult result;
  if (const QString why = rejection(points); !why.isEmpty()) {
    result.error = why;
    return result;
  }
  if (job.intervalCm < 10 || job.outputDir.isEmpty()) {
    result.error = QStringLiteral("등고선 간격과 결과 폴더가 필요합니다.");
    return result;
  }
  if (!QDir().mkpath(job.outputDir)) {
    result.error = QStringLiteral("결과 폴더를 만들지 못했습니다.");
    return result;
  }
  int minCm = surveyMetersToCm(points.first().z);
  int maxCm = minCm;
  double minX = points.first().x, maxX = minX, minY = points.first().y, maxY = minY;
  for (const SurveyPoint& point : points) {
    const int cm = surveyMetersToCm(point.z);
    minCm = std::min(minCm, cm);
    maxCm = std::max(maxCm, cm);
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }
  const double cell = job.cellSizeM > 0 ? job.cellSizeM : autoCellSize(points);
  return writeSurveyContourFiles(points, job, minCm, maxCm, minX - cell * 0.5, maxX + cell * 0.5,
                                 minY - cell * 0.5, maxY + cell * 0.5, cell, cancel, progress);
}
