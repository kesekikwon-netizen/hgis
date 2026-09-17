#include <QtTest>
#include <QFile>
#include <cmath>

#include "core/BufferAnalysis.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>

class TestBuffer : public QObject {
  Q_OBJECT
private slots:
  void ringHasGapAndLabel();
  void offsetIs500mFromSource();
  void moveFeatureVertex_updatesPolygonCorner();
  void setLabelsVisible_togglesPolygonLabels();
  void clipLayerByBoundary_clipsIntersectingFeatures();
  void splitPolygonWithLine_splitsGeometry();
  void splitTwoOverlappingFeatures_splitsTargetPolygon();
};

void TestBuffer::ringHasGapAndLabel() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* src = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("site"),
                                 QStringLiteral("memory"));
  QVERIFY(src->isValid());
  QVERIFY(src->startEditing());
  QgsFeature f(src->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200080, 450060)));
  QVERIFY(src->addFeature(f));
  QVERIFY(src->commitChanges());
  proj.addMapLayer(src);

  QString err;
  QVERIFY2(BufferAnalysis::addDistanceRing(&proj, nullptr, src, 500.0, &err), qPrintable(err));
  QgsVectorLayer* ring = nullptr;
  QgsVectorLayer* lab = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    const QString k = LayerOps::layerKeyOf(l);
    if (k == QLatin1String("user:buffer_ring_500"))
      ring = qobject_cast<QgsVectorLayer*>(l);
    if (k == QLatin1String("user:buffer_label_500"))
      lab = qobject_cast<QgsVectorLayer*>(l);
  }
  QVERIFY(ring);
  QVERIFY(lab);
  QCOMPARE(int(ring->featureCount()), 1);
  QCOMPARE(int(lab->featureCount()), 4);
  bool dashed = true;
  LayerOps::readSimpleVectorStyle(ring, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  &dashed);
  QVERIFY2(!dashed, "ring must be a solid line; dash is a 도형 색 option");
  QgsFeature lf;
  QVERIFY(lab->getFeatures().nextFeature(lf));
  QCOMPARE(lf.attribute(QStringLiteral("label")).toString(), QStringLiteral("500m"));

  const QgsRectangle ext = ring->extent();
  QVERIFY2(std::abs(ext.xMinimum() - 199500) < 12.0, qPrintable(QString::number(ext.xMinimum())));
  QVERIFY2(std::abs(ext.xMaximum() - 200580) < 12.0, qPrintable(QString::number(ext.xMaximum())));
  QVERIFY2(std::abs(ext.yMinimum() - 449500) < 12.0, qPrintable(QString::number(ext.yMinimum())));
  QVERIFY2(std::abs(ext.yMaximum() - 450560) < 12.0, qPrintable(QString::number(ext.yMaximum())));
}

void TestBuffer::offsetIs500mFromSource() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* src = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("site"),
                                 QStringLiteral("memory"));
  QVERIFY(src->isValid());
  QVERIFY(src->startEditing());
  QgsFeature f(src->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200026, 450026)));
  QVERIFY(src->addFeature(f));
  QVERIFY(src->commitChanges());
  proj.addMapLayer(src);
  QString err;
  QVERIFY2(BufferAnalysis::addDistanceRing(&proj, nullptr, src, 500.0, &err), qPrintable(err));
  QgsVectorLayer* ring = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (LayerOps::layerKeyOf(l) == QLatin1String("user:buffer_ring_500"))
      ring = qobject_cast<QgsVectorLayer*>(l);
  }
  QVERIFY(ring);
  QgsFeature srcF;
  QVERIFY(src->getFeatures().nextFeature(srcF));
  const QgsGeometry site = srcF.geometry();
  QgsFeature rf;
  QVERIFY(ring->getFeatures().nextFeature(rf));
  const QgsGeometry ringG = rf.geometry();
  QVERIFY(!ringG.isEmpty());
  // Vertices of a rectangular offset are the corners (~500√2). Measure the
  // line itself and samples along it — that is the 500m distance.
  const double minD = site.distance(ringG);
  QVERIFY2(minD > 490.0 && minD < 510.0, qPrintable(QStringLiteral("minD=%1").arg(minD)));
  double maxD = minD;
  const double len = ringG.length();
  QVERIFY(len > 100.0);
  for (int i = 0; i <= 48; ++i) {
    const QgsGeometry pt = ringG.interpolate(len * double(i) / 48.0);
    if (pt.isEmpty()) continue;
    const double d = site.distance(pt);
    if (d > maxD) maxD = d;
  }
  QVERIFY2(maxD >= 500.0 && maxD < 720.0, qPrintable(QStringLiteral("maxD=%1").arg(maxD)));
}

void TestBuffer::moveFeatureVertex_updatesPolygonCorner() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("poly"),
                                QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 10, 10)));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  QgsFeature got;
  QVERIFY(vl->getFeatures().nextFeature(got));
  int at = -1, before = -1, after = -1;
  double d2 = 0;
  got.geometry().closestVertex(QgsPointXY(10, 10), at, before, after, d2);
  QVERIFY(at >= 0);
  QString err;
  QVERIFY(vl->startEditing());
  QVERIFY2(LayerOps::moveFeatureVertex(vl, static_cast<qint64>(got.id()), at, 20.0, 10.0, &err),
           qPrintable(err));
  QVERIFY(vl->commitChanges());
  const QgsFeature afterF = vl->getFeature(got.id());
  QVERIFY(afterF.isValid());
  const QgsRectangle ext = afterF.geometry().boundingBox();
  QVERIFY2(std::abs(ext.xMaximum() - 20.0) < 1e-6, qPrintable(QString::number(ext.xMaximum())));
}

void TestBuffer::setLabelsVisible_togglesPolygonLabels() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("조사구역"),
                                QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markSurveyLayer(vl, QStringLiteral("survey_area"));
  QVERIFY2(LayerOps::hasToggleableLabels(vl),
           "라벨을 아직 안 켠 폴리곤에도 우클릭 「글자 켜기」가 나와야 한다");
  QVERIFY(LayerOps::setLabelsVisible(vl, true));
  QVERIFY(LayerOps::labelsVisible(vl));
  QVERIFY(LayerOps::setLabelsVisible(vl, false));
  QVERIFY2(!LayerOps::labelsVisible(vl), "글자 끄기는 면은 두고 글자만 꺼야 한다");
  QVERIFY(LayerOps::setLabelsVisible(vl, true));
  QVERIFY2(LayerOps::labelsVisible(vl), "글자 켜기는 같은 레이어 라벨을 다시 켜야 한다");
  delete vl;
}

void TestBuffer::clipLayerByBoundary_clipsIntersectingFeatures() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));

  // 대상 레이어: (0, 0) ~ (100, 100) 면적 10000
  auto* src = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("source_roads"),
                                 QStringLiteral("memory"));
  QVERIFY(src->isValid());
  QVERIFY(src->startEditing());
  QgsFeature sf(src->fields());
  sf.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 100, 100)));
  QVERIFY(src->addFeature(sf));
  QVERIFY(src->commitChanges());
  proj.addMapLayer(src);

  // 바운더리 레이어: (0, 0) ~ (50, 100) - 절반만 겹침
  auto* bnd = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("survey_boundary"),
                                 QStringLiteral("memory"));
  QVERIFY(bnd->isValid());
  QVERIFY(bnd->startEditing());
  QgsFeature bf(bnd->fields());
  bf.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 50, 100)));
  QVERIFY(bnd->addFeature(bf));
  QVERIFY(bnd->commitChanges());
  proj.addMapLayer(bnd);

  QString err;
  QgsVectorLayer* clipped = LayerOps::clipLayerByBoundary(src, bnd, &proj, &err);
  QVERIFY2(clipped != nullptr, qPrintable(err));
  QCOMPARE(int(clipped->featureCount()), 1);

  QgsFeature cf;
  QVERIFY(clipped->getFeatures().nextFeature(cf));
  QVERIFY(cf.hasGeometry());
  // 클립된 지오메트리 면적은 대략 5000이어야 함
  const double area = cf.geometry().area();
  QVERIFY2(std::abs(area - 5000.0) < 1.0, qPrintable(QStringLiteral("클립 면적 오류: %1").arg(area)));
}

void TestBuffer::splitPolygonWithLine_splitsGeometry() {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("poly_layer"),
                                  QStringLiteral("memory"));
  QVERIFY(layer->isValid());
  QVERIFY(layer->startEditing());
  QgsFeature f(layer->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 100, 100)));
  QVERIFY(layer->addFeature(f));
  QVERIFY(layer->commitChanges());
  QCOMPARE(int(layer->featureCount()), 1);

  // x=50을 세로로 관통하는 분할선: (50, -10) ~ (50, 110)
  QVector<QgsPointXY> splitLine;
  splitLine.append(QgsPointXY(50, -10));
  splitLine.append(QgsPointXY(50, 110));

  QString err;
  bool ok = LayerOps::splitPolygonWithLine(layer, splitLine, &err);
  QVERIFY2(ok, qPrintable(err));
  // 분할 후 피처가 2개여야 함
  QCOMPARE(int(layer->featureCount()), 2);

  double totalArea = 0.0;
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature pf;
  while (it.nextFeature(pf)) {
    QVERIFY(pf.hasGeometry());
    const double a = pf.geometry().area();
    QVERIFY2(std::abs(a - 5000.0) < 1.0, qPrintable(QStringLiteral("분할된 파트 면적 오류: %1").arg(a)));
    totalArea += a;
  }
  QVERIFY2(std::abs(totalArea - 10000.0) < 1.0, qPrintable(QStringLiteral("전체 면적 합 오류: %1").arg(totalArea)));

  delete layer;
}

void TestBuffer::splitTwoOverlappingFeatures_splitsTargetPolygon() {
  // 바운더리 레이어: (0, 0) ~ (50, 100)
  auto* bnd = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("260813 림하면 바운더리"),
                                 QStringLiteral("memory"));
  QVERIFY(bnd->isValid());
  QVERIFY(bnd->startEditing());
  QgsFeature bf(bnd->fields());
  bf.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 50, 100)));
  QVERIFY(bnd->addFeature(bf));
  QVERIFY(bnd->commitChanges());
  QgsFeature actualBf;
  QVERIFY(bnd->getFeatures().nextFeature(actualBf));
  qint64 bndFid = actualBf.id();

  // 대상 도면 레이어: (0, 0) ~ (100, 100)
  auto* road = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("쉽게그리기"),
                                  QStringLiteral("memory"));
  QVERIFY(road->isValid());
  QVERIFY(road->startEditing());
  QgsFeature rf(road->fields());
  rf.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 100, 100)));
  QVERIFY(road->addFeature(rf));
  QVERIFY(road->commitChanges());
  QgsFeature actualRf;
  QVERIFY(road->getFeatures().nextFeature(actualRf));
  qint64 roadFid = actualRf.id();

  QString err;
  qint64 createdFid = -1;
  QgsVectorLayer* outLayer = nullptr;
  bool ok = LayerOps::splitTwoOverlappingFeatures(bnd, bndFid, road, roadFid, &createdFid, &outLayer, &err);
  QVERIFY2(ok, qPrintable(err));
  QCOMPARE(outLayer, road);
  QVERIFY(createdFid >= 0);

  // 원본 A·B는 유지하고 교차 구간만 새 피처로 추가한다.
  QCOMPARE(int(road->featureCount()), 2);
  QCOMPARE(int(bnd->featureCount()), 1);

  QgsFeature origCheck = road->getFeature(roadFid);
  QVERIFY(origCheck.isValid());
  QVERIFY2(std::abs(origCheck.geometry().area() - 10000.0) < 1.0, "원본 도형 면적은 그대로 10000");

  QgsFeature bndCheck = bnd->getFeature(bndFid);
  QVERIFY(bndCheck.isValid());
  QVERIFY2(std::abs(bndCheck.geometry().area() - 5000.0) < 1.0, "원본 바운더리 면적은 그대로 5000");

  QgsFeature newCheck = road->getFeature(createdFid);
  QVERIFY(newCheck.isValid());
  QVERIFY2(std::abs(newCheck.geometry().area() - 5000.0) < 1.0, "새로 분리된 교차 도형 면적이 5000이어야 함");

  QVERIFY(LayerOps::undoCommittedFeature(road, createdFid, &err));
  QCOMPARE(int(road->featureCount()), 1);

  // 피처 복원(restoreDeletedFeature) 테스트
  QVERIFY(LayerOps::restoreDeletedFeature(road, newCheck, &err));
  QCOMPARE(int(road->featureCount()), 2);

  delete bnd;
  delete road;
}

#include "test_buffer.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestBuffer tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
