#include "KaSessionLog.h"
#include "LayerOps.h"
#include "LayerOpsInternal.h"
#include "LayerLabelControls.h"
#include "HeritageStyle.h"
#include "DemPresentation.h"
#include "DemColorRampLegend.h"
#include <QSignalBlocker>
#include "GeorefService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"
#include "KaPortableRuntime.h"
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <memory>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QStandardPaths>
#include <QDomDocument>
#include <QUrlQuery>
#include <QSet>
#include <QTemporaryFile>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QHash>
#include <functional>

#include <qgis.h>
#include <QUndoStack>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsmapcanvas.h>
#include <qgssettings.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_api.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsdataprovider.h>
#include <qgsprojectviewsettings.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
#include <qgsreferencedgeometry.h>
#include <QNetworkRequest>

QString LayerOps::reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                       const QString& outPath, QgsProject* project, QString* errorOut,
                                       bool addToMap) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  const QgsCoordinateReferenceSystem dest(targetCrsAuthId);
  if (!dest.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid target CRS");
    return {};
  }
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = outPath.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)
                        ? QStringLiteral("GPKG")
                        : QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  opts.ct = QgsCoordinateTransform(layer->crs(), dest, project ? project->transformContext() : QgsCoordinateTransformContext());
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, outPath, project ? project->transformContext() : QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("reproject write failed") : err;
    return {};
  }
  if (addToMap && project) {
    auto* vl = new QgsVectorLayer(outPath, layer->name() + QStringLiteral("_") + targetCrsAuthId, QStringLiteral("ogr"));
    if (vl->isValid()) {
      vl->setCrs(dest);
      project->addMapLayer(vl);
    } else {
      delete vl;
    }
  }
  return outPath;
}

int LayerOps::ensureControlPointQualityFields(QgsVectorLayer* controlPoints) {
  if (!controlPoints || !controlPoints->isValid()) return 0;
  int added = 0;
  QgsFields fields = controlPoints->fields();
  auto ensure = [&](const QString& name, QMetaType::Type t) {
    if (fields.indexOf(name) < 0) {
      if (!controlPoints->isEditable()) controlPoints->startEditing();
      if (controlPoints->dataProvider()->addAttributes({QgsField(name, t)})) {
        ++added;
      }
    }
  };
  ensure(QStringLiteral("accuracy_m"), QMetaType::Type::Double);
  ensure(QStringLiteral("pdop"), QMetaType::Type::Double);
  ensure(QStringLiteral("fix_type"), QMetaType::Type::QString);
  ensure(QStringLiteral("pixel_x"), QMetaType::Type::Double);
  ensure(QStringLiteral("pixel_y"), QMetaType::Type::Double);
  if (added > 0) {
    controlPoints->updateFields();
    if (controlPoints->isEditable()) controlPoints->commitChanges();
  }
  return added;
}

bool LayerOps::applyDomainDrawStyle(QgsVectorLayer* layer, const QString& layerKeyIn) {
  if (!layer || !layer->isValid()) return false;
  if (isAdminEmdLayer(layer)) return true;
  const QString key = layerKeyIn.isEmpty() ? layerKeyOf(layer) : layerKeyIn;
  const Qgis::GeometryType gt = layer->geometryType();

  QColor fill(37, 99, 235, 90);
  QColor stroke(37, 99, 235, 255);
  double strokeW = 1.2;
  double markerSize = 3.5;

  if (key == QLatin1String("survey_area")) {
    fill = QColor(180, 83, 9, 70);
    stroke = QColor(146, 64, 14, 255);
    strokeW = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    fill = QColor(22, 163, 74, 90);
    stroke = QColor(17, 94, 44, 255);
    strokeW = 1.8;
  } else if (key == QLatin1String("feature_line") || key == QLatin1String("section_line")) {
    stroke = key == QLatin1String("section_line") ? QColor(190, 24, 93, 255) : QColor(202, 138, 4, 255);
    strokeW = 1.8;
  } else if (key == QLatin1String("control_points")) {
    fill = QColor(234, 179, 8, 255);
    stroke = QColor(161, 98, 7, 255);
    markerSize = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    fill = QColor(185, 28, 28, 255);
    stroke = QColor(127, 29, 29, 255);
    markerSize = 3.6;
  } else if (key == QLatin1String("trial_trench")) {
    // 시굴 트렌치 도면 관례: 붉은 외곽선 0.5, 채움은 거의 없음(위성·지적 위 판독).
    fill = QColor(220, 38, 38, 18);
    stroke = QColor(220, 38, 38, 255);
    strokeW = 0.5;
  }

  const QString savedStroke = layer->customProperty(QStringLiteral("ka_hgis/style_stroke")).toString();
  const QString savedFill = layer->customProperty(QStringLiteral("ka_hgis/style_fill")).toString();
  const double savedWidth = layer->customProperty(QStringLiteral("ka_hgis/style_width_mm")).toDouble();
  if (!savedStroke.isEmpty() && QColor(savedStroke).isValid()) {
    stroke = QColor(savedStroke);
  }
  if (!savedFill.isEmpty() && QColor(savedFill).isValid()) {
    fill = QColor(savedFill);
  }
  if (savedWidth > 0.05) {
    strokeW = savedWidth;
  }

  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    auto fs = QgsFillSymbol::createSimple({
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(strokeW)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
    });
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    if (key == QLatin1String("section_line")) {
      auto ls = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), QStringLiteral("#FFFFFF")},
          {QStringLiteral("line_width"), QStringLiteral("3.0")},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
          {QStringLiteral("line_style"), QStringLiteral("solid")},
      });
      auto* core = new QgsSimpleLineSymbolLayer(stroke, strokeW, Qt::SolidLine);
      core->setWidthUnit(Qgis::RenderUnit::Millimeters);
      ls->appendSymbolLayer(core);
      sym = ls.release();
    } else {
      auto ls = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
          {QStringLiteral("line_width"), QString::number(strokeW)},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
      });
      sym = ls.release();
    }
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QStringLiteral("0.6")},
        {QStringLiteral("size"), QString::number(markerSize)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    sym = QgsSymbol::defaultSymbol(gt);
    if (sym)
      sym->setColor(stroke);
  }
  if (!sym) return false;
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeW);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSize);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  // Drawing and attribute edits also call this function. Initialize labels
  // only once so a later edit cannot reset size, content or visibility.
  if (!layer->labeling()) {
    if (key == QLatin1String("trial_trench"))
      applyNameAttributeLabels(layer, QStringLiteral("name"), 5.0, true);
    else if (gt == Qgis::GeometryType::Polygon)
      applyAreaM2Labels(layer);
    else if (const QString field = detectNameField(layer); !field.isEmpty())
      applyNameAttributeLabels(layer, field, 5.0, false);
  }
  layer->triggerRepaint();
  return true;
}

QString LayerOps::detectNameField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const QgsFields fds = layer->fields();
  if (fds.isEmpty()) return {};

  // 1. Cultural heritage & Archaeology (문화유적분포지도, 발굴/지표조사구역, 유적, 고고학)
  const QStringList heritageCandidates = {
    QStringLiteral("사업명"), QStringLiteral("유적명"), QStringLiteral("유적명칭"),
    QStringLiteral("조사명"), QStringLiteral("보고서명"), QStringLiteral("소재지"),
    QStringLiteral("번호"), QStringLiteral("유적번호"), QStringLiteral("site_no"),
    QStringLiteral("yujuk_nm"), QStringLiteral("yujeok_nm"), QStringLiteral("site_name"),
    QStringLiteral("hist_nm"), QStringLiteral("rem_nm"), QStringLiteral("명칭"),
    QStringLiteral("name"), QStringLiteral("title")
  };
  for (const QString& c : heritageCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 2. Cadastral (연속지적도, 지적도, 토지)
  const QStringList cadCandidates = {
    QStringLiteral("jibun"), QStringLiteral("지번"), QStringLiteral("a2"),
    QStringLiteral("a1"), QStringLiteral("pnu")
  };
  for (const QString& c : cadCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 3. Archaeology features (유구, 도면)
  const QStringList featCandidates = {
    QStringLiteral("feature_no"), QStringLiteral("유구번호"), QStringLiteral("유구명"),
    QStringLiteral("호수"), QStringLiteral("kind_ko"), QStringLiteral("kind")
  };
  for (const QString& c : featCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 4. Digital topo / buildings / roads (수치지형도, 건물, 도로, 지명)
  const QStringList topoCandidates = {
    QStringLiteral("buld_nm"), QStringLiteral("건물명"), QStringLiteral("지명"),
    QStringLiteral("road_nm"), QStringLiteral("도로명"), QStringLiteral("label"),
    QStringLiteral("kor_nm")
  };
  for (const QString& c : topoCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 5. Scan string fields containing name-related keywords
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    const QString n = f.name().toLower();
    if (f.type() == QMetaType::QString || f.typeName().contains(QLatin1String("char"), Qt::CaseInsensitive) ||
        f.typeName().contains(QLatin1String("string"), Qt::CaseInsensitive)) {
      if (n.contains(QStringLiteral("명")) || n.contains(QLatin1String("name")) ||
          n.contains(QLatin1String("title")) || n.contains(QLatin1String("label")) ||
          n.contains(QStringLiteral("지번")) || n.contains(QLatin1String("jibun"))) {
        return f.name();
      }
    }
  }

  // 6. First string field if available
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    if (f.type() == QMetaType::QString)
      return f.name();
  }

  return fds.at(0).name();
}

LayerOps::FeatureFormFields LayerOps::featureFormFields(const QgsVectorLayer* layer) {
  FeatureFormFields out;
  if (!layer || !layer->isValid()) return out;
  const QgsFields fds = layer->fields();
  const auto firstOf = [&](const QStringList& names) {
    for (const QString& name : names) {
      const int idx = fds.indexOf(name);
      if (idx >= 0) return idx;
    }
    return -1;
  };
  out.numberIndex = firstOf({QStringLiteral("feature_no"), QStringLiteral("artifact_no"),
                             QStringLiteral("section_id"), QStringLiteral("point_id")});
  out.nameIndex = firstOf({QStringLiteral("survey_name"), QStringLiteral("site_name"),
                           QStringLiteral("name"), QStringLiteral("kind")});
  if (out.nameIndex >= 0 && out.nameIndex == out.numberIndex) out.nameIndex = -1;
  if (out.nameIndex >= 0) out.nameField = fds.at(out.nameIndex).name();
  if (out.numberIndex >= 0) out.numberField = fds.at(out.numberIndex).name();
  return out;
}

bool LayerOps::applyFeatureFormValues(QgsVectorLayer* layer, qint64 featureId, const QString& name,
                                      const QString& number, QString* errorOut) {
  const auto fail = [&](const QString& text) {
    if (errorOut) *errorOut = text;
    return false;
  };
  if (!layer || !layer->isValid()) return fail(QStringLiteral("레이어가 없습니다."));
  if (isCadastralLayer(layer)) return fail(QStringLiteral("지적도는 고칠 수 없습니다."));
  if (isReferenceLayer(layer)) return fail(QStringLiteral("참조 지도는 고칠 수 없습니다."));
  const FeatureFormFields fields = featureFormFields(layer);
  if (fields.nameIndex < 0 && fields.numberIndex < 0)
    return fail(QStringLiteral("이름·번호 필드가 없습니다."));
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid()) return fail(QStringLiteral("도형을 찾지 못했습니다."));
  if (!layer->isEditable() && !layer->startEditing())
    return fail(QStringLiteral("편집을 열 수 없습니다."));
  if (fields.nameIndex >= 0 && !layer->changeAttributeValue(fid, fields.nameIndex, name))
    return fail(QStringLiteral("이름을 쓰지 못했습니다."));
  if (fields.numberIndex >= 0 && !layer->changeAttributeValue(fid, fields.numberIndex, number))
    return fail(QStringLiteral("번호를 쓰지 못했습니다."));
  layer->triggerRepaint();
  return true;
}

QString LayerOps::prepareShapefileEncoding(const QString& shpPath) {
  QFileInfo fi(shpPath);
  if (!fi.exists() || fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) != 0)
    return QString();

  const QString dir = fi.absolutePath();
  const QString base = fi.completeBaseName();
  const QString cpgPath = QDir(dir).filePath(base + QStringLiteral(".cpg"));
  const QString dbfPath = QDir(dir).filePath(base + QStringLiteral(".dbf"));

  // 1. 이미 .cpg 파일이 존재하는 경우 해당 인코딩을 GDAL에 적용
  if (QFile::exists(cpgPath)) {
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString content = QString::fromUtf8(cpgFile.readAll()).trimmed();
      cpgFile.close();
      if (!content.isEmpty()) {
        const QString upper = content.toUpper();
        if (upper.contains(QLatin1String("UTF-8")) || upper.contains(QLatin1String("UTF8"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
          return QStringLiteral("UTF-8");
        }
        if (upper.contains(QLatin1String("CP949")) || upper.contains(QLatin1String("949")) ||
            upper.contains(QLatin1String("EUC-KR")) || upper.contains(QLatin1String("ANSI")) ||
            upper.contains(QLatin1String("SYSTEM"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "CP949");
          return QStringLiteral("CP949");
        }
        CPLSetConfigOption("SHAPE_ENCODING", content.toLatin1().constData());
        return content;
      }
    }
  }

  // 2. .cpg 파일이 없는 경우: .dbf 앞부분(16KB)을 검사하여 UTF-8 vs CP949 자동 판별
  QString detected = QStringLiteral("CP949"); // 한국 공공데이터·지적도·유적도 SHP 기본값
  if (QFile::exists(dbfPath)) {
    QFile dbfFile(dbfPath);
    if (dbfFile.open(QIODevice::ReadOnly)) {
      const QByteArray sample = dbfFile.read(32768);
      dbfFile.close();

      bool hasNonAscii = false;
      bool validUtf8 = true;
      int utf8MultiByteCount = 0;

      const unsigned char* bytes = reinterpret_cast<const unsigned char*>(sample.constData());
      const int len = sample.size();
      for (int i = 0; i < len; ++i) {
        const unsigned char c = bytes[i];
        if (c > 127) {
          hasNonAscii = true;
          if ((c & 0xE0) == 0xC0 && c >= 0xC2) { // 2-byte UTF-8
            if (i + 1 < len && (bytes[i + 1] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              ++i;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8 (한글 완성형은 EA..ED)
            if (i + 2 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 2;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) { // 4-byte UTF-8
            if (i + 3 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80 &&
                (bytes[i + 3] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 3;
            } else {
              validUtf8 = false;
              break;
            }
          } else {
            validUtf8 = false;
            break;
          }
        }
      }

      // 비ASCII 바이트가 있고 온전한 UTF-8 멀티바이트가 충분히 존재할 때만 UTF-8
      if (hasNonAscii && validUtf8 && utf8MultiByteCount >= 4) {
        detected = QStringLiteral("UTF-8");
      } else {
        // CP949 바이트이거나 영문/숫자 헤더만 있는 경우: 한국 GIS 환경 관례상 CP949
        detected = QStringLiteral("CP949");
      }
    }
  }

  // 3. .cpg 파일이 없으면 자동 생성하여 영구 보존 (다음번 및 타 GIS 소프트웨어 호환)
  if (!QFile::exists(cpgPath)) {
    QFile outCpg(cpgPath);
    if (outCpg.open(QIODevice::WriteOnly | QIODevice::Text)) {
      outCpg.write(detected.toUtf8() + "\n");
      outCpg.close();
    }
  }

  // 4. GDAL 드라이버 레벨에서 SHAPE_ENCODING 옵션 설정
  CPLSetConfigOption("SHAPE_ENCODING", detected.toLatin1().constData());
  return detected;
}

bool LayerOps::setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding) {
  if (!layer || !layer->isValid()) return false;
  const QString src = layer->source().split(QLatin1Char('|')).first();
  QFileInfo fi(src);
  if (fi.exists() && fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0) {
    const QString cpgPath = QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QStringLiteral(".cpg"));
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      cpgFile.write(encoding.toUtf8() + "\n");
      cpgFile.close();
    }
  }
  CPLSetConfigOption("SHAPE_ENCODING", encoding.toLatin1().constData());
  layer->setProviderEncoding(encoding);
  layer->reload();
  layer->updateFields();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fillIn, const QColor& strokeIn,
                                      double strokeWidthMm, double markerSizeMm, bool noFill,
                                      bool noStroke, bool dashed) {
  if (!layer || !layer->isValid()) return false;
  QColor fill = fillIn.isValid() ? fillIn : QColor(37, 99, 235, 90);
  QColor stroke = strokeIn.isValid() ? strokeIn : QColor(37, 99, 235, 255);
  if (strokeWidthMm <= 0.0) strokeWidthMm = 1.0;
  if (markerSizeMm <= 0.0) markerSizeMm = 3.5;
  if (noFill) fill = QColor(0, 0, 0, 0);
  if (noStroke) stroke = QColor(0, 0, 0, 0);
  if (noFill && noStroke) {
    noStroke = false;
    stroke = QColor(100, 100, 100, 255);
    strokeWidthMm = 0.4;
  }

  const Qgis::GeometryType gt = layer->geometryType();
  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    QVariantMap props{
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("style"), noFill ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("outline_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    };
    auto fs = QgsFillSymbol::createSimple(props);
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    });
    sym = ls.release();
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), noFill ? QStringLiteral("#00000000") : fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), noStroke ? QStringLiteral("0") : QStringLiteral("0.6")},
        {QStringLiteral("outline_style"), noStroke ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("size"), QString::number(markerSizeMm)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    return false;
  }
  if (!sym) return false;

  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeWidthMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSizeMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_fill"), noFill);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_stroke"), noStroke);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_dashed"), dashed);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  layer->triggerRepaint();
  return true;
}

bool LayerOps::readSimpleVectorStyle(const QgsVectorLayer* layer, QColor* fill, QColor* stroke,
                                     double* strokeWidthMm, double* markerSizeMm, bool* noFill,
                                     bool* noStroke, bool* dashed) {
  if (!layer) return false;

  QColor f(37, 99, 235, 90);
  QColor s(37, 99, 235, 255);
  double w = 1.2;
  double m = 3.5;
  bool nf = false;
  bool ns = false;
  bool dash = false;

  const QString key = layerKeyOf(layer);
  if (key == QLatin1String("survey_area")) {
    f = QColor(180, 83, 9, 70);
    s = QColor(146, 64, 14, 255);
    w = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    f = QColor(22, 163, 74, 90);
    s = QColor(17, 94, 44, 255);
    w = 1.8;
  } else if (key == QLatin1String("feature_line")) {
    s = QColor(202, 138, 4, 255);
    w = 1.8;
  } else if (key == QLatin1String("section_line")) {
    s = QColor(190, 24, 93, 255);
    w = 1.8;
  } else if (key == QLatin1String("control_points")) {
    f = QColor(234, 179, 8, 255);
    s = QColor(161, 98, 7, 255);
    m = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    f = QColor(185, 28, 28, 255);
    s = QColor(127, 29, 29, 255);
    m = 3.6;
  }

  const QVariant cf = layer->customProperty(QStringLiteral("ka_hgis/style_fill"));
  const QVariant cs = layer->customProperty(QStringLiteral("ka_hgis/style_stroke"));
  const QVariant cw = layer->customProperty(QStringLiteral("ka_hgis/style_width_mm"));
  const QVariant cm = layer->customProperty(QStringLiteral("ka_hgis/style_marker_mm"));
  const QVariant cnf = layer->customProperty(QStringLiteral("ka_hgis/style_no_fill"));
  const QVariant cns = layer->customProperty(QStringLiteral("ka_hgis/style_no_stroke"));
  const QVariant cd = layer->customProperty(QStringLiteral("ka_hgis/style_dashed"));
  if (cf.isValid()) {
    const QColor parsed(cf.toString());
    if (parsed.isValid()) f = parsed;
  }
  if (cs.isValid()) {
    const QColor parsed(cs.toString());
    if (parsed.isValid()) s = parsed;
  }
  if (cw.isValid()) w = cw.toDouble();
  if (cm.isValid()) m = cm.toDouble();
  if (cnf.isValid()) nf = cnf.toBool();
  if (cns.isValid()) ns = cns.toBool();
  if (cd.isValid()) dash = cd.toBool();
  if (f.alpha() == 0) nf = true;
  if (s.alpha() == 0) ns = true;

  if (const QgsFeatureRenderer* ren = layer->renderer()) {
    if (const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(ren)) {
      if (const QgsSymbol* sym = single->symbol()) {
        if (sym->color().isValid()) {
          if (layer->geometryType() == Qgis::GeometryType::Line)
            s = sym->color();
          else if (!nf)
            f = sym->color();
        }
      }
    }
  }

  if (fill) *fill = f;
  if (stroke) *stroke = s;
  if (strokeWidthMm) *strokeWidthMm = w;
  if (markerSizeMm) *markerSizeMm = m;
  if (noFill) *noFill = nf;
  if (noStroke) *noStroke = ns;
  if (dashed) *dashed = dash;
  return true;
}

bool LayerOps::applyFeaturePolyStyle(QgsVectorLayer* featurePoly) {
  if (!featurePoly || !featurePoly->isValid()) return false;
  QString field = QStringLiteral("kind");
  if (featurePoly->fields().indexOf(field) < 0) field = QStringLiteral("period");
  if (featurePoly->fields().indexOf(field) < 0)
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QSet<QString> values;
  QgsFeatureIterator it = featurePoly->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString v = f.attribute(field).toString().trimmed();
    if (!v.isEmpty()) values.insert(v);
  }
  if (values.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QgsCategoryList cats;
  int i = 0;
  const QList<QString> sorted = values.values();
  for (const QString& v : sorted) {
    QColor c = QColor::fromHsv((i * 47) % 360, 180, 230, 160);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(featurePoly->geometryType());
    if (sym) {
      sym->setColor(c);
      cats.append(QgsRendererCategory(QVariant(v), sym, v));
    }
    ++i;
  }
  if (cats.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));
  auto* renderer = new QgsCategorizedSymbolRenderer(field, cats);
  featurePoly->setRenderer(renderer);
  featurePoly->triggerRepaint();
  return true;
}

bool LayerOps::mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return false;
  }
  const QgsFeatureIds sel = layer->selectedFeatureIds();
  if (sel.size() >= 2) {
    return mergePolygonFeatures(layer, sel, errorOut);
  }
  return mergePolygonFeatures(layer, QgsFeatureIds(), errorOut);
}

bool LayerOps::mergePolygonFeatures(QgsVectorLayer* layer, const QgsFeatureIds& featureIds, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 묶을 수 있습니다");
    return false;
  }

  QVector<QgsGeometry> geoms;
  QgsFeatureIds ids;
  QgsFeature first;
  bool hasFirst = false;
  QgsFeature f;
  const bool useSpecificIds = !featureIds.isEmpty();
  QgsFeatureIterator it = useSpecificIds ? layer->getFeatures(QgsFeatureRequest().setFilterFids(featureIds))
                                         : layer->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    QgsGeometry g = f.geometry();
    if (!g.isGeosValid())
      g = g.makeValid();
    if (g.isEmpty()) continue;
    geoms.append(g);
    ids.insert(f.id());
    if (!hasFirst) {
      first = QgsFeature(f);
      hasFirst = true;
    }
  }
  if (geoms.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("묶을 폴리곤이 2개 이상 필요합니다 (선택: %1개)").arg(geoms.size());
    return false;
  }

  QgsGeometry multi = QgsGeometry::unaryUnion(geoms);
  if (multi.isEmpty() || !multi.isGeosValid()) {
    multi = QgsGeometry::collectGeometry(geoms);
  }
  if (multi.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 결합 실패");
    return false;
  }
  if (!multi.isGeosValid())
    multi = multi.makeValid();

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }
  if (!layer->deleteFeatures(ids)) {
    if (errorOut) *errorOut = QStringLiteral("기존 피처 삭제 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  QgsFeature out(layer->fields());
  out.setAttributes(first.attributes());
  out.setGeometry(multi);
  if (!layer->addFeature(out)) {
    if (errorOut) *errorOut = QStringLiteral("결합 피처 추가 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }
  layer->triggerRepaint();
  return true;
}

bool LayerOps::explodeMultipartFeatures(QgsVectorLayer* layer, const QgsFeatureIds& featureIds, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("올바른 레이어가 아닙니다.");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 나눌 수 있습니다.");
    return false;
  }

  QgsFeatureIds targetIds = featureIds;
  if (targetIds.isEmpty()) {
    targetIds = layer->selectedFeatureIds();
  }

  QgsFeature f;
  QgsFeatureIterator it = !targetIds.isEmpty() ? layer->getFeatures(QgsFeatureRequest().setFilterFids(targetIds))
                                              : layer->getFeatures();

  int explodedPartsCount = 0;
  QgsFeatureIds toDelete;
  QVector<QgsFeature> newFeatures;

  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    QgsGeometry g = f.geometry();
    if (!g.isGeosValid()) g = g.makeValid();

    if (g.isMultipart()) {
      const QVector<QgsGeometry> parts = g.asGeometryCollection();
      if (parts.size() > 1) {
        toDelete.insert(f.id());
        for (const QgsGeometry& part : parts) {
          if (part.isEmpty()) continue;
          QgsFeature nf(layer->fields());
          nf.setAttributes(f.attributes());
          nf.setGeometry(part);
          newFeatures.append(nf);
          explodedPartsCount++;
        }
      }
    }
  }

  if (newFeatures.isEmpty() || toDelete.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("선택한 폴리곤 중 묶여 있는 그룹(멀티폴리곤)이 없습니다.\n단일 폴리곤을 나누려면 분할선을 그어 자르거나 겹친 두 도형을 선택하세요.");
    return false;
  }

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }

  if (!layer->deleteFeatures(toDelete)) {
    if (errorOut) *errorOut = QStringLiteral("기존 멀티폴리곤 삭제 실패");
    if (startedHere) layer->rollBack();
    return false;
  }

  if (!layer->addFeatures(newFeatures)) {
    if (errorOut) *errorOut = QStringLiteral("분할된 단일 폴리곤 피처 추가 실패");
    if (startedHere) layer->rollBack();
    return false;
  }

  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }

  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* LayerOps::clipLayerByBoundary(QgsVectorLayer* sourceLayer,
                                             QgsVectorLayer* boundaryLayer,
                                             QgsProject* project,
                                             QString* errorOut) {
  if (!sourceLayer || !sourceLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("자를 대상 레이어가 올바르지 않습니다.");
    return nullptr;
  }
  if (!boundaryLayer || !boundaryLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("기준 바운더리 레이어가 올바르지 않습니다.");
    return nullptr;
  }
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 유효하지 않습니다.");
    return nullptr;
  }

  // 1. 바운더리 레이어의 지오메트리를 대상 레이어 CRS로 변환 후 Union
  QgsCoordinateTransform xf;
  const bool needXf = boundaryLayer->crs().isValid() && sourceLayer->crs().isValid() &&
                     boundaryLayer->crs() != sourceLayer->crs();
  if (needXf) {
    xf = QgsCoordinateTransform(boundaryLayer->crs(), sourceLayer->crs(),
                                project ? project->transformContext()
                                        : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
  }

  QVector<QgsGeometry> bGeoms;
  QgsFeature bf;
  QgsFeatureIterator bit = boundaryLayer->getFeatures();
  while (bit.nextFeature(bf)) {
    if (!bf.hasGeometry() || bf.geometry().isEmpty()) continue;
    QgsGeometry bg = bf.geometry();
    if (needXf) {
      try {
        if (bg.transform(xf) != Qgis::GeometryOperationResult::Success) continue;
      } catch (...) {
        KaSessionLog::line(QStringLiteral("[except] core/LayerOps.cpp:1322"));
        continue;
      }
    }
    if (!bg.isGeosValid())
      bg = bg.makeValid();
    if (!bg.isEmpty())
      bGeoms.append(bg);
  }

  if (bGeoms.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리 레이어에 유효한 지오메트리가 없습니다.");
    return nullptr;
  }

  QgsGeometry boundaryUnion = QgsGeometry::unaryUnion(bGeoms);
  if (boundaryUnion.isEmpty()) {
    boundaryUnion = QgsGeometry::collectGeometry(bGeoms);
  }
  if (boundaryUnion.isEmpty() || !boundaryUnion.isGeosValid()) {
    boundaryUnion = boundaryUnion.makeValid();
  }
  if (boundaryUnion.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리 결합 지오메트리 생성에 실패했습니다.");
    return nullptr;
  }

  // 2. 결과 메모리 레이어 생성
  const QString geomTypeStr = QgsWkbTypes::displayString(sourceLayer->wkbType());
  const QString uri = QStringLiteral("%1?crs=%2").arg(geomTypeStr, sourceLayer->crs().authid());
  const QString outTitle = QStringLiteral("[클립] %1").arg(sourceLayer->name());
  auto* clippedLayer = new QgsVectorLayer(uri, outTitle, QStringLiteral("memory"));
  if (!clippedLayer || !clippedLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("클립 결과 레이어 생성 실패");
    delete clippedLayer;
    return nullptr;
  }

  clippedLayer->dataProvider()->addAttributes(sourceLayer->fields().toList());
  clippedLayer->updateFields();

  // 3. 대상 레이어 피처 순회 및 교차(Intersection) 추출
  QgsFeature sf;
  QgsFeatureIterator sit = sourceLayer->getFeatures();
  QVector<QgsFeature> outFeatures;
  while (sit.nextFeature(sf)) {
    if (!sf.hasGeometry() || sf.geometry().isEmpty()) continue;
    QgsGeometry sg = sf.geometry();
    if (!sg.isGeosValid())
      sg = sg.makeValid();
    if (sg.isEmpty()) continue;

    if (!sg.intersects(boundaryUnion)) continue;

    QgsGeometry clippedGeom = sg.intersection(boundaryUnion);
    if (clippedGeom.isEmpty()) continue;
    if (!clippedGeom.isGeosValid())
      clippedGeom = clippedGeom.makeValid();
    if (clippedGeom.isEmpty()) continue;

    QgsFeature newFeat(clippedLayer->fields());
    newFeat.setAttributes(sf.attributes());
    newFeat.setGeometry(clippedGeom);
    outFeatures.append(newFeat);
  }

  if (outFeatures.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리와 겹치는 구간(피처)이 없습니다.");
    delete clippedLayer;
    return nullptr;
  }

  clippedLayer->dataProvider()->addFeatures(outFeatures);
  clippedLayer->updateExtents();

  if (sourceLayer->renderer()) {
    clippedLayer->setRenderer(sourceLayer->renderer()->clone());
  }

  markSurveyLayer(clippedLayer, QStringLiteral("clip_%1").arg(sourceLayer->id()));
  project->addMapLayer(clippedLayer);
  placeInLegendGroup(project, clippedLayer, kGroupSurveyData);

  return clippedLayer;
}

bool LayerOps::splitPolygonWithLine(QgsVectorLayer* layer,
                                   const QVector<QgsPointXY>& splitLine,
                                   QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("올바른 레이어가 아닙니다.");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 나눌 수 있습니다.");
    return false;
  }
  if (splitLine.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("분할선은 최소 2개 이상의 점이어야 합니다.");
    return false;
  }

  QgsGeometry lineGeom = QgsGeometry::fromPolylineXY(splitLine);
  if (lineGeom.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("유효한 분할선이 아닙니다.");
    return false;
  }

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }

  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  int splitCount = 0;

  const QgsFeatureIds selIds = layer->selectedFeatureIds();
  const bool filterSelected = !selIds.isEmpty();

  while (it.nextFeature(f)) {
    if (filterSelected && !selIds.contains(f.id())) continue;
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;

    QgsGeometry g = f.geometry();
    if (!g.intersects(lineGeom)) continue;

    QVector<QgsGeometry> newGeometries;
    QgsPointSequence topologyTestPoints;
    QgsLineString splitCurve(splitLine);
    Qgis::GeometryOperationResult res = g.splitGeometry(&splitCurve, newGeometries, false, false, topologyTestPoints, true);
    if (res == Qgis::GeometryOperationResult::Success && !newGeometries.isEmpty()) {
      layer->changeGeometry(f.id(), g);

      for (const auto& newG : newGeometries) {
        if (newG.isEmpty()) continue;
        QgsFeature newF(layer->fields());
        newF.setAttributes(f.attributes());
        newF.setGeometry(newG);
        layer->addFeature(newF);
      }
      ++splitCount;
    }
  }

  if (splitCount == 0) {
    if (startedHere) layer->rollBack();
    if (errorOut) *errorOut = QStringLiteral("분할선이 관통하는 폴리곤이 없습니다. 폴리곤 양쪽 경계를 완전히 가로질러야 합니다.");
    return false;
  }

  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }

  layer->triggerRepaint();
  return true;
}

bool LayerOps::splitTwoOverlappingFeatures(QgsVectorLayer* layer1, qint64 fid1,
                                          QgsVectorLayer* layer2, qint64 fid2,
                                          qint64* outCreatedFid,
                                          QgsVectorLayer** outTargetLayer,
                                          QString* errorOut) {
  if (outCreatedFid) *outCreatedFid = -1;
  if (outTargetLayer) *outTargetLayer = nullptr;

  if (!layer1 || !layer1->isValid() || !layer2 || !layer2->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("유효하지 않은 레이어입니다.");
    return false;
  }

  QgsFeature f1, f2;
  if (!layer1->getFeatures(QgsFeatureRequest(fid1)).nextFeature(f1) || !f1.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("첫 번째 피처를 찾을 수 없습니다.");
    return false;
  }
  if (!layer2->getFeatures(QgsFeatureRequest(fid2)).nextFeature(f2) || !f2.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("두 번째 피처를 찾을 수 없습니다.");
    return false;
  }

  QgsGeometry g1 = f1.geometry();
  QgsGeometry g2 = f2.geometry();

  if (layer1->crs().isValid() && layer2->crs().isValid() && layer1->crs() != layer2->crs()) {
    QgsCoordinateTransform xf(layer2->crs(), layer1->crs(),
                              QgsProject::instance() ? QgsProject::instance()->transformContext()
                                                     : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    try {
      if (g2.transform(xf) != Qgis::GeometryOperationResult::Success) {
        if (errorOut) *errorOut = QStringLiteral("좌표계 변환에 실패했습니다.");
        return false;
      }
    } catch (...) {
      KaSessionLog::line(QStringLiteral("[except] core/LayerOps.cpp:1520"));
      if (errorOut) *errorOut = QStringLiteral("좌표계 변환 예외가 발생했습니다.");
      return false;
    }
  }

  if (!g1.isGeosValid()) g1 = g1.makeValid();
  if (!g2.isGeosValid()) g2 = g2.makeValid();

  if (!g1.intersects(g2)) {
    if (errorOut) *errorOut = QStringLiteral("선택한 두 도형이 서로 겹치지 않습니다.");
    return false;
  }

  QgsGeometry interGeom = g1.intersection(g2);
  if (interGeom.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("두 도형의 교차 영역을 계산할 수 없습니다.");
    return false;
  }
  if (!interGeom.isGeosValid()) interGeom = interGeom.makeValid();

  QgsGeometry diff1 = g1.difference(g2);
  if (!diff1.isEmpty() && !diff1.isGeosValid()) diff1 = diff1.makeValid();

  QgsGeometry diff2 = g2.difference(g1);
  if (!diff2.isEmpty() && !diff2.isGeosValid()) diff2 = diff2.makeValid();

  const bool crossLayer = (layer1 != layer2);
  if (crossLayer && layer1->crs().isValid() && layer2->crs().isValid() && layer1->crs() != layer2->crs() && !diff2.isEmpty()) {
    QgsCoordinateTransform invXf(layer1->crs(), layer2->crs(),
                                 QgsProject::instance() ? QgsProject::instance()->transformContext()
                                                        : QgsCoordinateTransformContext());
    invXf.setBallparkTransformsAreAppropriate(true);
    try {
      diff2.transform(invXf);
    } catch (...) {
      KaSessionLog::line(QStringLiteral("[except] core/LayerOps.cpp:1555"));
    }
  }

  QgsVectorLayer* targetLayer = layer1;
  QgsFeature targetFeat = f1;
  const QString k1 = layerKeyOf(layer1);
  if (k1 == QLatin1String("survey_area") || layer1->name().contains(QStringLiteral("바운더리"))) {
    targetLayer = layer2;
    targetFeat = f2;
  }

  const bool started1 = !layer1->isEditable();
  if (started1 && !layer1->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("레이어1 편집 모드 시작 실패");
    return false;
  }
  const bool started2 = (crossLayer && !layer2->isEditable());
  if (started2 && !layer2->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("레이어2 편집 모드 시작 실패");
    if (started1) layer1->rollBack();
    return false;
  }

  // 원본 A·B는 그대로 두고 교차 영역만 새 피처로 분리한다.
  Q_UNUSED(diff1);
  Q_UNUSED(diff2);

  const QgsFeatureIds beforeIds = targetLayer->allFeatureIds();

  // 중첩된 교차 구간(A ∩ B)을 새 피처로 추가
  QgsFeature newF(targetLayer->fields());
  newF.setAttributes(targetFeat.attributes());
  newF.setGeometry(interGeom);
  if (!targetLayer->addFeature(newF)) {
    if (errorOut) *errorOut = QStringLiteral("분할된 교차 피처 추가 실패");
    if (started1) layer1->rollBack();
    if (started2) layer2->rollBack();
    return false;
  }

  if (started1 && !layer1->commitChanges()) {
    if (errorOut) *errorOut = layer1->commitErrors().join(QLatin1Char(';'));
    layer1->rollBack();
    if (started2) layer2->rollBack();
    return false;
  }
  if (started2 && !layer2->commitChanges()) {
    if (errorOut) *errorOut = layer2->commitErrors().join(QLatin1Char(';'));
    layer2->rollBack();
    return false;
  }

  qint64 addedId = -1;
  const QgsFeatureIds afterIds = targetLayer->allFeatureIds();
  for (QgsFeatureId id : afterIds) {
    if (!beforeIds.contains(id)) {
      addedId = static_cast<qint64>(id);
      break;
    }
  }
  if (addedId < 0 && newF.id() >= 0)
    addedId = static_cast<qint64>(newF.id());

  if (outCreatedFid) *outCreatedFid = addedId;
  if (outTargetLayer) *outTargetLayer = targetLayer;

  layer1->triggerRepaint();
  if (crossLayer) layer2->triggerRepaint();
  return true;
}

QgsLayerTreeGroup* LayerOps::ensureLegendGroup(QgsProject* project, const QString& groupName) {
  Q_UNUSED(project);
  Q_UNUSED(groupName);
  // ORIG-3: never create empty legend groups. Flat additive layer list only.
  return nullptr;
}

void LayerOps::placeInLegendGroup(QgsProject* project, QgsMapLayer* layer, const QString& groupName,
                                  bool insertAtBottom) {
  Q_UNUSED(groupName);
  Q_UNUSED(insertAtBottom);
  if (!project || !layer) return;
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) {
      node->setItemVisibilityChecked(true);
    }
  }
  ensureSatelliteAtBottom(project);
  pruneEmptyLegendGroups(project);
}

void LayerOps::markSurveyLayer(QgsMapLayer* layer, const QString& layerKey) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerKey), layerKey);
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleSurvey));
}

void LayerOps::markCadastralLayer(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleCadastral));
  layer->setCustomProperty(QStringLiteral("ka_hgis/cadastral"), true);
}

void LayerOps::markReferenceLayer(QgsMapLayer* layer) {
  if (!layer) return;
  if (isCadastralLayer(layer)) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleReference));
}

void LayerOps::placeCadastralLayer(QgsProject* project, QgsMapLayer* layer) {
  if (!project || !layer) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  // 바탕 지적 그림과 받은 지적도, 조사구역·옛 지도는 한 묶음이 아니다.
  // 예전 "지적도" 그룹에 들어가 있던 줄은 각각 최상위 한 줄로 올린다.
  if (QgsLayerTreeGroup* bundled = root->findGroup(QString::fromUtf8(kGroupCadastral))) {
    const int at = root->children().indexOf(bundled);
    while (!bundled->children().isEmpty()) {
      QgsLayerTreeNode* child = bundled->children().constFirst();
      if (!bundled->takeChild(child)) break;
      root->insertChildNode(qMax(0, at), child);
    }
    if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(bundled->parent()))
      parent->removeChildNode(bundled);
  }
  if (isVworldCadastralPicture(layer)) {
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) {
      if (node->parent() == root) return;
      auto* clone = node->clone();
      root->insertChildNode(0, clone);
      if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent()))
        parent->removeChildNode(node);
      return;
    }
    root->insertLayer(0, layer);
    return;
  }
  QgsLayerTreeGroup* refs = root->findGroup(QString::fromUtf8(kGroupReference));
  if (!refs)
    refs = root->addGroup(QString::fromUtf8(kGroupReference));
  if (!refs) return;
  if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) {
    if (node->parent() == refs) return;
    auto* clone = node->clone();
    refs->insertChildNode(0, clone);
    if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent()))
      parent->removeChildNode(node);
    return;
  }
  if (auto* added = refs->addLayer(layer))
    added->setItemVisibilityChecked(true);
}

void LayerOps::applyThematicOverlayScaleRange(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setCustomProperty(QStringLiteral("ka_hgis/thematic_overlay"), true);
  layer->setScaleBasedVisibility(false);
  layer->setMinimumScale(0.0);
  layer->setMaximumScale(0.0);
}

void LayerOps::restoreThematicOverlayVisibility(QgsProject* project) {
  if (!project) return;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer) continue;
    const bool tagged = layer->customProperty(QStringLiteral("ka_hgis/thematic_overlay")).toBool();
    // Upgrade only the exact scale limit formerly assigned by this app. Preserve
    // user scale rules on imported layers, including similarly named datasets.
    const bool legacyLimit = layer->hasScaleBasedVisibility() &&
        layer->minimumScale() == kThematicMinScaleDenom + 1.0 && layer->maximumScale() == 0.0;
    const QString source = layer->source();
    static const QRegularExpression thematicTable(
        QStringLiteral("\\|layername=(soil_map|geology_map|river_map|paleo_landform)(?:\\||$)"));
    const bool knownSource = source.contains(thematicTable) ||
        source.contains(QLatin1String("data.kigam.re.kr/geoserver")) ||
        source.contains(QLatin1String("soil.rda.go.kr"));
    if (legacyLimit && (tagged || (knownSource && isReferenceOrBasemapLayer(layer))))
      applyThematicOverlayScaleRange(layer);
  }
}

bool LayerOps::clampCanvasToThematicScale(QgsMapCanvas* canvas) {
  if (!canvas) return false;
  if (canvas->scale() > kThematicMinScaleDenom) {
    canvas->zoomScale(kThematicMinScaleDenom, true);
    return true;
  }
  return false;
}

QgsRectangle LayerOps::expandExtentToMaxSpan(const QgsRectangle& extent, double maxSpanMeters) {
  if (extent.isEmpty() || maxSpanMeters <= 0.0) return extent;
  const double w = extent.width();
  const double h = extent.height();
  if (w <= 0.0 || h <= 0.0) return extent;
  if (w > maxSpanMeters || h > maxSpanMeters) return extent;
  const double longer = std::max(w, h);
  const double factor = maxSpanMeters / longer;
  if (factor <= 1.0) return extent;
  QgsRectangle grown = extent;
  grown.scale(factor);
  return grown;
}

void LayerOps::setAlignPending(QgsMapLayer* layer, bool pending) {
  if (!layer) return;
  if (pending)
    layer->setCustomProperty(QString::fromUtf8(kPropAlignPending), true);
  else
    layer->removeCustomProperty(QString::fromUtf8(kPropAlignPending));
}

bool LayerOps::isAlignPending(const QgsMapLayer* layer) {
  return layer && layer->customProperty(QString::fromUtf8(kPropAlignPending)).toBool();
}

QString LayerOps::layerKeyOf(const QgsMapLayer* layer) {
  if (!layer) return {};
  return layer->customProperty(QString::fromUtf8(kPropLayerKey)).toString();
}

void LayerOps::applyLegendCrsLabel(QgsMapLayer* layer) {
  if (!layer) return;
  const QString shown = kaFriendlyLegendName(layer->name());
  if (!shown.isEmpty() && layer->name() != shown)
    layer->setName(shown);
  const QString auth = layer->crs().isValid() ? layer->crs().authid() : QString();
  if (!auth.isEmpty()) {
    layer->setAbstract(QStringLiteral("좌표계 %1").arg(auth));
    layer->setCustomProperty(QStringLiteral("ka_hgis/crs_label"), auth);
  }
}

bool LayerOps::isCadastralLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (!layerKeyOf(layer).isEmpty()) return false;
  if (layer->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool()) return true;
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleCadastral))
    return true;
  if (isBasemapLayer(layer)) return false;
  const QString n = layer->name();
  if (n.contains(QStringLiteral("VWorld"))) return false;
  if (!n.contains(QStringLiteral("지적"))) return false;
  return layer->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) == 0;
}

bool LayerOps::isVworldCadastralPicture(const QgsMapLayer* layer) {
  if (!layer) return false;
  const QString n = layer->name();
  if (n.contains(QStringLiteral("VWorld")) && n.contains(QStringLiteral("지적")))
    return true;
  if (n == QLatin1String("지적") || n.startsWith(QLatin1String("지적 본번")) ||
      n.startsWith(QLatin1String("지적 부번")) || n.startsWith(QLatin1String("지적(")))
    return true;
  return isBasemapLayer(layer) && n.contains(QStringLiteral("지적"));
}

bool LayerOps::projectHasCadastralLayer(const QgsProject* project) {
  if (!project) return false;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer || !layer->isValid()) continue;
    if (isCadastralLayer(layer) || isVworldCadastralPicture(layer))
      return true;
  }
  return false;
}

bool LayerOps::userRemovedCadastral(const QgsProject* project) {
  return project && project->readBoolEntry(QStringLiteral("ka_hgis"),
                                           QString::fromUtf8(kPropSkipAutoCadastral), false);
}

void LayerOps::rememberUserRemovedCadastral(QgsProject* project) {
  if (!project) return;
  project->writeEntry(QStringLiteral("ka_hgis"), QString::fromUtf8(kPropSkipAutoCadastral), true);
}

void LayerOps::clearUserRemovedCadastral(QgsProject* project) {
  if (!project) return;
  project->removeEntry(QStringLiteral("ka_hgis"), QString::fromUtf8(kPropSkipAutoCadastral));
}

QList<QgsMapLayer*> LayerOps::removableCadastralLayersFromNode(QgsLayerTreeNode* node) {
  QList<QgsMapLayer*> out;
  if (!node) return out;
  auto push = [&](QgsMapLayer* layer) {
    if (!layer || !layerKeyOf(layer).isEmpty() || out.contains(layer)) return;
    if (isCadastralLayer(layer) || isVworldCadastralPicture(layer))
      out.append(layer);
  };
  if (auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node))
    push(leaf->layer());
  return out;
}

QList<QgsMapLayer*> LayerOps::removableReferenceLayersFromNode(QgsLayerTreeNode* node) {
  QList<QgsMapLayer*> out;
  if (!node) return out;
  auto push = [&](QgsMapLayer* layer) {
    if (!layer || out.contains(layer) || !layerKeyOf(layer).isEmpty()) return;
    if (isCadastralLayer(layer) || isVworldCadastralPicture(layer)) return;
    out.append(layer);
  };
  if (auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node))
    push(leaf->layer());
  return out;
}

QList<QgsMapLayer*> LayerOps::removableLegendLayersFromNode(QgsLayerTreeNode* node) {
  // 묶음 줄을 지우면 안의 레이어가 하위 묶음까지 모두 빠진다. 보이지 않는 맨 위 뿌리는 묶음 줄이 아니다.
  if (auto* group = qobject_cast<QgsLayerTreeGroup*>(node); group && group->parent()) {
    QList<QgsMapLayer*> out;
    for (QgsLayerTreeLayer* child : group->findLayers()) {
      QgsMapLayer* layer = child ? child->layer() : nullptr;
      if (layer && !out.contains(layer))
        out.append(layer);
    }
    return out;
  }
  QList<QgsMapLayer*> out = removableCadastralLayersFromNode(node);
  for (QgsMapLayer* layer : removableReferenceLayersFromNode(node)) {
    if (layer && !out.contains(layer))
      out.append(layer);
  }
  return out;
}

bool LayerOps::isReferenceLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  // Downloaded cadastral has its own role for snap/edit; it is not a generic reference layer.
  if (isCadastralLayer(layer)) return false;
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleReference))
    return true;
  const QString n = layer->name();
  return n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("VWorld")) ||
         n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google")) ||
         n.contains(QStringLiteral("고도맵")) || n.contains(QStringLiteral("지형맵")) ||
         n == QLatin1String("DEM") || n.contains(QStringLiteral("OpenTopoMap")) ||
         n.contains(QStringLiteral("고지형")) || n == QLatin1String("위성") ||
         n.startsWith(QLatin1String("지적")) || n.contains(QStringLiteral("대동여지도")) ||
         n.contains(QStringLiteral("1919 조선지형도"));
}

bool LayerOps::isSnapSourceLayer(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (isCadastralLayer(layer)) return true;
  return !isReferenceLayer(layer);
}

bool LayerOps::isBasemapLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  // Live tiles only. A user SHP named "지적…" must not survive 새 조사.
  const QString p = layer->providerType();
  return p == QLatin1String("wms") || p == QLatin1String("xyz") || p == QLatin1String("vectortile");
}

bool LayerOps::isReferenceOrBasemapLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  // 조사 도메인 레이어(survey_area, feature_poly, feature_line 등)는 절대 배경지도가 아니다.
  if (!layerKeyOf(layer).isEmpty()) return false;
  if (isCadastralLayer(layer)) return true;

  // 명시적 참조 역할
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleReference))
    return true;

  // 기존 isBasemapLayer 또는 isReferenceLayer 확인
  if (isBasemapLayer(layer) || isReferenceLayer(layer))
    return true;

  // 레이어 트리의 "참조 지도" 그룹 소속 여부 확인
  if (auto* proj = QgsProject::instance()) {
    if (auto* root = proj->layerTreeRoot()) {
      if (auto* node = root->findLayer(layer->id())) {
        auto* parent = node->parent();
        while (parent) {
          if (parent->name() == QString::fromUtf8(kGroupReference) ||
              parent->name().contains(QStringLiteral("참조"))) {
            return true;
          }
          parent = parent->parent();
        }
      }
    }
  }

  // 지질도, 토양도, 수계도, 고지형, 음영기복, 위성, 지적, DEM 등 명칭 또는 소스 검사
  const QString n = layer->name();
  if (n.contains(QStringLiteral("지질")) || n.contains(QStringLiteral("토양")) ||
      n.contains(QStringLiteral("수계")) || n.contains(QStringLiteral("음영")) ||
      n.contains(QStringLiteral("단면")) || n.contains(QStringLiteral("배경")) ||
      n.contains(QStringLiteral("정사")) || n.contains(QStringLiteral("위성")) ||
      n.contains(QStringLiteral("지적")) || n.contains(QStringLiteral("DEM")) ||
      n.contains(QStringLiteral("지형")) || n.contains(QStringLiteral("등고"))) {
    return true;
  }

  // 도메인 키가 없는 모든 래스터 레이어는 배경/참조 지도로 간주
  if (layer->type() == Qgis::LayerType::Raster) {
    return true;
  }

  return false;
}

QgsVectorLayer* LayerOps::findByLayerKey(QgsProject* project, const QString& layerKey) {
  if (!project || layerKey.isEmpty()) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (!v) continue;
    if (layerKeyOf(v) == layerKey) return v;
  }
  const auto byName = project->mapLayersByName(layerKey);
  if (!byName.isEmpty())
    return qobject_cast<QgsVectorLayer*>(byName.first());
  return nullptr;
}

QList<QgsVectorLayer*> LayerOps::findAllByLayerKey(QgsProject* project, const QString& layerKey) {
  QList<QgsVectorLayer*> found;
  if (!project || layerKey.isEmpty()) return found;
  // 레이어 트리 순서를 먼저 따른다. mapLayers() 는 해시 순서라 실행마다 달라진다.
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    const auto nodes = root->findLayers();
    for (QgsLayerTreeLayer* node : nodes) {
      auto* v = qobject_cast<QgsVectorLayer*>(node ? node->layer() : nullptr);
      if (!v || found.contains(v)) continue;
      if (layerKeyOf(v) == layerKey) found.append(v);
    }
  }
  // 트리에 없지만 프로젝트에 있는 레이어도 포함한다. id 순으로 넣어 순서를 고정한다.
  QStringList restIds;
  const auto all = project->mapLayers();
  for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
    auto* v = qobject_cast<QgsVectorLayer*>(it.value());
    if (!v || found.contains(v)) continue;
    if (layerKeyOf(v) == layerKey) restIds.append(it.key());
  }
  restIds.sort();
  for (const QString& id : restIds)
    found.append(qobject_cast<QgsVectorLayer*>(all.value(id)));
  return found;
}

QList<QgsVectorLayer*> LayerOps::domainLayersForKey(QgsProject* project, const QString& layerKey) {
  QList<QgsVectorLayer*> found = findAllByLayerKey(project, layerKey);
  if (found.isEmpty() && project) {
    // layer_key 가 없는 예전 조사와 GPKG 에서 바로 연 레이어는 이름으로 찾는다.
    QMap<QString, QgsVectorLayer*> byName;  // id 순으로 정렬해 순서를 고정한다.
    const auto named = project->mapLayersByName(layerKey);
    for (QgsMapLayer* l : named) {
      auto* v = qobject_cast<QgsVectorLayer*>(l);
      if (!v) continue;
      const QString key = layerKeyOf(v);
      if (!key.isEmpty() && key != layerKey) continue;
      byName.insert(v->id(), v);
    }
    found = byName.values();
  }
  // 표시 이름을 도메인 이름으로 바꾼 참조 자료는 도메인 자료가 아니다.
  found.removeIf([](QgsVectorLayer* v) {
    return !v || !v->isValid() ||
           v->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
               QLatin1String(kRoleReference) ||
           v->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
               QLatin1String(kRoleCadastral) ||
           isCadastralLayer(v);
  });
  return found;
}

QgsVectorLayer* LayerOps::digitizeTargetLayer(QgsProject* project, QgsVectorLayer* current,
                                              const QString& requiredKey) {
  if (requiredKey.isEmpty())
    return nullptr;
  if (current && current->isValid() && layerKeyOf(current) == requiredKey)
    return current;
  return findByLayerKey(project, requiredKey);
}

QStringList LayerOps::domainLayerKeys() {
  return {QStringLiteral("survey_area"), QStringLiteral("feature_poly"), QStringLiteral("feature_line"),
          QStringLiteral("section_line"), QStringLiteral("control_points"),
          QStringLiteral("artifact_point"), QStringLiteral("trial_trench")};
}

void LayerOps::removeSurveyDomainLayers(QgsProject* project) {
  if (!project) return;
  const QStringList domainKeys = domainLayerKeys();
  QStringList ids;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    if (isBasemapLayer(l)) continue;
    const QString key = layerKeyOf(l);
    // 도면 레이어(survey_area, feature_poly 등)만 제거하고, 외부에서 불러온 SHP/DXF 등 사용자 레이어는 온전히 유지한다.
    if (!domainKeys.contains(key))
      continue;
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable())
        v->rollBack();
    }
    ids.append(l->id());
  }
  if (!ids.isEmpty())
    project->removeMapLayers(ids);
  pruneEmptyLegendGroups(project);
}

void LayerOps::pruneEmptyLegendGroups(QgsProject* project) {
  if (!project) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  const QList<QgsLayerTreeNode*> children = root->children();
  for (QgsLayerTreeNode* n : children) {
    auto* g = qobject_cast<QgsLayerTreeGroup*>(n);
    if (!g) continue;
    if (g->children().isEmpty())
      root->removeChildNode(g);
  }
}

QList<QgsVectorLayer*> LayerOps::surveyAreaLayers(QgsProject* project) {
  QList<QgsVectorLayer*> res;
  if (!project) return res;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(l);
    if (!vl || !vl->isValid()) continue;
    const QString key = layerKeyOf(vl);
    const QString name = vl->name();
    const bool isSa = (key == QLatin1String("survey_area")) ||
                      key.startsWith(QLatin1String("survey_area_")) ||
                      vl->customProperty(QStringLiteral("ka_hgis/is_survey_area")).toBool() ||
                      name == QLatin1String("survey_area") ||
                      name == QStringLiteral("조사구역") ||
                      vl->source().contains(QLatin1String("layername=survey_area"));
    if (isSa && !res.contains(vl)) {
      res.append(vl);
    }
  }
  return res;
}

QgsVectorLayer* LayerOps::createSurveyAreaLayer(QgsProject* project, const QString& gpkgPath,
                                                const QString& titleKo, const QColor& stroke,
                                                const QColor& fill, double widthMm,
                                                QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  if (gpkgPath.isEmpty() || !QFile::exists(gpkgPath)) {
    if (errorOut) *errorOut = QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.");
    return nullptr;
  }

  const auto existingSas = surveyAreaLayers(project);
  QSet<QString> usedTables;
  for (auto* sa : existingSas) {
    const QString src = sa->source();
    const int idx = src.indexOf(QStringLiteral("layername="));
    if (idx >= 0) {
      usedTables.insert(src.mid(idx + 10));
    }
  }

  QString tableName = QStringLiteral("survey_area");
  bool canReuseDefault = false;
  if (!usedTables.contains(tableName)) {
    auto* testVl = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkgPath),
                                      titleKo, QStringLiteral("ogr"));
    if (testVl && testVl->isValid() && testVl->featureCount() == 0) {
      canReuseDefault = true;
      delete testVl;
    } else {
      delete testVl;
    }
  }

  QgsVectorLayer* vl = nullptr;
  if (canReuseDefault) {
    vl = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkgPath),
                            titleKo, QStringLiteral("ogr"));
  } else {
    int counter = 2;
    tableName = QStringLiteral("survey_area_%1").arg(counter);
    while (usedTables.contains(tableName)) {
      counter++;
      tableName = QStringLiteral("survey_area_%1").arg(counter);
    }

    const QgsCoordinateReferenceSystem crs = project->crs().isValid()
        ? project->crs()
        : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    const QString memUri = QStringLiteral("Polygon?crs=%1").arg(crs.authid());
    QgsVectorLayer mem(memUri, titleKo, QStringLiteral("memory"));
    if (!mem.isValid()) {
      if (errorOut) *errorOut = QStringLiteral("레이어 메모리 생성 실패");
      return nullptr;
    }
    QgsFields fields;
    fields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
    fields.append(QgsField(QStringLiteral("survey_id"), QMetaType::Type::QString));
    fields.append(QgsField(QStringLiteral("area_m2"), QMetaType::Type::Double));
    fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
    mem.dataProvider()->addAttributes(fields.toList());
    mem.updateFields();
    mem.setCrs(crs);

    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = tableName;
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QString errMsg, newFn, newLayer;
    if (QgsVectorFileWriter::writeAsVectorFormatV3(
            &mem, gpkgPath, project->transformContext(), opts, &errMsg, &newFn, &newLayer) !=
        QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errMsg;
      return nullptr;
    }
    vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, tableName),
                            titleKo, QStringLiteral("ogr"));
  }

  if (!vl || !vl->isValid()) {
    if (errorOut) *errorOut = vl ? vl->error().message() : QStringLiteral("레이어를 열 수 없습니다.");
    delete vl;
    return nullptr;
  }

  vl->setName(titleKo);
  markSurveyLayer(vl, QStringLiteral("survey_area"));
  vl->setCustomProperty(QStringLiteral("ka_hgis/is_survey_area"), true);
  vl->setCustomProperty(QStringLiteral("ka_hgis/survey_table"), tableName);
  vl->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexRgb));
  vl->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  vl->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), widthMm);

  applySimpleVectorStyle(vl, fill, stroke, widthMm, 3.5);
  applyLegendCrsLabel(vl);
  applyAreaM2Labels(vl);

  project->addMapLayer(vl, true);
  placeInLegendGroup(project, vl, QString::fromUtf8(kGroupSurveyData));
  pruneEmptyLegendGroups(project);
  ensureSatelliteAtBottom(project);
  return vl;
}

namespace {
QgsVectorLayer* kaFindExistingSavedLayer(QgsProject* project, const QString& gpkgPath,
                                         const QString& table);
}

QgsVectorLayer* LayerOps::ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                            const QString& layerKey, const QString& titleKo,
                                            QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return nullptr;
  }
  if (auto* existing = kaFindExistingSavedLayer(project, gpkgPath, layerKey))
    return existing;
  if (gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.");
    return nullptr;
  }
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
  const bool canCreateMissing = layerKey == QLatin1String("artifact_point")
                                || layerKey == QLatin1String("trial_trench");
  if (!vl->isValid() && canCreateMissing) {
    delete vl;
    vl = nullptr;
    const QgsCoordinateReferenceSystem crs = project->crs().isValid()
        ? project->crs()
        : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    const QString memUri = (layerKey == QLatin1String("trial_trench"))
                               ? QStringLiteral("Polygon?crs=%1").arg(crs.authid())
                               : QStringLiteral("Point?crs=%1").arg(crs.authid());
    QgsVectorLayer mem(memUri, titleKo, QStringLiteral("memory"));
    if (mem.isValid()) {
      QgsFields fields;
      if (layerKey == QLatin1String("trial_trench")) {
        fields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("width"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("length"), QMetaType::Type::Double));
      } else {
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("artifact_no"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      }
      mem.dataProvider()->addAttributes(fields.toList());
      mem.updateFields();
      mem.setCrs(crs);
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("GPKG");
      opts.layerName = layerKey;
      opts.fileEncoding = QStringLiteral("UTF-8");
      opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      QString errMsg, newFn, newLayer;
      if (QgsVectorFileWriter::writeAsVectorFormatV3(
              &mem, gpkgPath, project->transformContext(), opts, &errMsg, &newFn, &newLayer) ==
          QgsVectorFileWriter::NoError) {
        vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
      } else if (errorOut) {
        *errorOut = errMsg;
      }
    }
  }
  if (!vl || !vl->isValid()) {
    if (errorOut && errorOut->isEmpty())
      *errorOut = vl ? vl->error().message() : QStringLiteral("레이어를 열 수 없습니다.");
    delete vl;
    return nullptr;
  }
  // OGR/GPKG keeps a feature cache. A previous legend-only delete leaves the
  // table; without reload, 「조사구역」 다시 만들기가 지운 면을 그대로 보여 준다.
  if (QgsDataProvider* p = vl->dataProvider())
    p->reloadData();
  vl->updateExtents();
  vl->setName(titleKo);
  markSurveyLayer(vl, layerKey);
  applyLegendCrsLabel(vl);
  if (!loadGpkgDefaultStyle(vl))
    applyDomainDrawStyle(vl, layerKey);
  project->addMapLayer(vl, true);
  pruneEmptyLegendGroups(project);
  return vl;
}

int LayerOps::addNonEmptyDomainLayers(QgsProject* project, const QString& gpkgPath) {
  return addNonEmptySavedGpkgLayers(project, gpkgPath);
}

namespace {

bool kaIsGpkgMetadataTable(const QString& name) {
  const QString n = name.toLower();
  return n.startsWith(QLatin1String("gpkg_")) || n.startsWith(QLatin1String("rtree_")) ||
         n.startsWith(QLatin1String("sqlite_")) || n.startsWith(QLatin1String("qgis_")) ||
         n == QLatin1String("layer_styles");
}

QString kaGpkgLayerNameFromSource(const QString& source) {
  const int i = source.indexOf(QLatin1String("layername="), 0, Qt::CaseInsensitive);
  if (i < 0) return {};
  QString rest = source.mid(i + 10);
  const int cut = rest.indexOf(QLatin1Char('|'));
  if (cut >= 0) rest = rest.left(cut);
  return rest;
}

bool kaSourcePointsAtGpkgTable(const QgsMapLayer* layer, const QString& gpkgPath,
                               const QString& table) {
  if (!layer) return false;
  const QString src = layer->source();
  const QString file = src.section(QLatin1Char('|'), 0, 0);
  if (QFileInfo(file).absoluteFilePath().compare(QFileInfo(gpkgPath).absoluteFilePath(),
                                                 Qt::CaseInsensitive) != 0)
    return false;
  return kaGpkgLayerNameFromSource(src).compare(table, Qt::CaseInsensitive) == 0;
}

QStringList kaListGpkgFeatureTables(const QString& gpkgPath) {
  QStringList names;
  if (gpkgPath.isEmpty() || !QFileInfo::exists(gpkgPath)) return names;
  GDALDatasetH ds = GDALOpenEx(gpkgPath.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr,
                               nullptr);
  if (!ds) return names;
  OGRLayerH res = GDALDatasetExecuteSQL(
      ds, "SELECT table_name FROM gpkg_contents WHERE data_type='features'", nullptr, nullptr);
  if (res) {
    OGRFeatureH f = nullptr;
    while ((f = OGR_L_GetNextFeature(res)) != nullptr) {
      const char* v = OGR_F_GetFieldAsString(f, 0);
      if (v && *v) {
        const QString name = QString::fromUtf8(v);
        if (!kaIsGpkgMetadataTable(name))
          names.append(name);
      }
      OGR_F_Destroy(f);
    }
    GDALDatasetReleaseResultSet(ds, res);
  }
  GDALClose(ds);
  return names;
}

QgsVectorLayer* kaFindExistingSavedLayer(QgsProject* project, const QString& gpkgPath,
                                         const QString& table) {
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl) continue;
    if (kaSourcePointsAtGpkgTable(vl, gpkgPath, table))
      return vl;
  }
  // 조사 파일을 옮기거나 이름을 바꾸면 내장 작업공간이 적어 둔 상대경로(./옛이름.gpkg)가
  // 깨져 저장된 벡터가 전부 invalid 로 열린다.
  //
  // 예전에는 여기서 ka_hgis/layer_key 가 테이블 이름과 똑같을 때만 그 자리를 물려받게
  // 했다. 도메인 레이어(layer_key="survey_area")만 조건에 맞고, 들여온 SHP는
  // layer_key 가 "user:문화유적분포지도" 라서 하나도 물려받지 못했다. 그 결과 죽은
  // 레이어가 범례에 그대로 남고 옆에 같은 이름의 새 레이어가 하나 더 생겼다
  // (실제 파일에서 15장 → 25장, 그중 11장이 invalid).
  //
  // 짝은 원본이 가리키던 GPKG 테이블 이름으로만 짓는다. 테이블 이름은 한 파일 안에서
  // 유일하므로 이것이 가장 확실한 열쇠다.
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || vl->isValid()) continue;
    if (vl->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) != 0) continue;
    const QString src = vl->source();
    if (!src.section(QLatin1Char('|'), 0, 0).endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
      continue;
    if (kaGpkgLayerNameFromSource(src).compare(table, Qt::CaseInsensitive) == 0)
      return vl;
  }
  return nullptr;
}

bool kaEnsureLayerTreeNode(QgsProject* project, QgsMapLayer* layer) {
  if (!project || !layer) return false;
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    // A saved layer may still be registered after its legend node was lost.
    // Reuse the layer object so its name, style and references remain intact.
    // Existing nodes retain the user's saved visibility and position.
    if (!root->findLayer(layer->id())) {
      root->insertLayer(0, layer);
      return true;
    }
  }
  return false;
}

bool kaSourceIsSurveyGpkg(const QgsMapLayer* layer, const QString& gpkgPath) {
  if (!layer || gpkgPath.isEmpty()) return false;
  const QString file = layer->source().section(QLatin1Char('|'), 0, 0);
  if (file.isEmpty()) return false;
  return QFileInfo(file).absoluteFilePath().compare(QFileInfo(gpkgPath).absoluteFilePath(),
                                                    Qt::CaseInsensitive) == 0;
}

QString kaSavedGpkgLayerTitle(const QString& table) {
  if (table == QLatin1String("survey_area")) return QStringLiteral("조사구역");
  if (table == QLatin1String("feature_poly")) return QStringLiteral("유구 (면)");
  if (table == QLatin1String("feature_line")) return QStringLiteral("유구 (선)");
  if (table == QLatin1String("section_line")) return QStringLiteral("단면선");
  if (table == QLatin1String("control_points")) return QStringLiteral("기준점");
  if (table == QLatin1String("artifact_point")) return QStringLiteral("유물");
  if (table == QLatin1String("trial_trench")) return QStringLiteral("시굴격자");
  if (table.startsWith(QLatin1String("user_poly_"))) return QStringLiteral("면");
  return table;
}

}  // namespace

bool LayerOps::loadGpkgDefaultStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid())
    return false;
  bool ok = false;
  layer->loadDefaultStyle(ok);
  if (ok)
    return true;
  ok = false;
  layer->loadNamedStyle(layer->source(), ok);
  return ok;
}

int LayerOps::saveGpkgDefaultStyles(QgsProject* project, const QString& gpkgPath) {
  if (!project || gpkgPath.isEmpty())
    return 0;
  int n = 0;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid())
      continue;
    if (!kaSourceIsSurveyGpkg(vl, gpkgPath))
      continue;
    QString err;
    const QgsMapLayer::SaveStyleResults res = vl->saveStyleToDatabaseV2(
        QStringLiteral("ka_hgis"), QString(), true, QString(), err);
    if (!res.testFlag(QgsMapLayer::SaveStyleResult::DatabaseWriteFailed) &&
        !res.testFlag(QgsMapLayer::SaveStyleResult::QmlGenerationFailed))
      ++n;
  }
  return n;
}

int LayerOps::reloadSurveyGpkgReaders(QgsProject* project, const QString& gpkgPath) {
  if (!project || gpkgPath.isEmpty()) return 0;
  int n = 0;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || vl->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) != 0) continue;
    if (!kaSourceIsSurveyGpkg(vl, gpkgPath)) continue;
    if (vl->isEditable() && vl->isModified()) continue;
    if (QgsDataProvider* provider = vl->dataProvider())
      provider->reloadData();
    if (!vl->isValid()) {
      vl->setDataSource(vl->source(), vl->name(), vl->providerType());
    }
    if (!vl->isValid()) continue;
    vl->updateExtents();
    vl->triggerRepaint();
    ++n;
  }
  return n;
}

int LayerOps::restoreMissingLayerTreeNodes(QgsProject* project) {
  if (!project) return 0;
  int restored = 0;
  for (const QString& id : project->mapLayers().keys()) {
    QgsMapLayer* layer = project->mapLayer(id);
    if (layer && layer->isValid() && kaEnsureLayerTreeNode(project, layer))
      ++restored;
  }
  return restored;
}

int LayerOps::addNonEmptySavedGpkgLayers(QgsProject* project, const QString& gpkgPath) {
  if (!project || gpkgPath.isEmpty())
    return 0;
  int added = restoreMissingLayerTreeNodes(project);
  const QStringList domainKeys = domainLayerKeys();
  const QStringList tables = kaListGpkgFeatureTables(gpkgPath);

  // 아래 표 순회는 비어 있는 테이블을 건너뛴다. 파일 이름이 바뀌어 깨진 레이어 중
  // 테이블이 비어 있는 것(예: 아직 도형을 안 넣은 문화유산 목록)은 그러면 영원히
  // invalid 로 남아 범례에만 있고 지도에는 안 그려진다. 먼저 한 번 다 물려 준다.
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || vl->isValid()) continue;
    if (vl->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) != 0) continue;
    const QString src = vl->source();
    if (!src.section(QLatin1Char('|'), 0, 0).endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
      continue;
    const QString table = kaGpkgLayerNameFromSource(src);
    if (table.isEmpty() || !tables.contains(table, Qt::CaseInsensitive)) continue;
    vl->setDataSource(QStringLiteral("%1|layername=%2").arg(gpkgPath, table), vl->name(),
                      QStringLiteral("ogr"));
    if (vl->isValid()) {
      vl->updateExtents();
      kaEnsureLayerTreeNode(project, vl);
      ++added;  // 되살린 것도 "올린 레이어"로 센다 — 호출자는 이 수로 복구 여부를 본다.
    }
  }

  for (const QString& table : tables) {
    QgsVectorLayer probe(QStringLiteral("%1|layername=%2").arg(gpkgPath, table), table,
                         QStringLiteral("ogr"));
    if (!probe.isValid() || probe.featureCount() <= 0)
      continue;

    if (QgsVectorLayer* existing = kaFindExistingSavedLayer(project, gpkgPath, table)) {
      if (!existing->isValid()) {
        // 옮겨지거나 이름이 바뀐 조사 파일. 같은 테이블을 새 경로로 다시 물린다.
        // 지우고 새로 만들면 이름·색·라벨·범례 순서가 전부 공장 기본값으로 돌아간다.
        existing->setDataSource(QStringLiteral("%1|layername=%2").arg(gpkgPath, table),
                                existing->name(), QStringLiteral("ogr"));
      }
      if (existing->isValid()) {
        if (QgsDataProvider* p = existing->dataProvider())
          p->reloadData();
        existing->updateExtents();
        if (existing->isValid() && existing->featureCount() > 0) {
          if (kaEnsureLayerTreeNode(project, existing))
            ++added;
          continue;
        }
      }
      project->removeMapLayer(existing->id());
    }

    const QString titleKo = kaSavedGpkgLayerTitle(table);
    if (domainKeys.contains(table)) {
      QString err;
      if (auto* vl = ensureDomainLayer(project, gpkgPath, table, titleKo, &err)) {
        kaEnsureLayerTreeNode(project, vl);
        ++added;
      }
      continue;
    }

    auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, table), titleKo,
                                  QStringLiteral("ogr"));
    if (!vl->isValid()) {
      delete vl;
      continue;
    }
    if (QgsDataProvider* p = vl->dataProvider())
      p->reloadData();
    vl->updateExtents();
    vl->setName(titleKo);
    markSurveyLayer(vl, table);
    applyLegendCrsLabel(vl);
    if (!loadGpkgDefaultStyle(vl))
      applyAreaM2Labels(vl);
    project->addMapLayer(vl, true);
    placeInLegendGroup(project, vl, QString::fromUtf8(kGroupSurveyData));
    kaEnsureLayerTreeNode(project, vl);
    ++added;
  }
  return added;
}

QgsVectorLayer* LayerOps::createUserPolygonLayer(QgsProject* project, const QString& gpkgPath,
                                                 const QString& titleKo, const QString& crsAuthId,
                                                 QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  QString crsId = crsAuthId.trimmed();
  if (crsId.isEmpty()) crsId = QStringLiteral("EPSG:5186");
  QgsCoordinateReferenceSystem crs(crsId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계가 올바르지 않습니다.");
    return nullptr;
  }

  const QString key = QStringLiteral("user_poly_%1").arg(QDateTime::currentMSecsSinceEpoch());
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=%1").arg(crs.authid()), titleKo, QStringLiteral("memory"));
  if (!mem.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("면 레이어를 만들 수 없습니다.");
    return nullptr;
  }
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  mem.setCrs(crs);

  QString loadPath;
  if (!gpkgPath.isEmpty() && QFile::exists(gpkgPath)) {
    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = key;
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QString errMsg, newFn, newLayer;
    const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
        &mem, gpkgPath, QgsCoordinateTransformContext(), opts, &errMsg, &newFn, &newLayer);
    if (we != QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errMsg.isEmpty() ? QStringLiteral("GPKG에 레이어를 쓰지 못했습니다.") : errMsg;
      return nullptr;
    }
    loadPath = QStringLiteral("%1|layername=%2").arg(gpkgPath, key);
  } else {
    loadPath = QStringLiteral("Polygon?crs=%1").arg(crs.authid());
  }

  auto* vl = new QgsVectorLayer(loadPath, titleKo, gpkgPath.isEmpty() ? QStringLiteral("memory")
                                                                      : QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = vl->error().message();
    delete vl;
    return nullptr;
  }
  if (gpkgPath.isEmpty()) {
    vl->dataProvider()->addAttributes(fields.toList());
    vl->updateFields();
    vl->setCrs(crs);
  }
  vl->setName(titleKo);
  markSurveyLayer(vl, key);
  applyLegendCrsLabel(vl);
  applyAreaM2Labels(vl);
  project->addMapLayer(vl, true);
  placeInLegendGroup(project, vl, QString::fromUtf8(kGroupSurveyData));
  pruneEmptyLegendGroups(project);
  return vl;
}

QList<QgsMapLayer*> LayerOps::visibleLayersPaintOrder(QgsProject* project) {
  QList<QgsMapLayer*> visible;
  if (!project) return visible;
  QgsLayerTree* root = project->layerTreeRoot();
  QList<QgsMapLayer*> ordered = root ? root->layerOrder() : QList<QgsMapLayer*>();
  if (ordered.isEmpty()) {
    const QMap<QString, QgsMapLayer*> all = project->mapLayers();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
      if (it.value() && it.value()->isValid())
        ordered.append(it.value());
    }
  }

  auto pushVisible = [&](QgsMapLayer* l) {
    if (!l || !l->isValid()) return;
    if (isAlignPending(l)) return;
    if (root) {
      if (QgsLayerTreeLayer* n = root->findLayer(l->id())) {
        if (!n->isVisible()) return;
      }
    }
    visible.append(l);
  };
  for (QgsMapLayer* l : ordered)
    pushVisible(l);
  if (visible.isEmpty()) {
    for (QgsMapLayer* l : project->mapLayers())
      pushVisible(l);
  }

  QList<QgsMapLayer*> backgrounds;
  for (int i = visible.size() - 1; i >= 0; --i)
    if (isBasemapLayer(visible[i])) backgrounds.prepend(visible.takeAt(i));
  visible.append(backgrounds);
  // 위성 레이어는 캔버스 렌더링 순서에서도 항상 가장 바닥(스택의 맨 끝)으로 배치
  QList<QgsMapLayer*> sats;
  for (int i = visible.size() - 1; i >= 0; --i) {
    if (visible[i] && visible[i]->name().contains(QStringLiteral("위성"))) {
      sats.prepend(visible.takeAt(i));
    }
  }
  for (QgsMapLayer* sat : sats) {
    visible.append(sat);
  }

  return visible;
}

QString LayerOps::layerCensus(QgsProject* project) {
  if (!project) return QStringLiteral("(프로젝트 없음)");
  QgsLayerTree* root = project->layerTreeRoot();
  QStringList parts;
  const QList<QgsMapLayer*> ordered =
      root && !root->layerOrder().isEmpty() ? root->layerOrder() : project->mapLayers().values();
  QStringList seen;
  auto describe = [&](QgsMapLayer* l) {
    if (!l || seen.contains(l->id())) return;
    seen << l->id();
    const bool hasNode = root && root->findLayer(l->id()) != nullptr;
    const bool checked = hasNode && root->findLayer(l->id())->itemVisibilityChecked();
    QString count = QStringLiteral("-");
    if (auto* v = qobject_cast<QgsVectorLayer*>(l))
      count = l->isValid() ? QString::number(v->featureCount()) : QStringLiteral("?");
    parts << QStringLiteral("%1{%2 valid=%3 node=%4 chk=%5 n=%6}")
                 .arg(l->name(), l->id().left(12))
                 .arg(l->isValid() ? 1 : 0)
                 .arg(hasNode ? 1 : 0)
                 .arg(checked ? 1 : 0)
                 .arg(count);
  };
  for (QgsMapLayer* l : ordered) describe(l);
  // 범례에서 빠진 레이어도 빠짐없이 적는다 — 사라짐의 절반은 이 상태다.
  for (QgsMapLayer* l : project->mapLayers()) describe(l);
  return QStringLiteral("총 %1 · %2").arg(parts.size()).arg(parts.join(QStringLiteral(" | ")));
}

// persistWorkspace가 .ka-survey-gen-* 에서 상대 경로를 쓰면 ../ 가 하나 더
// 붙는다. 조사 폴더에서 풀면 C:/Users/<계정>/AppData 가 C:/Users/AppData 가 된다.
static QString kaRestoreDroppedHomeComponent(const QString& file) {
  const QString clean = QDir::fromNativeSeparators(file);
  const QString home = QDir::fromNativeSeparators(QFileInfo(QDir::homePath()).absoluteFilePath());
  if (home.size() < 4) return {};
  const QString usersDir = QFileInfo(home).path();
  if (!usersDir.endsWith(QLatin1String("/Users"), Qt::CaseInsensitive)) return {};
  const QString prefix = usersDir + QLatin1Char('/');
  if (!clean.startsWith(prefix, Qt::CaseInsensitive)) return {};
  const QString rest = clean.mid(prefix.size());
  const QString user = QFileInfo(home).fileName();
  if (user.isEmpty() || rest.startsWith(user + QLatin1Char('/'), Qt::CaseInsensitive)) return {};
  return QDir::cleanPath(home + QLatin1Char('/') + rest);
}

static QString kaResolvePersistedSource(const QString& source, const QString& home,
                                        const QString& surveyGpkg) {
  const int pipe = source.indexOf(QLatin1Char('|'));
  const QString file = pipe < 0 ? source : source.left(pipe);
  const QString extra = pipe < 0 ? QString() : source.mid(pipe);
  if (file.isEmpty()) return source;
  const auto exists = [](const QString& path) {
    return !path.isEmpty() && QFileInfo::exists(QDir::cleanPath(path));
  };
  if (exists(file)) return source;
  if (const QString restored = kaRestoreDroppedHomeComponent(file); exists(restored))
    return restored + extra;
  if (home.isEmpty()) return source;
  const QString fromHome = QDir::cleanPath(QDir(home).absoluteFilePath(file));
  if (exists(fromHome)) return fromHome + extra;
  // persistWorkspace가 .ka-survey-gen 하위에 쓰던 시절의 상대 경로.
  const QString fromGenerationHome = QDir::cleanPath(
      QDir(QDir(home).filePath(QStringLiteral(".ka-survey-gen-home"))).absoluteFilePath(file));
  if (exists(fromGenerationHome)) return fromGenerationHome + extra;
  QString rel = file;
  for (int i = 0; i < 4; ++i) {
    if (!rel.startsWith(QLatin1String("../")) && !rel.startsWith(QLatin1String("..\\")))
      break;
    rel = rel.mid(3);
    const QString stripped = QDir::cleanPath(QDir(home).absoluteFilePath(rel));
    if (exists(stripped)) return stripped + extra;
  }
  // 저장은 검증한 다음 세대(.ka-survey-gen-*)에 작업공간을 쓰므로, 그때 계산한 상대
  // 경로는 조사 폴더에서 읽으면 한 칸 어긋난 절대 경로로 풀린다. 조사 폴더를 기준으로
  // 뒤쪽 조각부터(가장 구체적인 것부터) 같은 파일을 찾는다. 조사 폴더를 옮긴 경우도
  // 같은 방법으로 붙는다.
  const QStringList parts =
      QDir::fromNativeSeparators(file).split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (int start = 1; start < parts.size(); ++start) {
    const QString tail = QStringList(parts.mid(start)).join(QLatin1Char('/'));
    if (tail.isEmpty() || tail.startsWith(QLatin1String(".."))) continue;
    const QString candidate = QDir::cleanPath(QDir(home).absoluteFilePath(tail));
    if (exists(candidate)) return candidate + extra;
  }
  // 저장 때 조사 파일 안으로 흡수된 레이어는 세대 파일 이름을 가리킨 채 남는다.
  // 테이블 이름이 있으면 조사 파일에서 같은 테이블을 찾게 한다.
  if (!surveyGpkg.isEmpty() && exists(surveyGpkg) &&
      extra.contains(QLatin1String("layername="), Qt::CaseInsensitive) &&
      file.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
    return QDir::cleanPath(surveyGpkg) + extra;
  return source;
}

int LayerOps::repairPersistedFileSources(QgsProject* project) {
  if (!project) return 0;
  const QString fileName = project->fileName();
  QString surveyGpkg;
  if (fileName.startsWith(QLatin1String("geopackage:"), Qt::CaseInsensitive))
    surveyGpkg = fileName.mid(11).section(QLatin1Char('?'), 0, 0);
  else if (fileName.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
    surveyGpkg = fileName;
  else if (!fileName.isEmpty()) {
    // 동반 .qgz 로 열었으면 같은 이름의 조사 파일이 옆에 있다.
    const QFileInfo info(fileName);
    const QString sibling = info.dir().filePath(info.completeBaseName() + QStringLiteral(".gpkg"));
    if (QFileInfo::exists(sibling)) surveyGpkg = sibling;
  }
  // 기준 폴더는 지금 연 조사 파일이 있는 곳이 먼저다. 저장한 작업공간의
  // presetHomePath 는 저장에 쓰고 지운 세대 폴더(.ka-survey-gen-*)로 남아 있어,
  // 그것만 믿으면 바깥 참조 지도를 영영 찾지 못한다.
  QStringList bases;
  const auto addBase = [&bases](const QString& path) {
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty() || clean == QLatin1String(".") || bases.contains(clean)) return;
    if (QFileInfo::exists(clean)) bases << clean;
  };
  if (!surveyGpkg.isEmpty()) addBase(QFileInfo(surveyGpkg).absolutePath());
  addBase(project->presetHomePath());
  if (!fileName.isEmpty() && !fileName.startsWith(QLatin1String("geopackage:"), Qt::CaseInsensitive))
    addBase(QFileInfo(fileName).absolutePath());
  const QString localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  addBase(localData);
  addBase(QDir(localData).filePath(QStringLiteral("ka-hgis")));
  addBase(QDir::home().filePath(QStringLiteral("AppData/Local/ka-hgis")));
  addBase(QDir::home().filePath(QStringLiteral("AppData/Local/ka-hgis/ka-hgis")));
  int n = 0;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (!layer || layer->isValid()) continue;
    QString provider = layer->providerType().toLower();
    if (provider.isEmpty()) {
      const QString file = layer->source().section(QLatin1Char('|'), 0, 0).toLower();
      provider = (file.endsWith(QLatin1String(".tif")) || file.endsWith(QLatin1String(".xml")) ||
                  file.endsWith(QLatin1String(".vrt")))
                     ? QStringLiteral("gdal")
                     : QStringLiteral("ogr");
    }
    if (provider != QLatin1String("ogr") && provider != QLatin1String("gdal")) continue;
    const QString original = layer->source();
    QString repaired = original;
    for (const QString& base : bases) {
      repaired = kaResolvePersistedSource(original, base, surveyGpkg);
      if (repaired != original) break;
    }
    if (repaired == original) continue;
    layer->setDataSource(repaired, layer->name(), provider);
    if (!layer->isValid()) {
      // 잘못 짚었으면 원래 경로로 되돌린다. 사용자가 원본을 찾을 단서를 잃지 않는다.
      layer->setDataSource(original, layer->name(), provider);
      continue;
    }
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer))
      vector->updateExtents();
    layer->triggerRepaint();
    ++n;
  }
  return n;
}

int LayerOps::reviveInvalidLayers(QgsProject* project, QStringList* revived,
                                  QStringList* stillBroken) {
  if (!project) return 0;
  int n = 0;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l || l->isValid()) continue;
    // 편집 중인 버퍼는 건드리지 않는다. 다시 열면 커밋 안 된 도형이 날아간다.
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable()) {
        if (stillBroken) *stillBroken << l->name();
        continue;
      }
    }
    if (QgsDataProvider* p = l->dataProvider())
      p->reloadData();
    if (!l->isValid()) {
      // 같은 URI로 다시 연다. GPKG에 쓰는 동안 끊긴 핸들은 이걸로 돌아온다.
      // 파일 기반이 아닌 원본(xyz/wms)은 파일 존재 검사를 건너뛴다.
      const QString src = l->source();
      const QString file = src.section(QLatin1Char('|'), 0, 0);
      const bool fileBacked = l->providerType().compare(QLatin1String("ogr"), Qt::CaseInsensitive) == 0 ||
                              l->providerType().compare(QLatin1String("gdal"), Qt::CaseInsensitive) == 0;
      if (!fileBacked || QFileInfo::exists(file))
        l->setDataSource(src, l->name(), l->providerType());
    }
    if (l->isValid()) {
      if (auto* v = qobject_cast<QgsVectorLayer*>(l))
        v->updateExtents();
      l->triggerRepaint();
      if (revived) *revived << l->name();
      ++n;
    } else if (stillBroken) {
      *stillBroken << l->name();
    }
  }
  return n;
}

void LayerOps::applyWheelZoomFactor(QgsMapCanvas* canvas) {
  QgsSettings().setValue(QStringLiteral("qgis/zoom_factor"), kWheelZoomFactor);
  if (canvas)
    canvas->setWheelFactor(kWheelZoomFactor);
}

void LayerOps::syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea) {
  if (!project || !canvas) return;
  applyWheelZoomFactor(canvas);
  knockOutProjectRasterPaper(project);
  applyCanvasScreenDpi(canvas);

  // visibleLayersPaintOrder는 무효한 레이어를 화면 목록에서 뺀다. GPKG에 쓰는 동안
  // 잠깐 끊긴 레이어가 여기서 빠지면 다시 넣어 주는 곳이 없어 재시작 전까지 사라진
  // 채로 남는다. 목록을 만들기 전에 되살릴 수 있는 것은 되살린다.
  repairPersistedFileSources(project);
  reviveInvalidLayers(project);

  // 덧그림 조사·유적은 캔버스 목록에서 뺀다. 오버레이가 그 도형을 한 번만 그린다.
  // 지적 지번은 이 목록에 남아 그 선 아래에 있다.
  QList<QgsMapLayer*> visible = sheetBasePaintLayers(project);
  const bool layersChanged = (visible != canvas->layers());

  if (project->crs().isValid())
    canvas->setDestinationCrs(project->crs());
  else
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::ensureOtfEnabled(project, canvas, canvas->mapSettings().destinationCrs().authid());

  if (layersChanged)
    canvas->setLayers(visible);
  canvas->setCachingEnabled(true);
  // QgsMapCanvas::setRenderFlag(true)는 이미 켜져 있어도 refresh()를 강제한다.
  // 여기가 레이어를 만질 때마다 불리므로 필요 없는 전체 렌더가 한 번씩 더 붙었다.
  if (!canvas->renderFlag())
    canvas->setRenderFlag(true);
  const bool wasFrozen = canvas->isFrozen();
  if (!wasFrozen)
    canvas->freeze(false);
  // XYZ + OTF(QgsRasterProjector) + parallel job: provider_wms deleteLater AV on Windows.
  // 병렬 렌더만 끈다 — 미리보기까지 끄면 화면을 끄는 동안 지도가 비어 깜빡인다.
  canvas->setParallelRenderingEnabled(false);

  if (zoomKorea) {
    const QString auth = canvas->mapSettings().destinationCrs().isValid()
                             ? canvas->mapSettings().destinationCrs().authid()
                             : QStringLiteral("EPSG:5186");
    LayerOps::applyKoreaMapLimits(project, canvas);
    QgsRectangle kr = LayerOps::koreaExtentForCrs(auth);
    if (kr.isEmpty() || !kr.isFinite())
      kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    if (!kr.isEmpty() && kr.isFinite()) {
      canvas->setExtent(kr);
      canvas->zoomToFeatureExtent(kr);
      if (canvas->scale() > 3000000.0)
        canvas->zoomScale(1200000.0, true);
    }
    LayerOps::clampCanvasToKorea(canvas);
  }
  if ((layersChanged || zoomKorea) && !canvas->isDrawing())
    canvas->refresh();
}

bool LayerOps::isolateAndZoomToLayer(QgsProject* project, QgsMapCanvas* canvas, QgsMapLayer* layer,
                                     bool keepReference) {
  if (!layer || !layer->isValid()) return false;
  if (canvas && !zoomToLayerMax(canvas, layer))
    return false;

  if (project) {
    if (QgsLayerTree* root = project->layerTreeRoot()) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (!l) continue;
        QgsLayerTreeLayer* n = root->findLayer(l->id());
        if (!n) continue;
        const bool show =
            (l == layer) ||
            (keepReference && !isReferenceLayer(layer) && !isCadastralLayer(layer) &&
             (isReferenceLayer(l) || isCadastralLayer(l)));
        n->setItemVisibilityChecked(show);
      }
    }
  }
  if (canvas) {
    QList<QgsMapLayer*> vis;
    vis.append(layer);
    if (project && keepReference && !isReferenceLayer(layer) && !isCadastralLayer(layer)) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (l && l != layer && l->isValid() && (isReferenceLayer(l) || isCadastralLayer(l)))
          vis.append(l);
      }
    }
    canvas->setLayers(vis);
    refreshCanvasIfIdle(canvas);
  }
  return true;
}

bool LayerOps::isAdminEmdLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QStringLiteral("ka_hgis/admin_emd")).toBool()) return true;
  return layerKeyOf(layer) == QLatin1String(kAdminEmdKey);
}

bool LayerOps::isImportedSiteLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (isAdminEmdLayer(layer)) return false;
  const QString key = layerKeyOf(layer);
  if (!key.startsWith(QLatin1String("user:"))) return false;
  if (key.startsWith(QLatin1String("user:buffer"))) return false;
  return true;
}

QgsVectorLayer* LayerOps::findImportedSiteLayer(QgsProject* project) {
  if (!project) return nullptr;
  QgsVectorLayer* named = nullptr;
  QgsVectorLayer* any = nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (!v || !isImportedSiteLayer(v)) continue;
    if (!any) any = v;
    const QString n = v->name() + layerKeyOf(v);
    if (n.contains(QStringLiteral("유적")))
      return v;
    if (!named) named = v;
  }
  return named ? named : any;
}

bool LayerOps::ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId) {
  const QString auth = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId.trimmed();
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) return false;

  QgsCoordinateTransformContext ctx;
  if (project) {
    ctx = project->transformContext();
    project->setTransformContext(ctx);
    project->setCrs(crs);
  }
  if (canvas)
    canvas->setDestinationCrs(crs);
  return true;
}

bool LayerOps::setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                          QString* errorOut, bool zoomKorea) {
  const QgsCoordinateReferenceSystem crs(epsgAuthId);
  if (!crs.isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("작업 좌표계 %1을 확인할 수 없습니다. 폴더 전체(share\\proj)가 있는지 확인하세요.")
                      .arg(epsgAuthId);
    return false;
  }
  QgsRectangle prev;
  QgsCoordinateReferenceSystem prevCrs;
  if (canvas) {
    prev = canvas->extent();
    prevCrs = canvas->mapSettings().destinationCrs();
  }
  ensureOtfEnabled(project, canvas, epsgAuthId);
  if (canvas) {
    if (zoomKorea) {
      zoomToKorea(canvas, epsgAuthId);
    } else if (!prev.isEmpty() && prevCrs.isValid() && prevCrs != crs) {
      try {
        QgsCoordinateTransform xf(prevCrs, crs, project ? project->transformContext()
                                                        : QgsCoordinateTransformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        canvas->setExtent(xf.transformBoundingBox(prev));
      } catch (...) {
        KaSessionLog::line(QStringLiteral("[except] core/LayerOps.cpp:4842"));
        zoomToKorea(canvas, epsgAuthId);
      }
    } else if (!prev.isEmpty()) {
      canvas->setExtent(prev);
    }
    refreshXyzBasemapTiles(canvas);
  }
  return true;
}

QString LayerOps::convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                   QgsProject* project, QString* errorOut, bool addToMap) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  QString path = outShpPath;
  if (!path.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive))
    path += QStringLiteral(".shp");
  return reprojectVectorLayer(layer, QStringLiteral("EPSG:5179"), path, project, errorOut,
                              addToMap);
}

QString LayerOps::convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                       QgsProject* project, QString* errorOut, bool addToMap) {
  if (!QFile::exists(inPath)) {
    if (errorOut) *errorOut = QStringLiteral("Input not found");
    return {};
  }
  auto* vl = new QgsVectorLayer(inPath, QFileInfo(inPath).completeBaseName(), QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot open: %1").arg(inPath);
    delete vl;
    return {};
  }
  const QString out = convertToShp5179(vl, outShpPath, project, errorOut, addToMap);
  delete vl;
  return out;
}

QString LayerOps::georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                          QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!QFile::exists(imagePath)) {
    if (errorOut) *errorOut = QStringLiteral("Image not found");
    return {};
  }
  if (!controlPoints || controlPoints->featureCount() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Need >=2 control points");
    return {};
  }
  QImage img(imagePath);
  if (img.isNull()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot read image");
    return {};
  }
  const double w = img.width();
  const double h = img.height();
  if (w < 2 || h < 2) {
    if (errorOut) *errorOut = QStringLiteral("Image too small");
    return {};
  }

  struct Gcp {
    double px = -1, py = -1;
    QgsPointXY map;
    bool hasPixel = false;
  };
  QVector<Gcp> gcps;
  QgsFeatureIterator it = controlPoints->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    Gcp g;
    g.map = f.geometry().asPoint();
    const int ix = controlPoints->fields().indexOf(QStringLiteral("pixel_x"));
    const int iy = controlPoints->fields().indexOf(QStringLiteral("pixel_y"));
    if (ix >= 0 && iy >= 0) {
      bool okx = false, oky = false;
      const double px = f.attribute(ix).toDouble(&okx);
      const double py = f.attribute(iy).toDouble(&oky);
      if (okx && oky) {
        g.px = px;
        g.py = py;
        g.hasPixel = true;
      }
    }
    gcps.append(g);
  }
  if (gcps.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Control points lack geometry");
    return {};
  }

  double rotA = 0, rotB = 0, rotD = 0, rotE = 0, ulx = 0, uly = 0;
  const bool allPixel = std::all_of(gcps.begin(), gcps.end(), [](const Gcp& g) { return g.hasPixel; })
                        && gcps.size() >= 2;

  bool lsSolved = false;
  if (allPixel && gcps.size() >= 3) {
    double sxx = 0, sxy = 0, sx = 0, syy = 0, sy = 0, sn = 0;
    double sxX = 0, syX = 0, sX = 0, sxY = 0, syY = 0, sY = 0;
    for (const Gcp& g : gcps) {
      const double x = g.px, y = g.py;
      sxx += x * x; sxy += x * y; sx += x;
      syy += y * y; sy += y; sn += 1;
      sxX += x * g.map.x(); syX += y * g.map.x(); sX += g.map.x();
      sxY += x * g.map.y(); syY += y * g.map.y(); sY += g.map.y();
    }
    auto solve3 = [](double a11, double a12, double a13, double a21, double a22, double a23,
                     double a31, double a32, double a33, double b1, double b2, double b3,
                     double& x1, double& x2, double& x3) -> bool {
      const double det = a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31) + a13 * (a21 * a32 - a22 * a31);
      if (std::abs(det) < 1e-18) return false;
      x1 = (b1 * (a22 * a33 - a23 * a32) - a12 * (b2 * a33 - a23 * b3) + a13 * (b2 * a32 - a22 * b3)) / det;
      x2 = (a11 * (b2 * a33 - a23 * b3) - b1 * (a21 * a33 - a23 * a31) + a13 * (a21 * b3 - b2 * a31)) / det;
      x3 = (a11 * (a22 * b3 - b2 * a32) - a12 * (a21 * b3 - b2 * a31) + b1 * (a21 * a32 - a22 * a31)) / det;
      return true;
    };
    double a = 0, b = 0, c = 0, d = 0, e = 0, fpar = 0;
    if (solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxX, syX, sX, a, b, c)
        && solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxY, syY, sY, d, e, fpar)) {
      rotA = a; rotB = b; rotD = d; rotE = e;
      ulx = c + 0.5 * (rotA + rotB);
      uly = fpar + 0.5 * (rotD + rotE);
      lsSolved = true;
    }
  }

  if (!lsSolved) {
    const QgsPointXY g0 = gcps[0].map;
    const QgsPointXY g1 = gcps[1].map;
    const double dx = g1.x() - g0.x();
    const double dy = g1.y() - g0.y();
    const double dist = std::hypot(dx, dy);
    if (dist < 1e-9) {
      if (errorOut) *errorOut = QStringLiteral("GCP0 and GCP1 too close");
      return {};
    }
    rotA = dx / w;
    rotB = 0.0;
    rotD = 0.0;
    rotE = (std::abs(dy) < 1e-6) ? -std::abs(rotA) : (dy / h);
    if (gcps.size() >= 3) {
      const double dy2 = gcps[2].map.y() - g0.y();
      if (std::abs(dy2) > 1e-6) rotE = dy2 / h;
    }
    ulx = g0.x() + rotA * 0.5;
    uly = g0.y() + rotE * (h - 0.5);
  }

  QString wfPath = imagePath;
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("jgw");
  else if (ext == QLatin1String("png"))
    wfPath = imagePath.left(imagePath.size() - 3) + QStringLiteral("pgw");
  else if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("tfw");
  else
    wfPath = imagePath + QStringLiteral(".wld");

  QFile wf(wfPath);
  if (!wf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write world file");
    return {};
  }
  QTextStream ts(&wf);
  ts.setEncoding(QStringConverter::Utf8);
  // standard world file 6 lines
  ts << QString::number(rotA, 'g', 16) << "\n";
  ts << QString::number(rotD, 'g', 16) << "\n";
  ts << QString::number(rotB, 'g', 16) << "\n";
  ts << QString::number(rotE, 'g', 16) << "\n";
  ts << QString::number(ulx, 'g', 16) << "\n";
  ts << QString::number(uly, 'g', 16) << "\n";
  wf.close();

  auto* rl = new QgsRasterLayer(imagePath, QFileInfo(imagePath).completeBaseName(), QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Georef raster invalid after worldfile");
    delete rl;
    return {};
  }
  if (project) {
    if (project->crs().isValid()) rl->setCrs(project->crs());
    project->addMapLayer(rl);
  }
  if (canvas) {
    canvas->setExtent(rl->extent());
    canvas->refresh();
  }
  return wfPath;
}

namespace {
bool stackCanUndo(QgsVectorLayer* layer) {
  return layer && layer->isValid() && layer->undoStack() && layer->undoStack()->canUndo();
}

bool stackCanRedo(QgsVectorLayer* layer) {
  return layer && layer->isValid() && layer->undoStack() && layer->undoStack()->canRedo();
}

template <typename Pred>
QgsVectorLayer* firstMatchingSurveyLayer(QgsProject* project, QgsVectorLayer* preferred, Pred pred) {
  if (pred(preferred) && !LayerOps::isReferenceLayer(preferred) &&
      !LayerOps::isCadastralLayer(preferred))
    return preferred;
  if (!project) return nullptr;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return nullptr;
  const QList<QgsLayerTreeLayer*> nodes = root->findLayers();
  for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
    auto* vector = qobject_cast<QgsVectorLayer*>((*it)->layer());
    if (pred(vector) && !LayerOps::isReferenceLayer(vector) &&
        !LayerOps::isCadastralLayer(vector))
      return vector;
  }
  return nullptr;
}
}

bool LayerOps::runEditCommand(QgsVectorLayer* layer, const QString& title,
                              const std::function<bool()>& change, QString* errorOut) {
  const auto fail = [&](const QString& text) {
    if (errorOut) *errorOut = text;
    return false;
  };
  if (!layer || !layer->isValid())
    return fail(QStringLiteral("도형이 있는 조사 레이어를 먼저 선택하세요."));
  if (isCadastralLayer(layer))
    return fail(QStringLiteral("지적도는 편집할 수 없습니다."));
  if (isReferenceLayer(layer))
    return fail(QStringLiteral("참조 지도는 편집할 수 없습니다."));
  if (layer->isEditCommandActive())
    return fail(QStringLiteral("진행 중인 도형 편집을 마친 뒤 다시 실행하세요."));
  if (!change)
    return fail(QStringLiteral("도형을 변경하지 못했습니다. 기존 편집은 유지됩니다."));
  if (!layer->isEditable() && !layer->startEditing())
    return fail(QStringLiteral("편집을 시작하지 못했습니다. 파일의 쓰기 권한을 확인하세요."));
  layer->beginEditCommand(title);
  if (!change()) {
    layer->destroyEditCommand();
    return fail(QStringLiteral("도형을 변경하지 못했습니다. 기존 편집은 유지됩니다."));
  }
  layer->endEditCommand();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* LayerOps::preferredUndoLayer(QgsProject* project, QgsVectorLayer* preferred) {
  return firstMatchingSurveyLayer(project, preferred, stackCanUndo);
}

QgsVectorLayer* LayerOps::preferredRedoLayer(QgsProject* project, QgsVectorLayer* preferred) {
  return firstMatchingSurveyLayer(project, preferred, stackCanRedo);
}

bool LayerOps::undoLayerEdits(QgsVectorLayer* layer) {
  if (!stackCanUndo(layer)) return false;
  layer->undoStack()->undo();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::redoLayerEdits(QgsVectorLayer* layer) {
  if (!stackCanRedo(layer)) return false;
  layer->undoStack()->redo();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::undoCommittedFeature(QgsVectorLayer* layer, qint64 featureId, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 되돌릴 수 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 되돌릴 수 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("되돌릴 도형을 찾지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->deleteFeature(fid)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 지우지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges(false)) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  if (startedHere)
    layer->startEditing();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::restoreDeletedFeature(QgsVectorLayer* layer, const QgsFeature& feat, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 복원할 수 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 복원할 수 없습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드를 시작할 수 없습니다.");
    return false;
  }
  QgsFeature f = feat;
  if (!layer->addFeature(f)) {
    if (errorOut) *errorOut = QStringLiteral("도형 복원에 실패했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges(false)) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  if (startedHere)
    layer->startEditing();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::purgeCommittedFeatures(QgsVectorLayer* layer, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 비울 수 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 비울 수 없습니다.");
    return false;
  }
  const QString src = layer->source().split(QLatin1Char('|')).first();
  if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) {
    if (errorOut) *errorOut = QStringLiteral("외부 SHP 파일은 물리적으로 도형을 비울 수 없습니다.");
    return false;
  }
  const QString key = layerKeyOf(layer);
  if (key.startsWith(QLatin1String("user:"))) {
    if (errorOut) *errorOut = QStringLiteral("외부 추가 레이어의 원본 파일은 비울 수 없습니다.");
    return false;
  }
  const QgsFeatureIds ids = layer->allFeatureIds();
  if (ids.isEmpty()) {
    if (QgsDataProvider* p = layer->dataProvider())
      p->reloadData();
    layer->updateExtents();
    return true;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->deleteFeatures(ids)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 지우지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsVectorDataProvider* p = layer->dataProvider()) {
    if (layer->featureCount() > 0 && !p->deleteFeatures(layer->allFeatureIds())) {
      if (errorOut) *errorOut = QStringLiteral("GPKG에서 도형을 지우지 못했습니다.");
      return false;
    }
    p->reloadData();
  }
  layer->updateExtents();
  layer->triggerRepaint();
  if (layer->featureCount() > 0) {
    if (errorOut) *errorOut = QStringLiteral("지운 뒤에도 도형이 남아 있습니다.");
    return false;
  }
  return true;
}

void LayerOps::applySnapSettings(QgsProject* project, const SnapSettings& settings) {
  if (!project) return;
  QgsSnappingConfig cfg(project);
  cfg = project->snappingConfig();
  cfg.setProject(project);
  cfg.setEnabled(settings.enabled);
  const double tolerance = settings.tolerancePx > 0.0 ? settings.tolerancePx : 16.0;
  cfg.setTolerance(tolerance);
  cfg.setUnits(Qgis::MapToolUnit::Pixels);
  cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  cfg.setIntersectionSnapping(true);
  cfg.setSelfSnapping(true);
  cfg.clearIndividualLayerSettings();
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (isCadastralLayer(layer)) placeCadastralLayer(project, layer);
  }
  if (settings.target == SnapTarget::CurrentLayer) {
    cfg.setMode(Qgis::SnappingMode::ActiveLayer);
  } else {
    cfg.setMode(Qgis::SnappingMode::AdvancedConfiguration);
    const Qgis::SnappingTypes types = Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment;
    for (QgsMapLayer* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (!vector || !vector->isValid()) continue;
      cfg.setIndividualLayerSettings(vector, QgsSnappingConfig::IndividualLayerSettings(
          isSnapSourceLayer(vector), types, tolerance, Qgis::MapToolUnit::Pixels));
    }
  }
  project->writeEntry(QStringLiteral("ka_hgis"), QStringLiteral("snap_target"),
                      settings.target == SnapTarget::CurrentLayer ? QStringLiteral("current")
                                                                  : QStringLiteral("survey"));
  project->setTopologicalEditing(settings.topological);
  project->writeEntry(QStringLiteral("ka_hgis"), QStringLiteral("topological"),
                      settings.topological ? QStringLiteral("1") : QStringLiteral("0"));
  project->setSnappingConfig(cfg);
}

LayerOps::SnapSettings LayerOps::readSnapSettings(const QgsProject* project) {
  SnapSettings settings;
  if (!project) return settings;
  const QgsSnappingConfig cfg = project->snappingConfig();
  settings.enabled = cfg.enabled();
  settings.tolerancePx = cfg.tolerance() > 0.0 ? cfg.tolerance() : 16.0;
  const QString target = project->readEntry(QStringLiteral("ka_hgis"), QStringLiteral("snap_target"),
                                            QStringLiteral("survey"));
  settings.target = target == QLatin1String("current") ? SnapTarget::CurrentLayer
                                                       : SnapTarget::SurveyLayers;
  const QString topo = project->readEntry(QStringLiteral("ka_hgis"), QStringLiteral("topological"));
  settings.topological = topo.isEmpty() ? true : topo != QLatin1String("0");
  return settings;
}

bool LayerOps::moveFeatureVertex(QgsVectorLayer* layer, qint64 featureId, int vertex,
                                 double x, double y, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 고칠 수 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 고칠 수 없습니다.");
    return false;
  }
  if (vertex < 0) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점이 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid() || !existing.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("고칠 도형을 찾지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  const bool topological = layer->project() && layer->project()->topologicalEditing();
  if (!applyVertexMove(layer, featureId, vertex, x, y, topological, errorOut)) {
    if (startedHere) layer->rollBack();
    return false;
  }
  return true;
}

bool LayerOps::applyVertexMove(QgsVectorLayer* layer, qint64 featureId, int vertex,
                               double x, double y, bool topological, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isCadastralLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("지적도는 고칠 수 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 고칠 수 없습니다.");
    return false;
  }
  if (vertex < 0) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점이 없습니다.");
    return false;
  }
  if (!layer->isEditable()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid() || !existing.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("고칠 도형을 찾지 못했습니다.");
    return false;
  }
  const QgsPoint origin = existing.geometry().vertexAt(vertex);
  const QgsPointXY from(origin.x(), origin.y());
  const double tol2 = 0.001 * 0.001;

  struct Hit {
    QgsFeatureId id = FID_NULL;
    QgsGeometry geom;
    QList<int> verts;
  };
  QHash<QgsFeatureId, Hit> hits;
  const auto addHit = [&](const QgsFeature& feature, int vi) {
    Hit& hit = hits[feature.id()];
    hit.id = feature.id();
    if (hit.geom.isNull()) hit.geom = feature.geometry();
    if (!hit.verts.contains(vi)) hit.verts.append(vi);
  };

  if (topological) {
    QgsFeature feature;
    QgsFeatureIterator it = layer->getFeatures();
    while (it.nextFeature(feature)) {
      if (!feature.hasGeometry()) continue;
      const QgsGeometry geom = feature.geometry();
      const int count = geom.constGet() ? static_cast<int>(geom.constGet()->nCoordinates()) : 0;
      for (int i = 0; i < count; ++i) {
        const QgsPoint pt = geom.vertexAt(i);
        if (QgsPointXY(pt.x(), pt.y()).sqrDist(from) <= tol2) addHit(feature, i);
      }
    }
  } else {
    addHit(existing, vertex);
    const QgsGeometry geom = existing.geometry();
    const int count = geom.constGet() ? static_cast<int>(geom.constGet()->nCoordinates()) : 0;
    if (geom.type() == Qgis::GeometryType::Polygon && count > 1) {
      if (vertex == 0) addHit(existing, count - 1);
      if (vertex == count - 1) addHit(existing, 0);
    }
  }
  if (!hits.contains(fid)) addHit(existing, vertex);

  for (auto it = hits.begin(); it != hits.end(); ++it) {
    Hit& hit = it.value();
    for (int vi : hit.verts) {
      if (!hit.geom.moveVertex(x, y, vi)) {
        if (errorOut) *errorOut = QStringLiteral("꼭짓점을 옮기지 못했습니다.");
        return false;
      }
    }
    if (!layer->changeGeometry(hit.id, hit.geom)) {
      if (errorOut) *errorOut = QStringLiteral("도형을 고치지 못했습니다.");
      return false;
    }
  }
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::hasVisibleReferenceLayer(QgsProject* project) {
  if (!project) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;

  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l || !isReferenceLayer(l)) continue;
    QgsLayerTreeLayer* node = root->findLayer(l->id());
    if (node && node->isVisible()) return true;
  }
  return false;
}

bool LayerOps::removeConfirmedLayers(QgsProject* project, QgsMapCanvas* canvas, const QStringList& layerIds) {
  if (!project || layerIds.isEmpty()) return false;
  for (const QString& id : layerIds) {
    project->removeMapLayer(id);
  }
  if (canvas)
    refreshCanvasIfIdle(canvas);
  return true;
}

