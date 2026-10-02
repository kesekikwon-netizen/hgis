#include "GeorefBackup.h"
#include "GeorefService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

#include <qgscoordinatereferencesystem.h>
#include <qgsdataprovider.h>
#include <qgsrasterlayer.h>
#include <gdal.h>

namespace GeorefBackup {
namespace {

bool fail(QString* errorOut, const QString& message) {
  if (errorOut) *errorOut = message;
  return false;
}

bool isGtiff(const QString& path) {
  GDALAllRegister();
  GDALDriverH drv = GDALIdentifyDriverEx(qUtf8Printable(path), GDAL_OF_RASTER, nullptr, nullptr);
  return drv && qstrcmp(GDALGetDriverShortName(drv), "GTiff") == 0;
}

// Reads only what is stored inside the GeoTIFF (not world file, not .aux.xml).
bool readInternal(const QString& path, RasterBackup* b) {
  b->hadGeoTransform = false;
  b->gcps.clear();
  b->gcpWkt.clear();
  if (!isGtiff(path)) return true;
  const char* const options[] = {"GEOREF_SOURCES=INTERNAL", nullptr};
  GDALDatasetH ds = GDALOpenEx(qUtf8Printable(path), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr,
                               options, nullptr);
  if (!ds) return false;
  double gt[6] = {0, 1, 0, 0, 0, 1};
  if (GDALGetGeoTransform(ds, gt) == CE_None) {
    b->hadGeoTransform = true;
    std::copy(gt, gt + 6, b->geoTransform);
  }
  const int n = GDALGetGCPCount(ds);
  const GDAL_GCP* list = n > 0 ? GDALGetGCPs(ds) : nullptr;
  for (int i = 0; list && i < n; ++i) {
    Gcp g;
    g.id = list[i].pszId;
    g.info = list[i].pszInfo;
    g.pixel = list[i].dfGCPPixel;
    g.line = list[i].dfGCPLine;
    g.x = list[i].dfGCPX;
    g.y = list[i].dfGCPY;
    g.z = list[i].dfGCPZ;
    b->gcps.append(g);
  }
  if (list) b->gcpWkt = GDALGetGCPProjection(ds);
  GDALClose(ds);
  return true;
}

bool sameInternal(const RasterBackup& a, const RasterBackup& b) {
  if (a.hadGeoTransform != b.hadGeoTransform || a.gcps.size() != b.gcps.size()) return false;
  for (int i = 0; a.hadGeoTransform && i < 6; ++i) {
    if (std::abs(a.geoTransform[i] - b.geoTransform[i]) > 1e-12) return false;
  }
  return true;
}

// GCPs replace a geotransform (GDAL clears one when the other is set), so GCPs go first.
bool writeInternal(const RasterBackup& b) {
  GDALDatasetH ds = GDALOpen(qUtf8Printable(b.imagePath), GA_Update);
  if (!ds) return false;
  CPLErr err = CE_None;
  if (!b.gcps.isEmpty()) {
    QVector<GDAL_GCP> arr(b.gcps.size());
    for (int i = 0; i < b.gcps.size(); ++i) {
      const Gcp& g = b.gcps[i];
      arr[i].pszId = const_cast<char*>(g.id.constData());
      arr[i].pszInfo = const_cast<char*>(g.info.constData());
      arr[i].dfGCPPixel = g.pixel;
      arr[i].dfGCPLine = g.line;
      arr[i].dfGCPX = g.x;
      arr[i].dfGCPY = g.y;
      arr[i].dfGCPZ = g.z;
    }
    err = GDALSetGCPs(ds, static_cast<int>(arr.size()), arr.data(), b.gcpWkt.constData());
  } else if (b.hadGeoTransform) {
    double gt[6];
    std::copy(b.geoTransform, b.geoTransform + 6, gt);
    err = GDALSetGeoTransform(ds, gt);
  }
  GDALClose(ds);
  return err == CE_None;
}

QByteArray fileBytes(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// True when every sidecar is back exactly as backed up (absent ones absent).
bool sidecarsMatch(const RasterBackup& b) {
  for (const Sidecar& s : b.sidecars) {
    const bool exists = QFileInfo::exists(s.path);
    if (s.copy.isEmpty()) {
      if (exists) return false;
    } else if (!exists || fileBytes(s.path) != fileBytes(s.copy)) {
      return false;
    }
  }
  return true;
}

void writeManifest(const RasterBackup& b) {
  QJsonObject o;
  o.insert(QStringLiteral("image"), QDir::toNativeSeparators(b.imagePath));
  o.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
  QJsonArray files;
  for (const Sidecar& s : b.sidecars) {
    QJsonObject f;
    f.insert(QStringLiteral("file"), QFileInfo(s.path).fileName());
    f.insert(QStringLiteral("existed"), !s.copy.isEmpty());
    files.append(f);
  }
  o.insert(QStringLiteral("sidecars"), files);
  if (b.hadGeoTransform) {
    QJsonArray gt;
    for (double v : b.geoTransform) gt.append(v);
    o.insert(QStringLiteral("internalGeoTransform"), gt);
  }
  o.insert(QStringLiteral("internalGcpCount"), static_cast<int>(b.gcps.size()));
  QFile f(QDir(b.folder).filePath(QStringLiteral("backup.json")));
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

}  // namespace

QString backupRootFor(const QString& surveyPath) {
  const QString name = QStringLiteral("정합백업");
  if (!surveyPath.isEmpty()) return QFileInfo(surveyPath).absoluteDir().filePath(name);
  return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(name);
}

QStringList pamSidecarPaths(const QString& imagePath) {
  const QFileInfo fi(imagePath);
  QStringList out{imagePath + QStringLiteral(".aux.xml")};
  const QString byBase =
      fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".aux.xml");
  if (byBase != out.first()) out << byBase;  // one name only when the image has no extension
  return out;
}

bool pamGeorefPresent(const QString& imagePath) {
  for (const QString& p : pamSidecarPaths(imagePath)) {
    if (pamCarriesGeoref(p)) return true;
  }
  return false;
}

void releaseRasterHandle(QgsRasterLayer* layer) {
  if (!layer) return;
  if (QgsDataProvider* p = layer->dataProvider()) p->reloadData();
}

void reopenRasterLayer(QgsRasterLayer* layer, const QgsCoordinateReferenceSystem& crs) {
  if (!layer) return;
  layer->setDataSource(layer->source(), layer->name(), QStringLiteral("gdal"), false);
  if (crs.isValid()) layer->setCrs(crs);
}

bool pamCarriesGeoref(const QString& auxPath) {
  QFile f(auxPath);
  if (!f.open(QIODevice::ReadOnly)) return false;
  const QByteArray xml = f.readAll();
  return xml.contains("<GeoTransform") || xml.contains("<GCPList");
}

bool hasInternalGeoref(const QString& imagePath) {
  RasterBackup probe;
  return readInternal(imagePath, &probe) && (probe.hadGeoTransform || !probe.gcps.isEmpty());
}

bool backupRaster(const QString& imagePath, const QString& backupRoot, RasterBackup* out,
                  QString* errorOut) {
  if (!out) return false;
  *out = RasterBackup();
  const QFileInfo image(imagePath);
  if (imagePath.isEmpty() || !image.exists())
    return fail(errorOut, QStringLiteral("그림 파일을 찾지 못했습니다"));
  if (backupRoot.isEmpty()) return fail(errorOut, QStringLiteral("백업 폴더가 없습니다"));
  const QString base = QDir(backupRoot).filePath(
      image.completeBaseName() + QLatin1Char('_')
      + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
  QString folder = base;
  for (int i = 2; QFileInfo::exists(folder); ++i) folder = base + QStringLiteral("_%1").arg(i);
  if (!QDir().mkpath(folder))
    return fail(errorOut, QStringLiteral("백업 폴더를 만들지 못했습니다: %1")
                              .arg(QDir::toNativeSeparators(folder)));

  RasterBackup b;
  b.imagePath = imagePath;
  b.folder = folder;
  QStringList paths = {GeorefService::worldFilePathFor(imagePath),
                       GeorefService::prjPathFor(imagePath)};
  paths += pamSidecarPaths(imagePath);
  for (const QString& p : paths) {
    Sidecar s;
    s.path = p;
    if (QFileInfo::exists(p)) {
      s.copy = QDir(folder).filePath(QFileInfo(p).fileName());
      if (!QFile::copy(p, s.copy))
        return fail(errorOut, QStringLiteral("%1 을(를) 백업하지 못했습니다")
                                  .arg(QFileInfo(p).fileName()));
    }
    b.sidecars.append(s);
  }
  if (!readInternal(imagePath, &b))
    return fail(errorOut, QStringLiteral("그림 파일의 좌표를 읽지 못했습니다"));
  writeManifest(b);
  b.valid = true;
  *out = b;
  return true;
}

bool restoreRaster(const RasterBackup& b, QString* errorOut) {
  if (!b.valid) return fail(errorOut, QStringLiteral("되돌릴 백업이 없습니다"));
  const QString where = QDir::toNativeSeparators(b.folder);
  // Internal georeferencing first: GDAL may touch sidecars while the file is open.
  if (b.hadGeoTransform || !b.gcps.isEmpty()) {
    RasterBackup now;
    now.imagePath = b.imagePath;
    const bool unchanged = readInternal(b.imagePath, &now) && sameInternal(now, b);
    if (!unchanged && !writeInternal(b))
      return fail(errorOut, QStringLiteral("그림 파일 안의 좌표를 되돌리지 못했습니다. 백업: %1")
                                .arg(where));
  }
  QStringList failed;
  for (const Sidecar& s : b.sidecars) {
    const bool exists = QFileInfo::exists(s.path);
    if (s.copy.isEmpty()) {
      if (exists && !QFile::remove(s.path)) failed << QFileInfo(s.path).fileName();
      continue;
    }
    if (exists) QFile::remove(s.path);
    if (!QFile::copy(s.copy, s.path)) failed << QFileInfo(s.path).fileName();
  }
  if (!failed.isEmpty())
    return fail(errorOut, QStringLiteral("%1 을(를) 되돌리지 못했습니다. 백업: %2")
                              .arg(failed.join(QStringLiteral(", ")), where));
  return true;
}

bool restoreRasterLayer(QgsRasterLayer* layer, const RasterBackup& backup,
                        const QgsCoordinateReferenceSystem& crs, QString* errorOut) {
  if (!layer) return fail(errorOut, QStringLiteral("그림 레이어가 없습니다"));
  // The handle open on the aligned image flushes its PAM state when it closes; let that
  // happen before anything is put back, or the flush would undo the restore.
  releaseRasterHandle(layer);
  if (!restoreRaster(backup, errorOut)) return false;
  reopenRasterLayer(layer, crs);
  // A handle released during the re-read may still have flushed its sidecar: once more.
  if (!sidecarsMatch(backup)) {
    if (!restoreRaster(backup, errorOut)) return false;
    reopenRasterLayer(layer, crs);
  }
  layer->triggerRepaint();
  if (!layer->isValid())
    return fail(errorOut, QStringLiteral("되돌린 그림을 다시 읽지 못했습니다"));
  return true;
}

QString uniqueOutputPath(const QString& desiredPath) {
  if (desiredPath.isEmpty() || !QFileInfo::exists(desiredPath)) return desiredPath;
  const QFileInfo fi(desiredPath);
  const QString stem = fi.path() + QLatin1Char('/') + fi.completeBaseName();
  const QString suffix = fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
  for (int i = 2; i < 10000; ++i) {
    const QString candidate = stem + QStringLiteral("_%1").arg(i) + suffix;
    if (!QFileInfo::exists(candidate)) return candidate;
  }
  return stem + QStringLiteral("_%1").arg(QDateTime::currentMSecsSinceEpoch()) + suffix;
}

}  // namespace GeorefBackup
