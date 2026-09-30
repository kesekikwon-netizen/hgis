#include "SurveyContourBuilder.h"

#include "SurveyContourMath.h"
#include "SurveyContourTin.h"
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

const char* const kResultFiles[] = {"contours.gpkg", "surface.tif"};

void removeWithSidecars(const QString& path) {
  QFile::remove(path);
  QFile::remove(path + QStringLiteral("-wal"));
  QFile::remove(path + QStringLiteral("-shm"));
}

// Says what really happened: grid widening, emptied triangles and whether the clip applied.
QString describe(const SurveyContourResult& built, const SurveyContourJob& job) {
  QStringList notes;
  if (built.cellEnlarged)
    notes << QStringLiteral("점이 촘촘하고 범위가 넓어 계산 격자를 %1 m로 넓혔습니다.")
                 .arg(QString::number(built.cellSizeM, 'f', 2));
  if (!built.warning.isEmpty()) notes << built.warning;
  if (built.maskedTriangles > 0)
    notes << QStringLiteral("변이 %1 m보다 긴 삼각형 %2개는 비워 두었습니다.")
                 .arg(QString::number(job.maxEdgeM, 'f', 1))
                 .arg(built.maskedTriangles);
  else if (built.maskedTriangles < 0)
    notes << QStringLiteral("이 GDAL은 삼각망을 만들 수 없어 긴 삼각형을 비우지 못했습니다.");
  if (built.lineCount == 0)
    // Lines existed before the clip, so the clip removed every one of them.
    notes << (built.linesBeforeClip > 0
                  ? QStringLiteral("조사구역과 측량점이 겹치지 않아 남은 선이 없습니다. 파일 좌표계를 확인하세요.")
                  : QStringLiteral("높이차가 등고선 간격보다 작아 선이 없습니다."));
  else if (job.clipWkt.isEmpty())
    notes << QStringLiteral("조사구역이 없어 측량점 범위까지 그렸습니다.");
  else if (built.clipped)
    notes << QStringLiteral("조사구역 안에만 표시합니다.");
  else
    notes << QStringLiteral("조사구역 모양으로 다 자르지 못해 일부는 측량점 범위까지 그렸습니다.");
  return notes.join(QLatin1Char(' '));
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

double SurveyContourBuilder::fitCellSize(double cell, double spanX, double spanY, qint64 maxCells) {
  if (!(cell > 0) || !std::isfinite(spanX) || !std::isfinite(spanY) || maxCells < 4) return cell;
  // writeSurveyContourFiles pads the point bounds by half a cell on every side.
  auto cells = [&](double size) {
    return static_cast<qint64>(std::ceil(spanX / size + 1.0)) * static_cast<qint64>(std::ceil(spanY / size + 1.0));
  };
  if (cells(cell) <= maxCells) return cell;
  double fitted = std::max(cell, std::sqrt(std::max(spanX, 1e-3) * std::max(spanY, 1e-3) /
                                           static_cast<double>(maxCells)));
  fitted = std::ceil(fitted * 100.0) / 100.0;
  while (cells(fitted) > maxCells) fitted += 0.01;
  return fitted;
}

bool SurveyContourBuilder::installFiles(const QString& fromDir, const QString& toDir, QString* error) {
  if (!QDir().mkpath(toDir)) {
    if (error) *error = QStringLiteral("결과 폴더를 만들지 못했습니다.");
    return false;
  }
  struct Moved { QString source; QString dest; QString backup; };
  QVector<Moved> done;
  // New files go back to fromDir so a retry can still install them.
  auto rollback = [&done] {
    for (auto it = done.crbegin(); it != done.crend(); ++it) {
      if (!QFile::rename(it->dest, it->source)) removeWithSidecars(it->dest);
      if (!it->backup.isEmpty()) QFile::rename(it->backup, it->dest);
    }
  };
  for (const char* name : kResultFiles) {
    const QString source = QDir(fromDir).filePath(QString::fromUtf8(name));
    const QString dest = QDir(toDir).filePath(QString::fromUtf8(name));
    if (!QFileInfo::exists(source)) continue;
    // SQLite sidecars exist only while a connection is open; the layers are closed by now.
    QFile::remove(dest + QStringLiteral("-wal"));
    QFile::remove(dest + QStringLiteral("-shm"));
    QString backup;
    if (QFileInfo::exists(dest)) {
      backup = dest + QStringLiteral(".old");
      QFile::remove(backup);
      if (!QFile::rename(dest, backup)) {
        rollback();
        if (error) *error = QStringLiteral("이전 결과 파일이 다른 곳에서 열려 있어 바꾸지 못했습니다. 이전 결과는 그대로 두었습니다.");
        return false;
      }
    }
    if (!QFile::rename(source, dest) && !(QFile::copy(source, dest) && QFile::remove(source))) {
      QFile::remove(dest);
      if (!backup.isEmpty()) QFile::rename(backup, dest);
      rollback();
      if (error) *error = QStringLiteral("결과 파일을 조사 폴더에 두지 못했습니다. 이전 결과는 그대로 두었습니다.");
      return false;
    }
    done.push_back({source, dest, backup});
  }
  for (const Moved& moved : done)
    if (!moved.backup.isEmpty()) QFile::remove(moved.backup);
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
  double cell = job.cellSizeM > 0 ? job.cellSizeM : autoCellSize(points);
  QVector<SurveyPoint> nodes = points;
  const QVector<SurveyPoint> added = densifySurveyBreaklines(job.breaklines, cell, points);
  nodes += added;
  int minCm = surveyMetersToCm(nodes.first().z);
  int maxCm = minCm;
  double minX = nodes.first().x, maxX = minX, minY = nodes.first().y, maxY = minY;
  for (const SurveyPoint& point : nodes) {
    const int cm = surveyMetersToCm(point.z);
    minCm = std::min(minCm, cm);
    maxCm = std::max(maxCm, cm);
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }
  // A dense point cloud over a wide site would exceed the grid budget; widen the cell instead of failing.
  const double fitted = fitCellSize(cell, maxX - minX, maxY - minY);
  const bool enlarged = fitted > cell;
  cell = fitted;
  SurveyContourResult built = writeSurveyContourFiles(nodes, job, minCm, maxCm, minX - cell * 0.5, maxX + cell * 0.5,
                                                      minY - cell * 0.5, maxY + cell * 0.5, cell, cancel, progress);
  built.breaklineNodes = added.size();
  built.cellEnlarged = enlarged;
  if (built.ok) built.warning = describe(built, job);
  return built;
}
