#include "CadDrawingReader.h"

#include <QFile>
#include <QScopeGuard>
#include <QStringDecoder>

#include <algorithm>
#include <cmath>
#include <memory>

#include <cpl_error.h>
#include <gdal_priv.h>
#include <ogr_featurestyle.h>
#include <ogrsf_frmts.h>

#include <qgsabstractgeometry.h>
#include <qgsgeometrycollection.h>
#include <qgswkbtypes.h>

namespace CadDrawingReader {
namespace {

constexpr int kCancelEvery = 256;  // 몇 도형마다 취소를 볼지

void CPL_STDCALL countWarnings(CPLErr level, CPLErrorNum, const char*) {
  if (level != CE_Warning) return;
  if (auto* count = static_cast<int*>(CPLGetErrorHandlerUserData())) ++*count;
}

std::unique_ptr<OGRStyleTool> partOf(OGRStyleMgr& manager, int index) {
  return std::unique_ptr<OGRStyleTool>(manager.GetPart(index));
}

// 글자 스타일 LABEL 의 크기(지상 단위)·각도·기준점. dx·dy 는 읽지 않는다(LibreDWG 출력에서 엉뚱한 값이다).
struct Label {
  double size = 0;
  double angle = 0;
  int anchor = 1;
};

Label labelOf(const QByteArray& style) {
  Label label;
  if (style.isEmpty()) return label;
  OGRStyleMgr manager;
  if (!manager.InitStyleString(style.constData())) return label;
  for (int i = 0; i < manager.GetPartCount(); ++i) {
    std::unique_ptr<OGRStyleTool> tool = partOf(manager, i);
    if (!tool || tool->GetType() != OGRSTCLabel) continue;
    auto* text = static_cast<OGRStyleLabel*>(tool.get());
    text->SetUnit(OGRSTUGround, 1.0);
    GBool missing = FALSE;
    const double size = text->Size(missing);
    if (!missing) label.size = size;
    const double angle = text->Angle(missing);
    if (!missing) label.angle = angle;
    const int anchor = text->Anchor(missing);
    if (!missing && anchor >= 1 && anchor <= 12) label.anchor = anchor;
    break;
  }
  return label;
}

// 모음·여러 조각은 끝까지 풀어 2D 한 조각씩 담는다(블록 안의 블록은 모음 안의 모음으로 온다).
// 곡선은 직선 조각으로 바꾼다.
void addSingleParts(const QgsAbstractGeometry* geometry, QVector<QgsGeometry>* parts) {
  if (const auto* collection = qgsgeometry_cast<const QgsGeometryCollection*>(geometry)) {
    for (int i = 0; i < collection->numGeometries(); ++i) addSingleParts(collection->geometryN(i), parts);
    return;
  }
  QgsGeometry part(QgsWkbTypes::isCurvedType(geometry->wkbType()) ? geometry->segmentize() : geometry->clone());
  part.get()->dropZValue();
  part.get()->dropMValue();
  *parts << part;
}

QVector<QgsGeometry> singleParts(const OGRGeometry* source) {
  QVector<QgsGeometry> parts;
  QByteArray wkb(int(source->WkbSize()), Qt::Uninitialized);
  if (source->exportToWkb(wkbNDR, reinterpret_cast<unsigned char*>(wkb.data()), wkbVariantIso) != OGRERR_NONE)
    return parts;
  QgsGeometry geometry;
  geometry.fromWkb(wkb);
  if (!geometry.isNull()) addSingleParts(geometry.constGet(), &parts);
  return parts;
}

}  // namespace

bool looksUtf8(const QByteArray& bytes) {
  if (std::none_of(bytes.cbegin(), bytes.cend(), [](char c) { return static_cast<unsigned char>(c) > 0x7F; }))
    return false;
  QStringDecoder decoder(QStringDecoder::Utf8);
  const QString decoded = decoder(bytes);
  Q_UNUSED(decoded);
  return !decoder.hasError();
}

QColor colorFromStyle(const QString& ogrStyle) {
  const QColor dark(QStringLiteral("#3C3C3C"));
  const QColor none(QStringLiteral("#000000"));
  if (ogrStyle.isEmpty()) return none;
  OGRStyleMgr manager;
  const QByteArray style = ogrStyle.toUtf8();
  if (!manager.InitStyleString(style.constData())) return none;
  for (int i = 0; i < manager.GetPartCount(); ++i) {
    std::unique_ptr<OGRStyleTool> tool = partOf(manager, i);
    if (!tool) continue;
    GBool missing = TRUE;
    const char* colour = nullptr;
    switch (tool->GetType()) {
      case OGRSTCPen: colour = static_cast<OGRStylePen*>(tool.get())->Color(missing); break;
      case OGRSTCBrush: colour = static_cast<OGRStyleBrush*>(tool.get())->ForeColor(missing); break;
      case OGRSTCLabel: colour = static_cast<OGRStyleLabel*>(tool.get())->ForeColor(missing); break;
      case OGRSTCSymbol: colour = static_cast<OGRStyleSymbol*>(tool.get())->Color(missing); break;
      default: break;
    }
    int r = 0, g = 0, b = 0, alpha = 255;
    if (missing || !colour || !OGRStyleTool::GetRGBFromString(colour, r, g, b, alpha)) continue;
    const double luminance = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    return luminance >= 0.9 ? dark : QColor(r, g, b);
  }
  return none;
}

QgsRectangle robustExtent(const QVector<QgsPointXY>& centres) {
  if (centres.isEmpty()) return {};
  QVector<double> xs, ys;
  xs.reserve(centres.size());
  ys.reserve(centres.size());
  for (const QgsPointXY& p : centres) {
    xs << p.x();
    ys << p.y();
  }
  std::sort(xs.begin(), xs.end());
  std::sort(ys.begin(), ys.end());
  const double last = double(centres.size() - 1);
  const int lo = int(std::floor(0.05 * last));
  const int hi = int(std::ceil(0.95 * last));
  return QgsRectangle(xs[lo], ys[lo], xs[hi], ys[hi]);
}

bool read(const QString& dxfPath, CadDrawing* out, QString* error, QString* details,
          const std::function<bool()>& canceled) {
  const auto fail = [&](const QString& what, const QString& why = QString()) {
    if (error) *error = what;
    if (details) *details = why;
    return false;
  };
  CadDrawing drawing;
  {
    QFile file(dxfPath);
    if (file.open(QIODevice::ReadOnly)) drawing.readAsUtf8 = looksUtf8(file.readAll());
  }
  CPLPushErrorHandlerEx(countWarnings, &drawing.gdalWarnings);
  const auto popHandler = qScopeGuard([] { CPLPopErrorHandler(); });
  CPLErrorReset();
  const char* drivers[] = {"DXF", nullptr};
  const char* utf8[] = {"ENCODING=UTF-8", nullptr};
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
      static_cast<GDALDataset*>(GDALOpenEx(dxfPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers,
                                           drawing.readAsUtf8 ? utf8 : nullptr, nullptr)),
      GDALClose);
  if (!dataset) return fail(QStringLiteral("DXF를 열지 못했습니다."), QString::fromUtf8(CPLGetLastErrorMsg()));
  OGRLayer* layer = dataset->GetLayerByName("entities");
  if (!layer && dataset->GetLayerCount() > 0) layer = dataset->GetLayer(0);
  if (!layer) return fail(QStringLiteral("DXF를 열지 못했습니다."), QStringLiteral("entities 레이어가 없습니다."));
  const OGRFeatureDefn* definition = layer->GetLayerDefn();
  const int paperField = definition->GetFieldIndex("PaperSpace");
  const int layerField = definition->GetFieldIndex("Layer");
  const int textField = definition->GetFieldIndex("Text");

  QVector<QgsPointXY> centres;
  int seen = 0;
  layer->ResetReading();
  for (;;) {
    if (seen++ % kCancelEvery == 0 && canceled && canceled())
      return fail(QStringLiteral("도면 읽기를 취소했습니다."));
    std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(layer->GetNextFeature(),
                                                                                 OGRFeature::DestroyFeature);
    if (!feature) break;
    if (paperField >= 0 && feature->IsFieldSetAndNotNull(paperField) && feature->GetFieldAsInteger(paperField) == 1) {
      ++drawing.paperSpaceSkipped;
      continue;
    }
    const OGRGeometry* geometry = feature->GetGeometryRef();
    if (!geometry || geometry->IsEmpty()) continue;
    const QByteArray style = feature->GetStyleString() ? QByteArray(feature->GetStyleString()) : QByteArray();
    const QString text = textField >= 0 ? QString::fromUtf8(feature->GetFieldAsString(textField)) : QString();
    CadEntity base;
    base.cadLayer = layerField >= 0 ? QString::fromUtf8(feature->GetFieldAsString(layerField)) : QString();
    base.color = colorFromStyle(QString::fromUtf8(style));
    for (const QgsGeometry& part : singleParts(geometry)) {
      CadEntity entity = base;
      entity.geometry = part;
      switch (QgsWkbTypes::geometryType(part.wkbType())) {
        case Qgis::GeometryType::Point:
          if (text.isEmpty()) {
            entity.kind = CadKind::Point;
          } else {
            const Label label = labelOf(style);
            entity.kind = CadKind::Text;
            entity.text = text;
            entity.textHeight = label.size;
            entity.textAngle = label.angle;
            entity.textAnchor = label.anchor;
          }
          break;
        case Qgis::GeometryType::Line: entity.kind = CadKind::Line; break;
        case Qgis::GeometryType::Polygon: entity.kind = CadKind::Fill; break;
        default: continue;
      }
      centres << entity.geometry.boundingBox().center();
      drawing.entities << entity;
    }
  }
  if (drawing.entities.isEmpty())
    return fail(QStringLiteral("도면에 모형 공간 도형이 없습니다."),
                drawing.paperSpaceSkipped > 0
                    ? QStringLiteral("종이 공간 도형 %1개만 있습니다.").arg(drawing.paperSpaceSkipped)
                    : QString());
  drawing.robustExtent = robustExtent(centres);
  if (out) *out = drawing;
  return true;
}

}  // namespace CadDrawingReader
