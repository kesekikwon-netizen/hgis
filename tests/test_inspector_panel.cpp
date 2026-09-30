// P4 map-panels, right side: the 「선택한 유구」 panel (three tabs, the hosted feature card,
// the empty/drawing sentences, its width) and the 「배경 지도」 card that only asks and shows.
// KA_HGIS_QA_OUTPUT_DIR saves inspector-panel.png.
#include <QtTest>

#include <QAbstractButton>
#include <QDir>
#include <QFocusEvent>
#include <QLabel>
#include <QLineEdit>
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
  void tabs_areAttrStyleAndPhoto() {
    KaInspectorPanel panel;
    QTabBar* tabs = panel.tabs();
    QCOMPARE(tabs->objectName(), QStringLiteral("inspectorTabs"));
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("속성"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("스타일"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("사진"));
    for (int i = 0; i < 3; ++i) QVERIFY(!tabs->tabIcon(i).isNull());
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

  void styleTab_buttonEmits() {
    KaInspectorPanel panel;
    QSignalSpy asked(&panel, &KaInspectorPanel::styleEditRequested);
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("inspectorStyleEdit"));
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("레이어 스타일 편집…"));
    QVERIFY(sentenceOf(panel.page(1))->text().contains(QStringLiteral("표시 설정")));
    button->click();
    QCOMPARE(asked.count(), 1);
    QSignalSpy fold(&panel, &KaInspectorPanel::collapseRequested);
    panel.findChild<QToolButton*>(QStringLiteral("inspectorCollapse"))->click();
    QCOMPARE(fold.count(), 1);
  }

  void photoTab_isSentenceNotControl() {
    KaInspectorPanel panel;
    QWidget* photo = panel.page(2);
    QVERIFY(sentenceOf(photo));
    QCOMPARE(sentenceOf(photo)->text(),
             QStringLiteral("이 판에는 사진 첨부가 없습니다. 조사카드 메모에 사진 파일 이름을 적어 두세요."));
    QVERIFY(photo->findChildren<QAbstractButton*>().isEmpty());
    QVERIFY(photo->findChildren<QLineEdit*>().isEmpty());
    QVERIFY(photo->findChildren<QPlainTextEdit*>().isEmpty());
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
