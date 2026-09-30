#include "GeorefService.h"
#include "GeorefBackup.h"
#include "LayerOps.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>
#include <cmath>
#include <algorithm>
#include <memory>

#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsgeometry.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <QPainter>
#include <qgsdataprovider.h>
#include <qgsrasterdataprovider.h>
#include <qgsrastertransparency.h>
#include <qgsrasterrenderer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsrasterblock.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgis.h>
#include <qgswkbtypes.h>
#include <gdal.h>

namespace GeorefService {
namespace {

constexpr double kDetEps = 1e-18;
// Collinear source points: the centred normal matrix is singular relative to its size.
constexpr double kRelDetEps = 1e-10;

// 그림 픽셀은 위에서 아래로 y 가 커지고(행 번호), 지도는 아래에서 위로 커진다.
// 그래서 그림 → 지도 변환에는 반드시 상하 뒤집기가 들어가야 한다(행렬식 < 0).
// 뒤집기 없는 회전만으로 두 점을 맞추면 그 두 점은 정확히 붙지만 나머지 그림은
// 두 점을 잇는 선을 축으로 거울처럼 뒤집힌 자리에 놓인다 — 현장에서 「점은 맞는데
// 사진이 엉뚱한 데로 간다」로 나타난다. sourceYDown 이 그 뒤집기를 켠다.
// (CAD·벡터 정합은 왼쪽도 지도 좌표라 y 가 위로 커지므로 뒤집지 않는다.)
Affine helmertFromTwo(const Pair& p0, const Pair& p1, bool sourceYDown) {
  Affine out;
  const double ky = sourceYDown ? -1.0 : 1.0;
  const double dsx = p1.srcX - p0.srcX;
  const double dsy = ky * (p1.srcY - p0.srcY);
  const double dmx = p1.mapX - p0.mapX;
  const double dmy = p1.mapY - p0.mapY;
  const double lenS = std::hypot(dsx, dsy);
  const double lenM = std::hypot(dmx, dmy);
  if (lenS < 1e-12 || lenM < 1e-12) return out;
  const double scale = lenM / lenS;
  const double ang = std::atan2(dmy, dmx) - std::atan2(dsy, dsx);
  const double c = std::cos(ang);
  const double s = std::sin(ang);
  // (sx, ky*sy) 에 회전을 건 뒤 ky 를 행렬 안으로 접어 넣는다.
  out.a = scale * c;
  out.b = -scale * s * ky;
  out.d = scale * s;
  out.e = scale * c * ky;
  out.c = p0.mapX - out.a * p0.srcX - out.b * p0.srcY;
  out.f = p0.mapY - out.d * p0.srcX - out.e * p0.srcY;
  out.valid = true;
  out.pairCount = 2;
  return out;
}

// Least squares on coordinates centred at their means. Raw sums of 200000-class map
// coordinates (CAD sources) cancel out their significant digits in the normal equations;
// centred sums stay at the size of the drawing, and the translation comes back at the end.
Affine affineLeastSquares(const QVector<Pair>& pairs) {
  Affine out;
  const int n = pairs.size();
  if (n < 3) return out;
  double mx = 0, my = 0, mX = 0, mY = 0;
  for (const Pair& g : pairs) {
    mx += g.srcX;
    my += g.srcY;
    mX += g.mapX;
    mY += g.mapY;
  }
  mx /= n;
  my /= n;
  mX /= n;
  mY /= n;
  double sxx = 0, sxy = 0, syy = 0, sxX = 0, syX = 0, sxY = 0, syY = 0;
  for (const Pair& g : pairs) {
    const double x = g.srcX - mx, y = g.srcY - my;
    const double X = g.mapX - mX, Y = g.mapY - mY;
    sxx += x * x;
    sxy += x * y;
    syy += y * y;
    sxX += x * X;
    syX += y * X;
    sxY += x * Y;
    syY += y * Y;
  }
  const double det = sxx * syy - sxy * sxy;
  if (!(sxx > 0.0 && syy > 0.0) || std::abs(det) <= kRelDetEps * sxx * syy) return out;
  out.a = (sxX * syy - syX * sxy) / det;
  out.b = (syX * sxx - sxX * sxy) / det;
  out.d = (sxY * syy - syY * sxy) / det;
  out.e = (syY * sxx - sxY * sxy) / det;
  out.c = mX - out.a * mx - out.b * my;
  out.f = mY - out.d * mx - out.e * my;
  out.valid = true;
  out.pairCount = n;
  return out;
}

}  // namespace

bool transform(const Affine& a, double sx, double sy, double* mx, double* my) {
  if (!a.valid || !mx || !my) return false;
  *mx = a.a * sx + a.b * sy + a.c;
  *my = a.d * sx + a.e * sy + a.f;
  return true;
}

bool invert(const Affine& a, double mx, double my, double* sx, double* sy) {
  if (!a.valid || !sx || !sy) return false;
  const double det = a.a * a.e - a.b * a.d;
  if (std::abs(det) < kDetEps) return false;
  const double x = mx - a.c;
  const double y = my - a.f;
  *sx = (a.e * x - a.b * y) / det;
  *sy = (-a.d * x + a.a * y) / det;
  return true;
}

QTransform toQTransform(const Affine& a) {
  return QTransform(a.a, a.d, a.b, a.e, a.c, a.f);
}

double rmsMeters(const Affine& a, const QVector<Pair>& pairs) {
  if (!a.valid || pairs.isEmpty()) return 0;
  double acc = 0;
  int n = 0;
  for (const Pair& p : pairs) {
    double mx = 0, my = 0;
    if (!transform(a, p.srcX, p.srcY, &mx, &my)) continue;
    acc += (mx - p.mapX) * (mx - p.mapX) + (my - p.mapY) * (my - p.mapY);
    ++n;
  }
  if (n <= 0) return 0;
  return std::sqrt(acc / n);
}

Affine fromPairs(const QVector<Pair>& pairs, bool sourceYDown) {
  Affine out;
  if (pairs.size() < 2) return out;
  if (pairs.size() == 2) {
    out = helmertFromTwo(pairs[0], pairs[1], sourceYDown);
  } else {
    // 3점 이상이면 일반 어파인이라 데이터가 뒤집기까지 스스로 담아낸다.
    // 다만 점이 한 줄로 서면 풀리지 않아 2점 식으로 내려오므로 여기도 넘겨준다.
    out = affineLeastSquares(pairs);
    if (!out.valid) out = helmertFromTwo(pairs[0], pairs[1], sourceYDown);
  }
  if (out.valid) {
    out.pairCount = pairs.size();
    out.rmsMeters = rmsMeters(out, pairs);
  }
  return out;
}

Affine fitSrcBoxToExtent(double srcMinX, double srcMinY, double srcMaxX, double srcMaxY,
                         const QgsRectangle& dest) {
  Affine out;
  const double sw = srcMaxX - srcMinX;
  const double sh = srcMaxY - srcMinY;
  if (sw < 1e-12 || sh < 1e-12 || dest.isEmpty() || !dest.isFinite()) return out;
  out.a = dest.width() / sw;
  out.b = 0;
  out.c = dest.xMinimum() - out.a * srcMinX;
  out.d = 0;
  out.e = dest.height() / sh;
  out.f = dest.yMinimum() - out.e * srcMinY;
  out.valid = true;
  out.pairCount = 0;
  return out;
}

Affine fitRasterToExtent(int pixelW, int pixelH, const QgsRectangle& dest) {
  Affine out;
  if (pixelW < 2 || pixelH < 2 || dest.isEmpty() || !dest.isFinite()) return out;
  out.a = dest.width() / static_cast<double>(pixelW);
  out.b = 0;
  out.c = dest.xMinimum();
  out.d = 0;
  out.e = -dest.height() / static_cast<double>(pixelH);
  out.f = dest.yMaximum();
  out.valid = true;
  return out;
}

QString worldFilePathFor(const QString& imagePath) {
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    return imagePath.left(imagePath.size() - ext.size()) + QStringLiteral("jgw");
  if (ext == QLatin1String("png"))
    return imagePath.left(imagePath.size() - 3) + QStringLiteral("pgw");
  if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
    return imagePath.left(imagePath.size() - ext.size()) + QStringLiteral("tfw");
  return imagePath + QStringLiteral(".wld");
}

QString prjPathFor(const QString& imagePath) {
  const QFileInfo fi(imagePath);
  return fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".prj");
}

bool writeWorldFile(const QString& imagePath, const Affine& a, QString* errorOut) {
  if (!a.valid) {
    if (errorOut) *errorOut = QStringLiteral("변환이 없습니다");
    return false;
  }
  const QString wfPath = worldFilePathFor(imagePath);
  QFile wf(wfPath);
  if (!wf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("월드파일을 쓸 수 없습니다");
    return false;
  }
  const double A = a.a;
  const double D = a.d;
  const double B = a.b;
  const double E = a.e;
  const double C = a.c + 0.5 * (a.a + a.b);
  const double F = a.f + 0.5 * (a.d + a.e);
  QTextStream ts(&wf);
  ts.setEncoding(QStringConverter::Utf8);
  ts << QString::number(A, 'g', 16) << "\n";
  ts << QString::number(D, 'g', 16) << "\n";
  ts << QString::number(B, 'g', 16) << "\n";
  ts << QString::number(E, 'g', 16) << "\n";
  ts << QString::number(C, 'g', 16) << "\n";
  ts << QString::number(F, 'g', 16) << "\n";
  return true;
}

bool writeSidecarPrj(const QString& imagePath, const QgsCoordinateReferenceSystem& crs,
                     QString* errorOut) {
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계가 없습니다");
    return false;
  }
  QFile f(prjPathFor(imagePath));
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("prj 파일을 쓸 수 없습니다");
    return false;
  }
  f.write(crs.toWkt().toUtf8());
  return true;
}

// Writes the geotransform into the file's own tags; a file GDAL cannot update (read-only
// medium) gets it in the PAM sidecar (.aux.xml) instead.
bool stampGdalGeoTransform(const QString& imagePath, const Affine& a) {
  if (!a.valid || imagePath.isEmpty()) return false;
  GDALAllRegister();
  GDALDatasetH ds = GDALOpen(qUtf8Printable(imagePath), GA_Update);
  if (!ds)
    ds = GDALOpen(qUtf8Printable(imagePath), GA_ReadOnly);
  if (!ds) return false;
  double gt[6] = {a.c, a.a, a.b, a.f, a.d, a.e};
  const CPLErr err = GDALSetGeoTransform(ds, gt);
  GDALClose(ds);
  return err == CE_None;
}

// Only a PAM file that carries georeferencing overrides the world file; statistics and
// metadata stay. The aligner backs every sidecar up (GeorefBackup) before this runs.
void dropGdalPamSidecar(const QString& imagePath) {
  for (const QString& p : GeorefBackup::pamSidecarPaths(imagePath)) {
    if (GeorefBackup::pamCarriesGeoref(p)) QFile::remove(p);
  }
}

bool applyWorldFileToRaster(QgsRasterLayer* layer, const Affine& a,
                            const QgsCoordinateReferenceSystem& crs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("그림 레이어가 없습니다");
    return false;
  }
  const QString src = layer->source();
  // The provider's open GDAL handle re-serialises the PAM state it loaded (QGIS put the
  // band statistics it computed into it) when it closes. Close it before the sidecars
  // change: otherwise that flush lands on top of them, and a PAM GeoTransform written back
  // that way overrides the new world file (a GeoTIFF reads PAM first, so it would not move
  // at all). Probed against qgis-dev 3.44; test_georef_backup covers PNG and TIFF.
  GeorefBackup::releaseRasterHandle(layer);
  if (!writeWorldFile(src, a, errorOut)) return false;
  if (crs.isValid()) writeSidecarPrj(src, crs, nullptr);
  dropGdalPamSidecar(src);
  // GDAL ignores a world file when a GeoTIFF has its own georeferencing; only then are the
  // file's tags replaced (backed up first). Plain scans, JPG and PNG keep their bytes and
  // follow the world file alone, so 「되돌리기」 only has to put the sidecars back.
  if (GeorefBackup::hasInternalGeoref(src)) stampGdalGeoTransform(src, a);
  // Same layer object, same id: only its data source is read again. A handle released
  // during that reopen may still have flushed a georeferencing PAM: once more.
  GeorefBackup::reopenRasterLayer(layer, crs);
  if (GeorefBackup::pamGeorefPresent(src)) {
    dropGdalPamSidecar(src);
    GeorefBackup::reopenRasterLayer(layer, crs);
  }
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("맞춰진 그림을 다시 열지 못했습니다");
    return false;
  }
  LayerOps::setAlignPending(layer, false);
  if (QgsProject* proj = QgsProject::instance()) {
    if (QgsLayerTree* root = proj->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = root->findLayer(layer->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  layer->triggerRepaint();
  return true;
}

bool persistAlignedRaster(QgsRasterLayer* layer, const Affine& a,
                          const QgsCoordinateReferenceSystem& crs, QString* errorOut) {
  if (!applyWorldFileToRaster(layer, a, crs, errorOut)) return false;
  if (looksUnreferencedRaster(layer)) {
    if (errorOut)
      *errorOut = QStringLiteral("그림을 지도 좌표로 붙이지 못했습니다. 점을 다시 찍고 이동하세요.");
    return false;
  }
  return true;
}

bool mustRebuildRasterAfterWorldFile(const QgsRasterLayer* layer) {
  return looksUnreferencedRaster(layer);
}

bool transformGeometry(QgsGeometry* geom, const Affine& a) {
  if (!geom || geom->isEmpty() || !a.valid) return false;
  const Qgis::GeometryOperationResult r = geom->transform(toQTransform(a));
  return r == Qgis::GeometryOperationResult::Success;
}

bool applyAffineToVector(QgsVectorLayer* layer, const Affine& a,
                         const QHash<qint64, QgsGeometry>& originals,
                         const QgsCoordinateReferenceSystem& destCrs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("도면 레이어가 없습니다");
    return false;
  }
  if (!a.valid) {
    if (errorOut) *errorOut = QStringLiteral("변환이 없습니다");
    return false;
  }
  if (!layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("도면을 고칠 수 없습니다");
    return false;
  }
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    QgsGeometry g = originals.value(f.id());
    if (g.isEmpty()) g = f.geometry();
    if (g.isEmpty()) continue;
    if (!transformGeometry(&g, a)) {
      layer->rollBack();
      if (errorOut) *errorOut = QStringLiteral("도형 변환 실패");
      return false;
    }
    layer->changeGeometry(f.id(), g);
  }
  if (!layer->commitChanges()) {
    layer->rollBack();
    if (errorOut) *errorOut = QStringLiteral("도면 저장 실패");
    return false;
  }
  if (destCrs.isValid()) layer->setCrs(destCrs);
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* cloneToMemory(QgsVectorLayer* src, const QString& name, QString* errorOut) {
  if (!src || !src->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("원본 도면이 없습니다");
    return nullptr;
  }
  auto makeMem = [&](const QString& geomName) -> QgsVectorLayer* {
    const QString crs = src->crs().isValid() ? src->crs().authid() : QString();
    QString uri = geomName;
    if (!crs.isEmpty()) uri += QStringLiteral("?crs=%1").arg(crs);
    auto* mem = new QgsVectorLayer(uri, name, QStringLiteral("memory"));
    if (!mem->isValid()) {
      delete mem;
      return nullptr;
    }
    QList<QgsField> add;
    const QgsFields fields = src->fields();
    for (int i = 0; i < fields.size(); ++i) add.append(fields.at(i));
    if (!add.isEmpty()) {
      mem->dataProvider()->addAttributes(add);
      mem->updateFields();
    }
    return mem;
  };

  QString geom = QgsWkbTypes::displayString(src->wkbType());
  if (geom.isEmpty() || geom.contains(QLatin1String("Unknown"), Qt::CaseInsensitive))
    geom = QStringLiteral("LineString");

  auto fill = [&](QgsVectorLayer* mem, bool forceLine) -> int {
    QgsFeatureIterator it = src->getFeatures();
    QgsFeature f;
    QgsFeatureList copied;
    while (it.nextFeature(f)) {
      QgsGeometry g = f.geometry();
      if (g.isEmpty()) continue;
      if (forceLine && g.type() != Qgis::GeometryType::Line)
        g = g.convertToType(Qgis::GeometryType::Line, true);
      else if (g.type() != mem->geometryType())
        g = g.convertToType(mem->geometryType(), true);
      if (g.isEmpty()) continue;
      QgsFeature nf(mem->fields());
      nf.setGeometry(g);
      const QgsFields sf = src->fields();
      for (int i = 0; i < sf.size(); ++i) {
        const int di = mem->fields().indexOf(sf.at(i).name());
        if (di >= 0) nf.setAttribute(di, f.attribute(i));
      }
      copied.append(nf);
    }
    if (copied.isEmpty()) return 0;
    mem->dataProvider()->addFeatures(copied);
    return static_cast<int>(copied.size());
  };

  QgsVectorLayer* mem = makeMem(geom);
  if (!mem) {
    if (errorOut) *errorOut = QStringLiteral("맞춤 도면을 만들지 못했습니다");
    return nullptr;
  }
  int n = fill(mem, false);
  if (n <= 0) {
    delete mem;
    mem = makeMem(QStringLiteral("LineString"));
    if (!mem) {
      if (errorOut) *errorOut = QStringLiteral("맞춤 도면을 만들지 못했습니다");
      return nullptr;
    }
    n = fill(mem, true);
  }
  if (n <= 0) {
    delete mem;
    if (errorOut)
      *errorOut = QStringLiteral("CAD에서 선을 읽지 못했습니다. DXF로 저장한 뒤 다시 열어 보세요.");
    return nullptr;
  }
  return mem;
}

QString saveVectorCopyGpkg(QgsVectorLayer* layer, const QString& outPath,
                           const QgsCoordinateReferenceSystem& destCrs, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("도면이 없습니다");
    return {};
  }
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("GPKG");
  opts.fileEncoding = QStringLiteral("UTF-8");
  if (destCrs.isValid() && layer->crs().isValid() && destCrs != layer->crs()) {
    opts.ct = QgsCoordinateTransform(layer->crs(), destCrs, QgsCoordinateTransformContext());
  }
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, outPath, QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("GPKG 저장 실패") : err;
    return {};
  }
  return outPath;
}

bool isDomainSurveyLayer(const QgsMapLayer* layer) {
  const QString key = LayerOps::layerKeyOf(layer);
  return key == QLatin1String("survey_area") || key == QLatin1String("feature_poly")
         || key == QLatin1String("feature_line") || key == QLatin1String("section_line")
         || key == QLatin1String("control_points") || key == QLatin1String("artifact_point")
         || key == QLatin1String("trial_trench");
}

bool isAlignableLayer(const QgsMapLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (isDomainSurveyLayer(layer)) return false;
  if (const auto* rl = qobject_cast<const QgsRasterLayer*>(layer))
    return rl->providerType() == QLatin1String("gdal");
  if (const auto* vl = qobject_cast<const QgsVectorLayer*>(layer)) {
    const QString p = vl->providerType();
    return p == QLatin1String("ogr") || p == QLatin1String("memory");
  }
  return false;
}

bool isImagePath(const QString& path) {
  const QString low = path.toLower();
  return low.endsWith(QLatin1String(".jpg")) || low.endsWith(QLatin1String(".jpeg"))
         || low.endsWith(QLatin1String(".png")) || low.endsWith(QLatin1String(".tif"))
         || low.endsWith(QLatin1String(".tiff")) || low.endsWith(QLatin1String(".gtiff"));
}

bool isCadPath(const QString& path) {
  const QString low = path.toLower();
  return low.endsWith(QLatin1String(".dxf")) || low.endsWith(QLatin1String(".dwg"));
}

bool looksLikePaperScan(const QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  QgsRasterDataProvider* p = const_cast<QgsRasterLayer*>(layer)->dataProvider();
  if (p && p->bandCount() >= 1) {
    const int w = std::min(std::max(layer->width(), 1), 32);
    const int h = std::min(std::max(layer->height(), 1), 32);
    if (w >= 2 && h >= 2) {
      std::unique_ptr<QgsRasterBlock> r(p->block(1, layer->extent(), w, h));
      std::unique_ptr<QgsRasterBlock> g(
          p->bandCount() >= 2 ? p->block(2, layer->extent(), w, h) : nullptr);
      std::unique_ptr<QgsRasterBlock> b(
          p->bandCount() >= 3 ? p->block(3, layer->extent(), w, h) : nullptr);
      if (r && r->isValid()) {
        int white = 0;
        int n = 0;
        for (int y = 0; y < h; ++y) {
          for (int x = 0; x < w; ++x) {
            if (r->isNoData(x, y)) continue;
            const double rv = r->value(x, y);
            const double gv = (g && g->isValid()) ? g->value(x, y) : rv;
            const double bv = (b && b->isValid()) ? b->value(x, y) : rv;
            ++n;
            if (rv >= 236.0 && gv >= 236.0 && bv >= 236.0)
              ++white;
          }
        }
        if (n > 0) return (white * 100 / n) >= 42;
      }
    }
  }
  const QString src = layer->source().toLower();
  if (src.contains(QLatin1String(".tif"))) return false;
  return src.contains(QLatin1String(".jpg")) || src.contains(QLatin1String(".jpeg"))
         || src.contains(QLatin1String(".png")) || src.contains(QLatin1String(".bmp"))
         || src.contains(QLatin1String(".gif"));
}

void styleAlignedInkScan(QgsRasterLayer* layer) {
  layer->setOpacity(1.0);
  layer->setBlendMode(QPainter::CompositionMode_Multiply);
  layer->setResamplingStage(Qgis::RasterResamplingStage::Provider);
  if (QgsRasterDataProvider* p = layer->dataProvider()) {
    p->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    p->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
  }
  if (QgsBrightnessContrastFilter* bf = layer->brightnessFilter()) {
    bf->setContrast(55);
    bf->setBrightness(-28);
    bf->setGamma(0.75);
  }
  QgsRasterRenderer* rend = layer->renderer();
  if (!rend) return;
  auto* tr = new QgsRasterTransparency();
  QVector<QgsRasterTransparency::TransparentThreeValuePixel> rgb;
  rgb.append(QgsRasterTransparency::TransparentThreeValuePixel(255, 255, 255, 0.0, 8, 8, 8));
  tr->setTransparentThreeValuePixelList(rgb);
  rend->setRasterTransparency(tr);
}

void styleAlignedColorRaster(QgsRasterLayer* layer) {
  // 항공 GeoTIFF 등은 먹선 필터를 쓰면 전혀 다른 색으로 이동한다.
  layer->setOpacity(1.0);
  layer->setBlendMode(QPainter::CompositionMode_SourceOver);
  layer->setResamplingStage(Qgis::RasterResamplingStage::Provider);
  if (QgsRasterDataProvider* p = layer->dataProvider()) {
    p->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
    p->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Bilinear);
  }
  if (QgsBrightnessContrastFilter* bf = layer->brightnessFilter()) {
    bf->setContrast(0);
    bf->setBrightness(0);
    bf->setGamma(1.0);
  }
  if (QgsRasterRenderer* rend = layer->renderer())
    rend->setRasterTransparency(new QgsRasterTransparency());
}

void styleAlignedRasterOverlay(QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return;
  if (looksLikePaperScan(layer))
    styleAlignedInkScan(layer);
  else
    styleAlignedColorRaster(layer);
  layer->triggerRepaint();
}

bool looksUnreferencedRaster(const QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return true;
  const int pw = layer->width();
  const int ph = layer->height();
  if (pw < 2 || ph < 2) return true;
  const QgsRectangle e = layer->extent();
  if (e.isEmpty() || !e.isFinite()) return true;
  // 픽셀 평면(0..폭, 0..높이)에만 앉은 그림. 월드파일·내장 GT가 없어도
  // 여기 해당하면 맞추기 대상이다. 내장 GT가 지도 좌표면 참조된 것이다
  // (사이드카 .tfw가 없다고 맞추기 대기로 숨기면 안 된다).
  if (e.xMinimum() > -2.0 && e.yMinimum() > -2.0 && e.xMaximum() < pw + 2.0
      && e.yMaximum() < ph + 2.0)
    return true;
  // Size ≈ pixels is only a raw scan when the box is still near the pixel origin.
  // 1 m/pixel on EPSG:5186 must not be treated as unreferenced.
  if (std::abs(e.width() - pw) < 2.0 && std::abs(e.height() - ph) < 2.0
      && e.xMinimum() < pw + 10.0 && e.yMinimum() < ph + 10.0)
    return true;
  return false;
}

}  // namespace GeorefService
