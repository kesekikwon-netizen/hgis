#include <QtTest>
#include <QTemporaryDir>
#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsmaplayerstyle.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"

class LayerLabelsTest : public QObject {
  Q_OBJECT
private slots:
  void preservesConfiguredExpressionAndStyle() {
    QgsVectorLayer layer(QStringLiteral("LineString?crs=EPSG:5187&field=elevation:double&field=z_valid:integer"),
                         QStringLiteral("등고선"), QStringLiteral("memory"));
    QgsPalLayerSettings settings;
    settings.isExpression = true;
    settings.fieldName = QStringLiteral("CASE WHEN \"z_valid\"=1 AND \"elevation\" > 0 THEN format_number(\"elevation\", 1) END");
    settings.placement = Qgis::LabelPlacement::Curved;
    settings.scaleVisibility = true; settings.minimumScale = 10000.;
    QgsTextFormat format; format.setColor(Qt::magenta); format.setSize(13.);
    settings.setFormat(format);
    layer.setLabeling(new QgsVectorLayerSimpleLabeling(settings)); layer.setLabelsEnabled(true);
    auto* labeling = layer.labeling(); auto* renderer = layer.renderer();
    const auto info = LayerLabelControls::describe(&layer, false, 25000.);
    QVERIFY(info.supported); QVERIFY(info.enabled); QVERIFY(!info.areaEditable);
    QCOMPARE(info.caption, QStringLiteral("높이")); QVERIFY(info.field.isEmpty());
    QVERIFY(info.reason.contains(QStringLiteral("확대")));
    QSignalSpy repaint(&layer, &QgsMapLayer::repaintRequested);
    QVERIFY(!LayerLabelControls::setVisible(&layer, true)); QCOMPARE(repaint.count(), 0);
    QVERIFY(LayerLabelControls::setVisible(&layer, false));
    QVERIFY(!layer.labelsEnabled()); QVERIFY(LayerOps::setLabelsVisible(&layer, true));
    QCOMPARE(layer.labeling(), labeling); QCOMPARE(layer.renderer(), renderer);
    const auto after = labeling->settings();
    QCOMPARE(after.fieldName, settings.fieldName); QCOMPARE(after.placement, settings.placement);
    QCOMPARE(after.format().color(), QColor(Qt::magenta)); QCOMPARE(after.format().size(), 13.);
    QCOMPARE(after.minimumScale, 10000.); QVERIFY(after.scaleVisibility);
  }

  void unknownVectorDoesNotExposeOpaqueFields() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=PNU:string&field=A1:string&field=fid:integer"),
                         QStringLiteral("외부 자료"), QStringLiteral("memory"));
    const auto info = LayerLabelControls::describe(&layer);
    QVERIFY(!info.supported); QVERIFY(info.needsField); QVERIFY(info.field.isEmpty()); QVERIFY(!info.areaEditable);
    QVERIFY(!LayerOps::hasToggleableLabels(&layer));
    QVERIFY(!LayerLabelControls::setVisible(&layer, true));
    QVERIFY(!layer.labeling()); QVERIFY(!layer.labelsEnabled());
  }

  void knownFieldsAndSurveyArea() {
    QgsVectorLayer cad(QStringLiteral("Polygon?crs=EPSG:5187&field=PNU:string&field=JIBUN:string"),
                       QStringLiteral("지적도"), QStringLiteral("memory"));
    auto info = LayerLabelControls::describe(&cad);
    QCOMPARE(info.caption, QStringLiteral("지번")); QCOMPARE(info.field, QStringLiteral("JIBUN"));
    QVERIFY(!info.areaEditable); QVERIFY(LayerLabelControls::setVisible(&cad, true));
    QVERIFY(cad.labeling()->settings().fieldName.contains(QStringLiteral("JIBUN")));
    QVERIFY(!cad.labeling()->settings().fieldName.contains(QStringLiteral("PNU")));
    QgsVectorLayer area(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(&area, QStringLiteral("survey_area"));
    QVERIFY(LayerLabelControls::describe(&area).supported);
    QVERIFY(LayerLabelControls::setVisible(&area, true));
    info = LayerLabelControls::describe(&area);
    QVERIFY(info.areaEditable); QCOMPARE(info.caption, QStringLiteral("면적"));
  }

  void arbitraryAreaExpressionIsNotEditableAsManagedArea() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"), QStringLiteral("사용자"), QStringLiteral("memory"));
    QgsPalLayerSettings settings; settings.isExpression = true;
    settings.fieldName = QStringLiteral("CASE WHEN area($geometry)>100 THEN upper(\"name\") END");
    layer.setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    layer.setCustomProperty(QStringLiteral("ka_hgis/label_field"), QStringLiteral("name"));
    layer.setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), true);
    QVERIFY(!LayerLabelControls::describe(&layer).areaEditable);
    QVERIFY(LayerLabelControls::setVisible(&layer, true));
    QCOMPARE(layer.labeling()->settings().fieldName, settings.fieldName);
    QVERIFY(LayerOps::applyNameAttributeLabels(&layer, QStringLiteral("name"), 12., true));
    QVERIFY(LayerLabelControls::describe(&layer).areaEditable);
    QCOMPARE(LayerLabelControls::describe(&layer).field, QStringLiteral("name"));
  }

  void rasterHasNoMisleadingCheckbox() {
    QgsRasterLayer raster(QStringLiteral("missing.tif"), QStringLiteral("위성영상"));
    const auto info = LayerLabelControls::describe(&raster);
    QVERIFY(!info.supported); QVERIFY(!info.needsField);
    QVERIFY(info.reason.contains(QStringLiteral("이미지")));
    QVERIFY(!LayerLabelControls::setVisible(&raster, true));
  }

  void specializedLabelsRejectGenericContentChanges() {
    QgsVectorLayer cad(QStringLiteral("Polygon?crs=EPSG:5187&field=JIBUN:string&field=PNU:string"),
                       QStringLiteral("지적도"), QStringLiteral("memory"));
    QVERIFY(LayerLabelControls::setVisible(&cad, true));
    auto settings = cad.labeling()->settings();
    settings.scaleVisibility = true; settings.minimumScale = 10000.;
    cad.setLabeling(new QgsVectorLayerSimpleLabeling(settings));
    QVERIFY(!LayerLabelControls::describe(&cad).fieldEditable);
    QVERIFY(!LayerLabelControls::setField(&cad, QStringLiteral("PNU")));
    QVERIFY(!LayerLabelControls::setArea(&cad, true));
    QCOMPARE(cad.labeling()->settings().fieldName, settings.fieldName);
    QCOMPARE(cad.labeling()->settings().minimumScale, 10000.);
    QgsVectorLayer geology(QStringLiteral("Polygon?crs=EPSG:5187&field=기호:string&field=name:string"),
                           QStringLiteral("지질도"), QStringLiteral("memory"));
    QgsPalLayerSettings geologySettings; geologySettings.fieldName = QStringLiteral("기호");
    geology.setLabeling(new QgsVectorLayerSimpleLabeling(geologySettings));
    QVERIFY(!LayerLabelControls::describe(&geology).fieldEditable);
    QVERIFY(!LayerLabelControls::setField(&geology, QStringLiteral("name")));
  }

  void areaWithoutNameNeverFallsBackToOpaqueField() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=opaque:string"), QStringLiteral("면적"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyAreaM2Labels(&layer));
    QVERIFY(LayerLabelControls::describe(&layer).field.isEmpty());
    QVERIFY(LayerLabelControls::setArea(&layer, false));
    QCOMPARE(layer.labeling()->settings().fieldName, QStringLiteral("''"));
    QVERIFY(!LayerLabelControls::setArea(&layer, false));
    QVERIFY(LayerLabelControls::describe(&layer).areaEditable);
    QVERIFY(LayerLabelControls::setArea(&layer, true));
    QVERIFY(!layer.labeling()->settings().fieldName.contains(QStringLiteral("opaque")));
    QVERIFY(layer.labeling()->settings().fieldName.contains(QStringLiteral("area($geometry)")));
  }

  void explicitContentEditsKeepVisibilityAndFormatting() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string&field=title:string"), QStringLiteral("유구"), QStringLiteral("memory"));
    QVERIFY(LayerOps::applyNameAttributeLabels(&layer, QStringLiteral("name"), 12., false));
    auto settings = layer.labeling()->settings();
    settings.scaleVisibility = true; settings.minimumScale = 7000.;
    auto format = settings.format(); format.setColor(Qt::cyan); settings.setFormat(format);
    layer.setLabeling(new QgsVectorLayerSimpleLabeling(settings)); layer.setLabelsEnabled(false);
    QSignalSpy repaint(&layer, &QgsMapLayer::repaintRequested);
    QVERIFY(!LayerLabelControls::setField(&layer, QStringLiteral("name")));
    QCOMPARE(repaint.count(), 0);
    QVERIFY(LayerLabelControls::setField(&layer, QStringLiteral("title")));
    QVERIFY(!layer.labelsEnabled());
    QVERIFY(LayerLabelControls::setArea(&layer, true));
    QVERIFY(!layer.labelsEnabled());
    QCOMPARE(layer.labeling()->settings().format().color(), QColor(Qt::cyan));
    QCOMPARE(layer.labeling()->settings().format().size(), 12.);
    QCOMPARE(layer.labeling()->settings().minimumScale, 7000.);
    QCOMPARE(layer.labeling()->settings().placement, settings.placement);
    QVERIFY(layer.labeling()->settings().fieldName.contains(QStringLiteral("\"title\"")));
    QVERIFY(!LayerLabelControls::setArea(&layer, true));
    QVERIFY(LayerLabelControls::setArea(&layer, false));
    QCOMPARE(layer.labeling()->settings().fieldName, QStringLiteral("\"title\""));
    QVERIFY(!layer.labelsEnabled());
    QgsVectorLayer unknown(QStringLiteral("Point?crs=EPSG:5187&field=opaque:string"), QStringLiteral("외부"), QStringLiteral("memory"));
    QVERIFY(LayerLabelControls::describe(&unknown).fieldEditable);
    QVERIFY(!LayerLabelControls::setField(&unknown, QStringLiteral("missing")));
    QVERIFY(LayerLabelControls::setField(&unknown, QStringLiteral("opaque")));
    QVERIFY(unknown.labelsEnabled());
    QCOMPARE(unknown.labeling()->settings().fieldName, QStringLiteral("\"opaque\""));
  }

  void layoutNumbersAreIndependentAndPersistInProject() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QgsProject project;
    QgsVectorLayer memory(QStringLiteral("Polygon?crs=EPSG:5187&field=site_name:string"), QStringLiteral("원본"), QStringLiteral("memory"));
    QgsVectorFileWriter::SaveVectorOptions options; options.driverName = QStringLiteral("GPKG");
    options.layerName = QStringLiteral("sites");
    const QString path = dir.filePath(QStringLiteral("sites.gpkg"));
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&memory, path, project.transformContext(), options), QgsVectorFileWriter::NoError);
    auto* layer = new QgsVectorLayer(path + QStringLiteral("|layername=sites"), QStringLiteral("사용자 제목"), QStringLiteral("ogr"));
    QVERIFY(layer->isValid()); QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("site_name"), 11., false));
    project.addMapLayer(layer, false);
    project.layerTreeRoot()->addGroup(QStringLiteral("문화유적분포지도"))->addLayer(layer);
    QVERIFY(LayerLabelControls::isHeritage(layer));
    QCOMPARE(LayerLabelControls::describe(layer).caption, QStringLiteral("유적명"));
    QCOMPARE(LayerLabelControls::describe(layer, true).caption, QStringLiteral("번호"));
    auto* labeling = layer->labeling();
    QSignalSpy repaint(layer, &QgsMapLayer::repaintRequested);
    QVERIFY(LayerLabelControls::setVisible(layer, false, true)); QVERIFY(repaint.count() > 0);
    QCOMPARE(layer->labeling(), labeling); QVERIFY(layer->labelsEnabled());
    QVERIFY(!LayerLabelControls::setVisible(layer, false, true));
    const QString id = layer->id(); const QString qgz = dir.filePath(QStringLiteral("survey.qgz"));
    QVERIFY(project.write(qgz)); project.clear(); QVERIFY(project.read(qgz));
    auto* restored = qobject_cast<QgsVectorLayer*>(project.mapLayer(id)); QVERIFY(restored);
    QVERIFY(!LayerLabelControls::describe(restored, true).enabled);
    QVERIFY(LayerLabelControls::describe(restored).enabled);
    QCOMPARE(restored->labeling()->settings().format().size(), 11.);
    QVERIFY(LayerLabelControls::setVisible(restored, true, true));
    QVERIFY(LayerLabelControls::setVisible(restored, false));
    QVERIFY(LayerLabelControls::describe(restored, true).enabled);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  LayerLabelsTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis(); return result;
}
#include "test_layer_labels.moc"
