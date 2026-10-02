// P4 map-panels, layer list side: section bands painted by a wrapping delegate (no tree
// change), the base row height the five-row minimum counts in, the 「레이어 찾기」 filter and
// the display-only 「편집 중」 pencil. KA_HGIS_QA_OUTPUT_DIR saves layer-list-sections.png.
#include <QtTest>

#include <QDir>
#include <QHeaderView>
#include <QHelpEvent>
#include <QLineEdit>
#include <QSplitter>
#include <QStyleOptionViewItem>

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertreeviewindicator.h>
#include <qgsrectangle.h>

#include "app/KaLayerListChrome.h"
#include "app/KaTheme.h"
#include "layer_list_fixture.h"

using namespace LayerListFixture;
using Section = KaLayerSectionDelegate::Section;

namespace {
// Records what the wrapped (QGIS) delegate receives.
class SpyDelegate : public QStyledItemDelegate {
 public:
  QRect helpRect;
  QRect editorRect;
  bool helpEvent(QHelpEvent*, QAbstractItemView*, const QStyleOptionViewItem& option, const QModelIndex&) override {
    helpRect = option.rect;
    return true;
  }
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override {
    editorRect = option.rect;
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }
};

QPoint checkSpot(const QRect& cell, int band) { return QPoint(cell.left() + 10, cell.top() + band + (cell.height() - band) / 2); }
}  // namespace

class TestLayerListChrome : public QObject {
  Q_OBJECT
 private slots:
  void sections_bandWhereRootSectionChanges() {
    Tree t;
    View v(t);
    QCOMPARE(v.bandRows(), (QList<int>{0, 2, 3}));
    QCOMPARE(KaLayerSectionDelegate::sectionOf(t.node(t.area)), Section::Survey);
    QCOMPARE(KaLayerSectionDelegate::sectionOf(t.node(t.cadastral)), Section::Nearby);
    QCOMPARE(KaLayerSectionDelegate::sectionOf(t.node(t.satellite)), Section::Basemap);
    QCOMPARE(KaLayerSectionDelegate::sectionTitle(Section::Survey), QStringLiteral("조사 데이터"));
    QCOMPARE(KaLayerSectionDelegate::sectionTitle(Section::Nearby), QStringLiteral("주변·참조"));
    QCOMPARE(KaLayerSectionDelegate::sectionTitle(Section::Basemap), QStringLiteral("배경 지도"));
    const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!out.isEmpty()) QVERIFY(v.host.grab().save(QDir(out).filePath(QStringLiteral("layer-list-sections.png"))));
  }

  void sections_noBandInsideGroups() {
    Tree t;
    View v(t);
    const QModelIndex river = v.index(t.node(t.river));
    QVERIFY(river.isValid() && river.parent().isValid());
    QVERIFY(!v.sections()->hasBand(river));
    QCOMPARE(v.sections()->bandHeight(river), 0);
  }

  void sections_groupNodeIsItsOwnHeader() {
    Tree t;
    View v(t);
    QCOMPARE(KaLayerSectionDelegate::sectionOf(t.refs), Section::None);
    QVERIFY(!v.sections()->hasBand(v.index(t.refs)));
  }

  void sections_cadastralStaysAtRoot() {
    Tree t;
    View v(t);
    QCOMPARE(t.node(t.cadastral)->parent(), t.project.layerTreeRoot());
    QCOMPARE(t.project.layerTreeRoot()->children().size(), 6);  // no group was created for the bands
    QCOMPARE(t.project.layerTreeRoot()->findGroup(QStringLiteral("주변·참조")), nullptr);
  }

  void sections_unmarkedUserFileIsNearby() {
    Tree t;
    QgsVectorLayer* user = vector(t.project, t.project.layerTreeRoot(), QStringLiteral("사용자 SHP"));
    QVERIFY(LayerOps::layerKeyOf(user).isEmpty());
    QCOMPARE(KaLayerSectionDelegate::sectionOf(t.node(user)), Section::Nearby);
  }

  void delegate_addsBandHeightOnlyOnBandRows() {
    Tree t;
    View v(t);
    // Row 0 carries a band; row 1 is the plain 10 px row (baseRowHeight clamps it to >= 22).
    QCOMPARE(v.view->sizeHintForRow(0) - v.view->sizeHintForRow(1), KaLayerSectionDelegate::kBandHeight);
    QCOMPARE(v.view->baseRowHeight(), qMax(22, v.view->sizeHintForRow(1)));
    QStyleOptionViewItem option;
    option.widget = v.view;
    QCOMPARE(v.sections()->sizeHint(option, v.row(0).siblingAtColumn(1)).height(),
             v.sections()->sizeHint(option, v.row(0)).height());
  }

  void delegate_checkClickStillTogglesVisibility() {
    Tree t;
    View v(t);
    QVERIFY(QTest::qWaitForWindowExposed(&v.host));
    QgsLayerTreeNode* node = t.node(t.area);
    QVERIFY(node->isVisible());
    const QRect cell = v.view->visualRect(v.index(node));
    QVERIFY(cell.height() > KaLayerSectionDelegate::kBandHeight);
    QTest::mouseClick(v.view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(cell.left() + 10, cell.top() + 4));
    QTest::qWait(30);
    QVERIFY(node->isVisible());  // a click in the band changes nothing
    QTest::mouseClick(v.view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      checkSpot(cell, KaLayerSectionDelegate::kBandHeight));
    QTRY_VERIFY(!node->isVisible());
    QTest::mouseClick(v.view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      checkSpot(cell, KaLayerSectionDelegate::kBandHeight));
    QTRY_VERIFY(node->isVisible());
  }

  void delegate_helpEventReachesInner() {
    Tree t;
    View v(t);
    auto* spy = new SpyDelegate;
    spy->setParent(v.view);
    v.view->setItemDelegate(new KaLayerSectionDelegate(spy, v.view));
    const QModelIndex first = v.row(0).siblingAtColumn(1);
    QStyleOptionViewItem option;
    option.widget = v.view;
    option.rect = v.view->visualRect(first);
    QHelpEvent help(QEvent::ToolTip, option.rect.center(), v.view->viewport()->mapToGlobal(option.rect.center()));
    QVERIFY(v.view->itemDelegate()->helpEvent(&help, v.view, option, first));
    QCOMPARE(spy->helpRect.top(), option.rect.top() + KaLayerSectionDelegate::kBandHeight);
    QCOMPARE(spy->helpRect.height(), option.rect.height() - KaLayerSectionDelegate::kBandHeight);
  }

  void baseRowHeight_staysAtTenPixelFont() {
    Tree t;
    View v(t);
    QCOMPARE(v.view->font().pixelSize(), 10);
    QCOMPARE(v.view->baseRowHeight(), qMax(22, v.view->sizeHintForRow(1)));
    QVERIFY2(v.view->baseRowHeight() <= 28, qPrintable(QString::number(v.view->baseRowHeight())));
  }

  void minimumListHeight_usesBaseRow() {
    Tree t;
    View v(t);
    const int head = qMax(v.view->header()->sizeHint().height(), v.view->fontMetrics().height() + 6);
    const int expected = head + v.view->baseRowHeight() * KaLayerInformationView::kMinVisibleRows + v.view->frameWidth() * 2;
    QCOMPARE(v.view->minimumListHeight(), expected);  // five 10 px rows, not five band rows
    QSplitter split(Qt::Vertical);
    KaLayerInformationView::protectSidebarList(&split, v.view, nullptr, nullptr, nullptr);
    QCOMPARE(v.view->minimumHeight(), expected);
  }

  void filter_hidesNonMatchingKeepsGroupOfMatch() {
    Tree t;
    View v(t);
    KaLayerListChrome chrome(v.view, &v.host);
    chrome.setFilterText(QStringLiteral("수계"));
    QCOMPARE(v.view->model()->rowCount(), 1);
    QCOMPARE(v.view->index2node(v.row(0)), t.refs);
    QCOMPARE(v.view->model()->rowCount(v.row(0)), 1);
    QVERIFY(!v.view->dragEnabled());
    chrome.setFilterText(QString());
    QCOMPARE(v.view->model()->rowCount(), 6);
    QVERIFY(v.view->dragEnabled());
  }

  void filter_emptySentenceShown() {
    Tree t;
    View v(t);
    KaLayerListChrome chrome(v.view, &v.host);
    QVERIFY(!chrome.isEmptySentenceShown());
    chrome.setFilterText(QStringLiteral("없는 이름"));
    // This QGIS proxy keeps the empty 「참조 지도」 group row; the sentence counts layer rows.
    QCOMPARE(chrome.visibleLayerCount(), 0);
    QVERIFY2(chrome.isEmptySentenceShown(), qPrintable(chrome.emptySentence()));
    QCOMPARE(chrome.emptySentence(), QStringLiteral("「없는 이름」에 맞는 레이어가 없습니다."));
    chrome.setFilterText(QString());
    QVERIFY(!chrome.isEmptySentenceShown());
    QCOMPARE(chrome.visibleLayerCount(), 6);
    chrome.filterEdit()->setText(QStringLiteral("없는 이름"));
    QTRY_VERIFY(chrome.isEmptySentenceShown());  // 120 ms debounce
    chrome.filterEdit()->clear();
    QTRY_VERIFY(!chrome.isEmptySentenceShown());
    QCOMPARE(v.view->model()->rowCount(), 6);
  }

  void filter_clearsCurrentWhenHidden() {
    Tree t;
    View v(t);
    KaLayerListChrome chrome(v.view, &v.host);
    v.view->setCurrentLayer(t.poly);
    QCOMPARE(v.view->currentLayer(), t.poly);
    chrome.setFilterText(QStringLiteral("지적"));
    QTRY_VERIFY(!v.view->currentLayer());
    QVERIFY(!v.view->currentIndex().isValid());
    v.view->setCurrentLayer(t.cadastral);  // a shown row may stay current
    chrome.setFilterText(QStringLiteral("지적도"));
    QCOMPARE(v.view->currentLayer(), t.cadastral);
    QTest::keyClick(chrome.filterEdit(), Qt::Key_Escape);
    QCOMPARE(chrome.filterText(), QString());
    QCOMPARE(v.view->model()->rowCount(), 6);
  }

  void editIndicator_pencilWhileModified() {
    Tree t;
    View v(t);
    KaLayerListChrome chrome(v.view, &v.host);
    QgsLayerTreeNode* node = t.node(t.poly);
    QVERIFY(t.poly->startEditing());
    QCOMPARE(v.view->indicators(node).size(), 0);  // editable but untouched: no pencil
    QgsFeature f(t.poly->fields());
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 1, 1)));
    QVERIFY(t.poly->addFeature(f));
    QTRY_COMPARE(v.view->indicators(node).size(), 1);
    QCOMPARE(v.view->indicators(node).first(), chrome.editIndicator());
    QCOMPARE(v.view->indicators(t.node(t.area)).size(), 0);
    QVERIFY(t.poly->rollBack());
    QTRY_COMPARE(v.view->indicators(node).size(), 0);
  }

  void killSwitch_propertyDisablesBands() {
    Tree t;
    View v(t);
    const int base = v.view->baseRowHeight();
    v.view->setProperty("kaLayerSections", false);
    QVERIFY(!v.sections()->enabled());
    QVERIFY(v.bandRows().isEmpty());
    QCOMPARE(v.view->sizeHintForRow(0), v.view->sizeHintForRow(1));
    QCOMPARE(v.view->baseRowHeight(), base);
    v.view->setProperty("kaLayerSections", true);
    QCOMPARE(v.bandRows(), (QList<int>{0, 2, 3}));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  KaTheme::apply(&app);  // the capture shows the Strata list (10 px rows, stripe), not Fusion
  TestLayerListChrome test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_layer_list_chrome.moc"
