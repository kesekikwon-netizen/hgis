// Drawing studio helpers (package C1): named sheets, layer sets, optional title
// block, vector north arrow, shared coordinate callout builder, PDF progress.
#include <QtTest>

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QProgressDialog>
#include <QSvgRenderer>
#include <QTemporaryDir>

#include "app/KaDrawingCoordCallout.h"
#include "app/KaDrawingLayerSets.h"
#include "app/KaDrawingNorthArrow.h"
#include "app/KaDrawingPdfProgress.h"
#include "app/KaDrawingSheetSet.h"
#include "app/KaTitleBlock.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitempicture.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpoint.h>
#include <qgsprintlayout.h>
#include <qgsvectordataprovider.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {
QgsVectorLayer* memoryPolygon(const QString& name) {
  return new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=survey_name:string&field=site_name:string"),
                            name, QStringLiteral("memory"));
}

QgsPrintLayout* sheet(QgsProject& project, const QString& name) {
  return dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(name));
}

QgsLayoutItemLabel* addMarker(QgsLayout* layout, const QString& text) {
  auto* label = new QgsLayoutItemLabel(layout);
  label->setId(QStringLiteral("marker"));
  label->setText(text);
  label->attemptSetSceneRect(QRectF(20, 20, 40, 10));
  layout->addLayoutItem(label);
  return label;
}
}  // namespace

class TestDrawingStudio : public QObject {
  Q_OBJECT
private slots:
  void northArrow_isVectorAndSaysGridNorth();
  void coordCallout_sharedBuilderMakesFourItems();
  void layerSets_applyOnlyWhenPicked();
  void sheetSet_storeAndReopenKeepsWorkingSheet();
  void titleBlock_isOptionalAndUsesSurveyFields();
  void pdfProgress_cancelKeepsTargetUntouched();
};

void TestDrawingStudio::northArrow_isVectorAndSaysGridNorth() {
  QCOMPARE(KaDrawingNorth::kindFromRel(QStringLiteral("arrows/NorthArrow_04.svg")), 2);
  // The picture follows grid north; the text fallback must not claim true north.
  QVERIFY(KaDrawingNorth::fallbackLabel().contains(QStringLiteral("도북")));
  QVERIFY(!KaDrawingNorth::fallbackLabel().contains(QStringLiteral("진북")));
  QVERIFY(KaDrawingNorth::gridNorthNote().contains(QStringLiteral("도북")));

  QgsProject project;
  QgsPrintLayout layout(&project);
  layout.initializeDefaults();
  for (int kind = 0; kind < 4; ++kind) {
    const QString path = KaDrawingNorth::writeSvg(kind);
    QVERIFY2(!path.isEmpty() && path.endsWith(QStringLiteral(".svg")), qPrintable(path));
    QFile file(path);
    const QByteArray svg = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    QVERIFY(svg.contains("<svg"));
    // Letters are outlines, so the file does not depend on installed fonts.
    QVERIFY2(!svg.contains("<text"), "north letters must be paths");
    QVERIFY(QSvgRenderer(path).isValid());
    auto* picture = new QgsLayoutItemPicture(&layout);
    picture->setPicturePath(path, Qgis::PictureFormat::SVG);
    picture->setMode(Qgis::PictureFormat::SVG);
    picture->setNorthMode(QgsLayoutItemPicture::GridNorth);
    layout.addLayoutItem(picture);
    picture->refreshPicture();
    QVERIFY2(!picture->isMissingImage(), qPrintable(path));
    QVERIFY(!KaDrawingNorth::pictureNeedsRebuild(picture));
    // The file name keeps the chosen style, so a rebuild on reopen keeps it too.
    QCOMPARE(KaDrawingNorth::generatedKind(picture), kind);
    QCOMPARE(KaDrawingNorth::kindFromRel(KaDrawingNorth::relForKind(kind)), kind);
    QVERIFY(!KaDrawingNorth::isLegacyRaster(picture));
  }
  QVERIFY(!KaDrawingNorth::previewIcon(1).isNull());
}

void TestDrawingStudio::coordCallout_sharedBuilderMakesFourItems() {
  QCOMPARE(KaDrawingCoordCallout::tagFor(0), QStringLiteral("A"));
  QCOMPARE(KaDrawingCoordCallout::tagFor(27), QStringLiteral("B"));
  // Survey convention: X is the northing (map Y), Y is the easting (map X).
  QCOMPARE(KaDrawingCoordCallout::xyText(200123.4, 550987.6),
           QStringLiteral("X=550987.600\nY=200123.400"));

  QgsProject project;
  QgsPrintLayout layout(&project);
  layout.initializeDefaults();
  const QPointF tip(100.0, 80.0);
  const QRectF box = KaDrawingCoordCallout::clickBoxRect(tip, 210.0);
  QVERIFY(box.left() > tip.x());
  const QRectF nearEdge = KaDrawingCoordCallout::clickBoxRect(QPointF(200.0, 80.0), 210.0);
  QVERIFY2(nearEdge.right() < 200.0, "box flips to the left near the paper edge");
  KaDrawingCoordCallout::add(&layout, tip, QStringLiteral("A"),
                             KaDrawingCoordCallout::xyText(1.0, 2.0), box);
  for (const char* kind : {"box", "let", "arr", "head"})
    QVERIFY2(layout.itemById(QStringLiteral("ka_coord_%1_A").arg(QLatin1String(kind))), kind);
  auto* label = dynamic_cast<QgsLayoutItemLabel*>(layout.itemById(QStringLiteral("ka_coord_box_A")));
  QVERIFY(label);
  QCOMPARE(label->text(), QStringLiteral("X=2.000\nY=1.000"));

  const QVector<QRectF> used{KaDrawingCoordCallout::freeBoxRect(tip, {})};
  const QRectF second = KaDrawingCoordCallout::freeBoxRect(tip, used);
  QVERIFY2(!second.adjusted(-2, -2, 2, 2).intersects(used.first()), "imported boxes do not overlap");

  KaDrawingCoordCallout::removeAll(&layout);
  QList<QgsLayoutItem*> items;
  layout.layoutItems(items);
  for (QgsLayoutItem* item : items)
    QVERIFY(!item->id().startsWith(QStringLiteral("ka_coord_")));
}

void TestDrawingStudio::layerSets_applyOnlyWhenPicked() {
  QgsProject project;
  auto* a = memoryPolygon(QStringLiteral("유구"));
  auto* b = memoryPolygon(QStringLiteral("지형"));
  QVERIFY(a->isValid() && b->isValid());
  project.addMapLayers({a, b});
  QgsLayerTreeModel model(project.layerTreeRoot());
  auto* nodeA = project.layerTreeRoot()->findLayer(a->id());
  auto* nodeB = project.layerTreeRoot()->findLayer(b->id());
  QVERIFY(nodeA && nodeB);
  nodeA->setItemVisibilityChecked(true);
  nodeB->setItemVisibilityChecked(false);
  QVERIFY(KaDrawingLayerSets::save(&project, QStringLiteral("유구 배치"), &model));
  QVERIFY(KaDrawingLayerSets::names(&project).contains(QStringLiteral("유구 배치")));
  // Saving records only; nothing is switched.
  QVERIFY(nodeA->itemVisibilityChecked() && !nodeB->itemVisibilityChecked());

  nodeA->setItemVisibilityChecked(false);
  nodeB->setItemVisibilityChecked(true);
  QVERIFY(KaDrawingLayerSets::apply(&project, QStringLiteral("유구 배치"), &model));
  QVERIFY(nodeA->itemVisibilityChecked());
  QVERIFY(!nodeB->itemVisibilityChecked());

  QVERIFY(!KaDrawingLayerSets::apply(&project, QStringLiteral("없는 세트"), &model));
  QVERIFY(!KaDrawingLayerSets::save(&project, QStringLiteral("  "), &model));
  QVERIFY(KaDrawingLayerSets::remove(&project, QStringLiteral("유구 배치")));
  QVERIFY(KaDrawingLayerSets::names(&project).isEmpty());
}

void TestDrawingStudio::sheetSet_storeAndReopenKeepsWorkingSheet() {
  QgsProject project;
  const QString working = QStringLiteral("user_sheet");
  QString error;
  QVERIFY(!LayoutService::createBlankSheet(&project, 210.0, 297.0, working, &error).isEmpty());
  addMarker(sheet(project, working), QStringLiteral("first"));

  QVERIFY2(KaDrawingSheetSet::store(&project, working, QStringLiteral("조사구역도"), &error), qPrintable(error));
  const auto entries = KaDrawingSheetSet::list(&project);
  QCOMPARE(entries.size(), 1);
  QCOMPARE(entries.first().title, QStringLiteral("조사구역도"));
  QVERIFY(entries.first().layoutName != working);
  auto* stored = sheet(project, entries.first().layoutName);
  QVERIFY(stored);
  // A stored copy is never a submission sheet by itself.
  QVERIFY(!stored->customProperty(QStringLiteral("ka_hgis/user_composed"), true).toBool());

  auto* marker = dynamic_cast<QgsLayoutItemLabel*>(sheet(project, working)->itemById(QStringLiteral("marker")));
  QVERIFY(marker);
  marker->setText(QStringLiteral("second"));
  QgsPrintLayout* restored = KaDrawingSheetSet::restore(&project, QStringLiteral("조사구역도"), working, &error);
  QVERIFY2(restored, qPrintable(error));
  QCOMPARE(sheet(project, working), restored);
  int workingCount = 0;
  for (QgsPrintLayout* layout : project.layoutManager()->printLayouts())
    if (layout->name() == working) ++workingCount;
  QCOMPARE(workingCount, 1);
  auto* back = dynamic_cast<QgsLayoutItemLabel*>(restored->itemById(QStringLiteral("marker")));
  QVERIFY(back);
  QCOMPARE(back->text(), QStringLiteral("first"));
  QVERIFY(restored->customProperty(QStringLiteral("ka_hgis/user_composed")).toBool());
  QVERIFY(!restored->customProperty(QStringLiteral("ka_hgis/saved_sheet"), false).toBool());
  QVERIFY(KaDrawingSheetSet::contains(&project, QStringLiteral("조사구역도")));

  QVERIFY(KaDrawingSheetSet::remove(&project, QStringLiteral("조사구역도")));
  QVERIFY(KaDrawingSheetSet::list(&project).isEmpty());
  QVERIFY(sheet(project, working));
  QVERIFY(!KaDrawingSheetSet::restore(&project, QStringLiteral("조사구역도"), working, &error));
  QVERIFY(sheet(project, working));
}

void TestDrawingStudio::titleBlock_isOptionalAndUsesSurveyFields() {
  QgsProject project;
  auto* area = memoryPolygon(QStringLiteral("조사구역"));
  QVERIFY(area->isValid());
  LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
  QgsFeature feature(area->fields());
  feature.setAttribute(QStringLiteral("survey_name"), QStringLiteral("왕복조사"));
  feature.setAttribute(QStringLiteral("site_name"), QStringLiteral("테스트유적"));
  QVERIFY(area->dataProvider()->addFeature(feature));
  project.addMapLayer(area);

  const KaTitleBlock::Info info = KaTitleBlock::collect(&project);
  QCOMPARE(info.surveyName, QStringLiteral("왕복조사"));
  QCOMPARE(info.siteName, QStringLiteral("테스트유적"));
  const QString text = KaTitleBlock::text(QStringLiteral("유구배치도"), info, QDate(2026, 9, 29),
                                          QStringLiteral("ka_map"));
  QVERIFY(text.contains(QStringLiteral("도면명   유구배치도")));
  QVERIFY(text.contains(QStringLiteral("조사명   왕복조사")));
  QVERIFY(text.contains(QStringLiteral("유적명   테스트유적")));
  QVERIFY(text.contains(QStringLiteral("item_variables('ka_map')")));
  QVERIFY(text.contains(QStringLiteral("2026-09-29")));
  QVERIFY(KaTitleBlock::text(QString(), {}, QDate(), QStringLiteral("ka_map")).contains(QStringLiteral("―")));

  QString error;
  QVERIFY(!LayoutService::createBlankSheet(&project, 210.0, 297.0, QStringLiteral("user_sheet"), &error).isEmpty());
  auto* layout = sheet(project, QStringLiteral("user_sheet"));
  // Default sheet stays map frame only.
  QVERIFY(!layout->itemById(KaTitleBlock::itemId()));
  const QRectF page(0, 0, 210, 297);
  const QRectF map(10, 10, 190, 249);
  const QRectF rect = KaTitleBlock::defaultRect(page, map);
  QVERIFY2(page.contains(rect), "title block stays on the paper");
  auto* block = KaTitleBlock::place(layout, rect, QStringLiteral("유구배치도"), text);
  QVERIFY(block);
  QCOMPARE(KaTitleBlock::drawingTitleOf(layout), QStringLiteral("유구배치도"));
  const QPointF moved(30.0, 40.0);
  block->attemptMove(QgsLayoutPoint(moved));
  auto* again = KaTitleBlock::place(layout, rect, QStringLiteral("조사구역도"), QStringLiteral("x"));
  QCOMPARE(again, block);
  QCOMPARE(block->text(), QStringLiteral("x"));
  QVERIFY2(QLineF(block->pos(), moved).length() < 0.01, "updating text keeps the user's position");
}

void TestDrawingStudio::pdfProgress_cancelKeepsTargetUntouched() {
  QVERIFY(!KaDrawingPdfProgress::renderNote(210, 297, 300).contains(QStringLiteral("큰 용지")));
  QVERIFY(KaDrawingPdfProgress::renderNote(841, 1189, 300).contains(QStringLiteral("큰 용지")));
  QVERIFY(KaDrawingPdfProgress::renderNote(210, 297, 300).contains(QStringLiteral("300 DPI")));

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString draft = dir.filePath(QStringLiteral("draft.pdf"));
  const QString target = dir.filePath(QStringLiteral("target.pdf"));
  auto write = [](const QString& path, const QByteArray& bytes) {
    QFile out(path);
    return out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size();
  };
  QVERIFY(write(draft, "%PDF-1.7 draft") && write(target, "old"));
  {
    KaDrawingPdfProgress progress(nullptr, QStringLiteral("PDF 내보내기"));
    QVERIFY(progress.step(QStringLiteral("도면을 정리하는 중…")));
    QProgressDialog* dialog = nullptr;
    for (QWidget* widget : QApplication::topLevelWidgets())
      if (widget->objectName() == QStringLiteral("drawingPdfProgress"))
        dialog = qobject_cast<QProgressDialog*>(widget);
    QVERIFY(dialog);
    dialog->cancel();
    QVERIFY(!progress.step(QStringLiteral("PDF를 그리는 중…")));
    QVERIFY(progress.cancelled());
  }
  auto read = [](const QString& path) {
    QFile in(path);
    return in.open(QIODevice::ReadOnly) ? in.readAll() : QByteArray();
  };
  QCOMPARE(read(target), QByteArray("old"));
  QString error;
  QVERIFY2(KaDrawingPdfProgress::commit(draft, target, &error), qPrintable(error));
  QCOMPARE(read(target), QByteArray("%PDF-1.7 draft"));
  QVERIFY(!KaDrawingPdfProgress::cancelledText().isEmpty());
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::initQgis();
  int result = 0;
  { TestDrawingStudio test; result = QTest::qExec(&test, argc, argv); }
  QgsApplication::exitQgis();
  return result;
}

#include "test_drawing_studio.moc"
