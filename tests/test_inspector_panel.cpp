// P4 map-panels, right side: the 「선택한 유구」 panel (two tabs, the hosted feature card,
// the empty/drawing sentences, the in-tab style editor, its width) and the 「배경 지도」 card.
// KA_HGIS_QA_OUTPUT_DIR saves inspector-panel.png.
#include <QtTest>

#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

#include "app/KaBasemapQuickCard.h"
#include "app/KaChip.h"
#include "app/KaFeatureCard.h"
#include "app/KaInspectorPanel.h"
#include "app/KaInspectorStyle.h"
#include "app/KaTheme.h"
#include "core/FeaturePresets.h"
#include "core/LayerOps.h"

namespace {
std::unique_ptr<QgsVectorLayer> houseLayer() {
  auto layer = std::make_unique<QgsVectorLayer>(
      QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string&field=feature_no:string&field=note:string"),
      QStringLiteral("유구"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), QStringLiteral("feature_poly"));
  QgsFeature f(layer->fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("주거지"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  f.setAttribute(QStringLiteral("feature_no"), QStringLiteral("1호"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450005)));
  layer->dataProvider()->addFeature(f);
  return layer;
}

QgsFeatureId firstId(QgsVectorLayer* layer) {
  QgsFeature f;
  layer->getFeatures().nextFeature(f);
  return f.id();
}

QLabel* sentenceOf(QWidget* page) { return page->findChild<QLabel*>(QStringLiteral("inspectorSentence")); }
}  // namespace

class TestInspectorPanel : public QObject {
  Q_OBJECT
 private slots:
  // 사진 is gone: nothing in the app stores photos (user 2026-10-03 「추천진행」).
  void tabs_areAttrAndStyle() {
    KaInspectorPanel panel;
    QTabBar* tabs = panel.tabs();
    QCOMPARE(tabs->objectName(), QStringLiteral("inspectorTabs"));
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0), QStringLiteral("속성"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("스타일"));
    for (int i = 0; i < 2; ++i) QVERIFY(!tabs->tabIcon(i).isNull());
    QCOMPARE(panel.objectName(), QStringLiteral("inspectorPanel"));
    QCOMPARE(panel.title(), QStringLiteral("선택한 유구"));
  }

  void attrTab_hostsFeatureCard() {
    KaInspectorPanel panel;
    auto* card = new KaFeatureCard;  // built elsewhere, adopted by the panel
    panel.setFeatureCard(card);
    QCOMPARE(card->parentWidget(), panel.attributeHost());
    QVERIFY(panel.attributeLayout()->indexOf(card) >= 0);
    QCOMPARE(panel.featureCard(), card);
    auto layer = houseLayer();
    panel.resize(300, 640);
    panel.show();
    card->setFeature(layer.get(), firstId(layer.get()));
    QVERIFY(!card->isHidden());
    QVERIFY(card->findChild<QLabel*>(QStringLiteral("kaFeatureCardTitle"))->isHidden());
    QCOMPARE(panel.title(), QStringLiteral("선택한 유구"));
    QCOMPARE(panel.sentence(), QString());
    panel.setSelectionCount(1);
    auto* count = panel.findChild<KaChip*>(QStringLiteral("inspectorCount"));
    QVERIFY(count && !count->isHidden());
    QCOMPARE(count->text(), QStringLiteral("1개 선택"));
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    QTest::qWait(50);  // let the posted LayoutRequest settle before the capture
    if (!out.isEmpty()) QVERIFY(panel.grab().save(QDir(out).filePath(QStringLiteral("inspector-panel.png"))));
    card->clear();
    QVERIFY(card->isHidden());
    QCOMPARE(panel.sentence(), QStringLiteral("지도에서 도형 하나를 고르면 기록이 여기에 나옵니다."));
  }

  void emptyAndDrawingSentences() {
    KaInspectorPanel panel;
    auto* card = new KaFeatureCard;
    panel.setFeatureCard(card);
    panel.show();
    QCOMPARE(panel.sentence(), QStringLiteral("지도에서 도형 하나를 고르면 기록이 여기에 나옵니다."));
    panel.setDrawing(true);
    QCOMPARE(panel.sentence(), QStringLiteral("그리는 동안에는 기록을 고치지 않습니다. 도형을 마치면 여기서 이어집니다."));
    auto layer = houseLayer();
    card->setFeature(layer.get(), firstId(layer.get()));
    QVERIFY(!card->isHidden());  // the card is never hidden for drawing
    QVERIFY(panel.sentence().startsWith(QStringLiteral("그리는 동안")));
    panel.setDrawing(false);
    QCOMPARE(panel.sentence(), QString());
  }

  void lockingCardKeepsTypedNote() {
    KaInspectorPanel panel;
    auto* card = new KaFeatureCard;
    panel.setFeatureCard(card);
    auto layer = houseLayer();
    panel.show();
    card->setFeature(layer.get(), firstId(layer.get()));
    auto* note = card->findChild<QPlainTextEdit*>(QStringLiteral("kaFeatureCardNote"));
    QVERIFY(note);
    QSignalSpy edited(card, &KaFeatureCard::edited);
    note->setFocus();
    note->setPlainText(QStringLiteral("북벽 교란"));
    card->setEnabled(false);  // the drawing lock (MainWindow::syncFeatureCard) while the note still has the focus
    // Qt disables the editor before its FocusOut arrives; an offscreen window may deliver none, so send what Qt would.
    if (edited.isEmpty()) {
      QFocusEvent leave(QEvent::FocusOut, Qt::OtherFocusReason);
      QCoreApplication::sendEvent(note, &leave);
    }
    QCOMPARE(edited.count(), 1);
    QCOMPARE(layer->getFeature(firstId(layer.get())).attribute(QStringLiteral("note")).toString(), QStringLiteral("북벽 교란"));
    QVERIFY(!note->isEnabled() && layer->isModified());  // locked, and the value is in the edit buffer, not lost
    layer->rollBack();
  }

  // 스타일 changes the chosen layer right in the tab: no extra button, no window (user 2026-10-03
  // 「바로 수정 편집할수있는거도 아니고」). Without a layer it says how to pick one.
  void styleTab_editsTheChosenLayerInPlace() {
    KaInspectorPanel panel;
    KaInspectorStyle* style = panel.styleEditor();
    QVERIFY(style && panel.page(1)->isAncestorOf(style));
    QVERIFY(!panel.findChild<QPushButton*>(QStringLiteral("inspectorStyleEdit")));
    QVERIFY(sentenceOf(style) && !sentenceOf(style)->isHidden());
    auto layer = houseLayer();
    style->setLayer(layer.get());
    QVERIFY(sentenceOf(style)->isHidden());
    const auto swatches = style->findChildren<QPushButton*>(QStringLiteral("inspectorStyleSwatch"));
    QCOMPARE(swatches.size(), 8);
    QSignalSpy applied(style, &KaInspectorStyle::styleApplied);
    swatches.at(4)->click();  // 파랑
    QColor fill, stroke;
    double width = 0, marker = 0;
    QVERIFY(LayerOps::readSimpleVectorStyle(layer.get(), &fill, &stroke, &width, &marker));
    QCOMPARE(stroke.name(), QStringLiteral("#2563eb"));
    QVERIFY(fill.rgb() == stroke.rgb() && fill.alpha() < 255);  // a see-through fill of the same colour
    auto* spin = style->findChild<QDoubleSpinBox*>(QStringLiteral("inspectorStyleWidth"));
    QVERIFY(spin);
    spin->setValue(2.4);
    QCOMPARE(layer->customProperty(QStringLiteral("ka_hgis/style_width_mm")).toDouble(), 2.4);  // remembered
    QCOMPARE(applied.count(), 2);
    // A colour set elsewhere (right-click 「면·외곽선 색」) is not undone by a later width change here.
    LayerOps::applySimpleVectorStyle(layer.get(), QColor(QStringLiteral("#dc2626")), QColor(QStringLiteral("#dc2626")), 2.4);
    spin->setValue(3.0);
    QVERIFY(LayerOps::readSimpleVectorStyle(layer.get(), &fill, &stroke, &width, &marker));
    QCOMPARE(stroke.name(), QStringLiteral("#dc2626"));
    QVERIFY2(style->minimumSizeHint().width() <= 272 - 24, qPrintable(QString::number(style->minimumSizeHint().width())));
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!out.isEmpty()) {
      panel.resize(300, 640);
      panel.tabs()->setCurrentIndex(1);
      panel.show();
      QTest::qWait(50);
      QVERIFY(panel.grab().save(QDir(out).filePath(QStringLiteral("inspector-style.png"))));
    }
    style->setLayer(nullptr);
    QVERIFY(!sentenceOf(style)->isHidden());
    layer->setCustomProperty(QString::fromLatin1(LayerOps::kPropLayerRole), QString::fromLatin1(LayerOps::kRoleReference));
    style->setLayer(layer.get());  // a line map: its colour is the right-click 「면·외곽선 색」 row
    QVERIFY(sentenceOf(style)->text().contains(QStringLiteral("「면·외곽선 색」")));
    QSignalSpy fold(&panel, &KaInspectorPanel::collapseRequested);
    panel.findChild<QToolButton*>(QStringLiteral("inspectorCollapse"))->click();
    QCOMPARE(fold.count(), 1);
  }

  // A survey area shows and edits 조사명·유적명 instead of 「이름 없음」 (user 2026-10-03 「속성도 안나오고」).
  void surveyAreaCardShowsSurveyAndSiteNames() {
    QgsVectorLayer layer(QStringLiteral("Polygon?crs=EPSG:5187&field=survey_name:string&field=site_name:string&field=note:string"),
                         QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(&layer, QStringLiteral("survey_area"));
    QgsFeature f(layer.fields());
    f.setAttribute(QStringLiteral("survey_name"), QStringLiteral("영천 시굴조사"));
    f.setAttribute(QStringLiteral("site_name"), QStringLiteral("가수리 유적"));
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450005)));
    QVERIFY(layer.dataProvider()->addFeature(f));
    KaFeatureCard card;
    card.setFeature(&layer, firstId(&layer));
    QCOMPARE(card.rowText(QStringLiteral("header")), QStringLiteral("영천 시굴조사"));
    QCOMPARE(card.rowText(QStringLiteral("survey_name")), QStringLiteral("영천 시굴조사"));
    QCOMPARE(card.rowText(QStringLiteral("site_name")), QStringLiteral("가수리 유적"));
    QVERIFY(layer.startEditing());
    QVERIFY(card.setValue(QStringLiteral("site_name"), QStringLiteral("가수리 유적 2지점")));
    QCOMPARE(card.rowText(QStringLiteral("site_name")), QStringLiteral("가수리 유적 2지점"));
  }

  // 「채우기 없음 (선만)」 and 「선 없음」 are in the tab too (user 2026-10-03 「선채우기없음도 나와야한다」).
  void styleTab_fillAndOutlineCanBeTurnedOff() {
    KaInspectorStyle style;
    auto layer = houseLayer();
    style.setLayer(layer.get());
    auto* noFill = style.findChild<QCheckBox*>(QStringLiteral("inspectorStyleNoFill"));
    auto* noStroke = style.findChild<QCheckBox*>(QStringLiteral("inspectorStyleNoStroke"));
    QVERIFY(noFill && noStroke && !noFill->isHidden() && !noStroke->isHidden());
    QColor fill, stroke;
    double width = 0, marker = 0;
    bool fillOff = false, strokeOff = false;
    noFill->click();  // outline only, at once and remembered
    QVERIFY(LayerOps::readSimpleVectorStyle(layer.get(), &fill, &stroke, &width, &marker, &fillOff, &strokeOff));
    QVERIFY(fillOff && !strokeOff);
    noFill->click();
    noStroke->click();
    QVERIFY(LayerOps::readSimpleVectorStyle(layer.get(), &fill, &stroke, &width, &marker, &fillOff, &strokeOff));
    QVERIFY(!fillOff && strokeOff);
  }

  void preferredWidth_1366And1920() {
    KaInspectorPanel panel;
    QCOMPARE(KaInspectorPanel::preferredWidth(1366), 272);
    QCOMPARE(KaInspectorPanel::preferredWidth(1499), 272);
    QCOMPARE(KaInspectorPanel::preferredWidth(1600), 300);
    QCOMPARE(KaInspectorPanel::preferredWidth(1920), 300);
    QCOMPARE(panel.minimumWidth(), 260);
    QCOMPARE(panel.maximumWidth(), 380);
  }

  void basemapCard_emitsIdsOnlyAndAddsNoLayer() {
    QgsProject project;
    const int layers = project.mapLayers().size();
    KaBasemapQuickCard card;
    QSignalSpy asked(&card, &KaBasemapQuickCard::basemapRequested);
    for (const QString& id : KaBasemapQuickCard::ids()) {
      QToolButton* button = card.button(id);
      QVERIFY2(button, qPrintable(id));
      button->click();
      QVERIFY2(!button->isChecked(), qPrintable(id));  // shows only what the window confirms
    }
    QCOMPARE(asked.count(), 3);
    QCOMPARE(asked.at(0).first().toString(), QStringLiteral("satellite"));
    QCOMPARE(asked.at(1).first().toString(), QStringLiteral("terrain"));
    QCOMPARE(asked.at(2).first().toString(), QStringLiteral("old_map"));
    QCOMPARE(project.mapLayers().size(), layers);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    QCOMPARE(card.button(QStringLiteral("satellite"))->text(), QStringLiteral("위성"));
    QCOMPARE(card.button(QStringLiteral("terrain"))->text(), QStringLiteral("지형"));
    QCOMPARE(card.button(QStringLiteral("old_map"))->text(), QStringLiteral("옛 지도"));
  }

  void basemapCard_setCheckedReflects() {
    KaBasemapQuickCard card;
    QSignalSpy asked(&card, &KaBasemapQuickCard::basemapRequested);
    card.setChecked(QStringLiteral("satellite"), true);
    QVERIFY(card.isChecked(QStringLiteral("satellite")));
    QVERIFY(card.button(QStringLiteral("satellite"))->isChecked());
    QVERIFY(!card.isChecked(QStringLiteral("terrain")));
    card.setChecked(QStringLiteral("satellite"), false);
    QVERIFY(!card.isChecked(QStringLiteral("satellite")));
    QCOMPARE(asked.count(), 0);  // mirroring never asks
  }

  void basemapCard_sentenceIsFactual() {
    KaBasemapQuickCard card;
    QVERIFY(card.sentence().contains(QStringLiteral("조사를 열면 위성과 지적이 올라옵니다")));
    QVERIFY(!card.sentence().contains(QStringLiteral("자동으로 올리지 않습니다")));
    QCOMPARE(card.findChild<QLabel*>(QStringLiteral("basemapQuickTitle"))->text(), QStringLiteral("배경 지도"));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  FeaturePresets::instance().ensureLoaded();
  KaTheme::apply(&app);  // the capture shows the Strata chrome, not Fusion
  TestInspectorPanel test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_inspector_panel.moc"
