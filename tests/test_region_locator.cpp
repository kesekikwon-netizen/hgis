#include <QtTest>
#include <QComboBox>
#include <QDir>
#include <QFrame>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <algorithm>

#include "app/KaBeginnerRibbon.h"
#include "app/KaRegionLocator.h"
#include "app/KaTheme.h"
#include "core/KoreaRegionCatalog.h"

class RegionLocatorTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
#ifdef Q_OS_WIN
    const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      QVERIFY2(QFontDatabase::addApplicationFont(path) >= 0, qPrintable(path));
    }
#endif
    KaTheme::apply(qApp);
  }

  void popupFitsAvailableScreen_data() {
    QTest::addColumn<QRect>("available");
    QTest::addColumn<QRect>("anchor");
    QTest::newRow("right-edge") << QRect(0, 0, 1920, 1040) << QRect(1640, 60, 234, 74);
    QTest::newRow("negative-left-monitor") << QRect(-1920, 0, 1920, 1040)
        << QRect(-260, 60, 234, 74);
    QTest::newRow("upper-monitor") << QRect(0, -1080, 1920, 1040)
        << QRect(1670, -1080, 234, 74);
    QTest::newRow("logical-high-dpi-screen") << QRect(1920, 0, 1280, 680)
        << QRect(2950, 60, 234, 74);
    QTest::newRow("bottom-edge") << QRect(0, 0, 800, 560) << QRect(550, 470, 234, 74);
    QTest::newRow("small-screen-reflow") << QRect(0, 0, 360, 640) << QRect(110, 40, 234, 74);
  }

  void provinceChipRequestsNavigationAndCanBeCancelled() {
    KaRegionLocator locator;
    QSignalSpy navigation(&locator, &KaRegionLocator::regionSelected);
    QSignalSpy search(&locator, &KaRegionLocator::searchRequested);
    const auto chips = locator.findChildren<QToolButton*>();
    const auto it = std::find_if(chips.begin(), chips.end(), [](QToolButton* button) {
      return button->text() == QStringLiteral("부산");
    });
    QVERIFY(it != chips.end());
    (*it)->click();
    QCOMPARE(navigation.count(), 1);
    QCOMPARE(navigation.first().first().toString(), QStringLiteral("부산광역시"));
    QCOMPARE(search.count(), 0);
    QVERIFY(locator.m_popup->isVisible());
    QVERIFY((*it)->isChecked());
    (*it)->click();
    QCOMPARE(navigation.count(), 1);
    QVERIFY(!locator.m_popup->isVisible());
    QVERIFY(!(*it)->isChecked());
  }

  void addressPopupOffersOnlySearchAndCancelActions() {
    KaRegionLocator locator;
    locator.openAddressPopup(QStringLiteral("부산광역시"));
    const auto buttons = locator.m_popup->findChildren<QPushButton*>();
    QStringList actions;
    for (QPushButton* button : buttons) actions.append(button->text());
    actions.sort();
    QStringList expected{QStringLiteral("찾기"), QStringLiteral("취소")};
    expected.sort();
    QCOMPARE(actions, expected);
    locator.closePanel();
  }

  void overviewContainsRepresentativeCity_data() {
    QTest::addColumn<QString>("sido");
    QTest::addColumn<QPointF>("city");
    QTest::newRow("seoul") << QStringLiteral("서울") << QPointF(126.9780, 37.5665);
    QTest::newRow("busan") << QStringLiteral("부산") << QPointF(129.0756, 35.1796);
    QTest::newRow("daegu") << QStringLiteral("대구") << QPointF(128.6014, 35.8714);
    QTest::newRow("incheon") << QStringLiteral("인천") << QPointF(126.7052, 37.4563);
    QTest::newRow("gwangju") << QStringLiteral("광주") << QPointF(126.8526, 35.1595);
    QTest::newRow("daejeon") << QStringLiteral("대전") << QPointF(127.3845, 36.3504);
    QTest::newRow("ulsan") << QStringLiteral("울산") << QPointF(129.3114, 35.5395);
    QTest::newRow("sejong") << QStringLiteral("세종") << QPointF(127.2890, 36.4800);
    QTest::newRow("gyeonggi-suwon") << QStringLiteral("경기") << QPointF(127.0286, 37.2636);
    QTest::newRow("gangwon-chuncheon") << QStringLiteral("강원") << QPointF(127.7298, 37.8813);
    QTest::newRow("chungbuk-cheongju") << QStringLiteral("충북") << QPointF(127.4890, 36.6424);
    QTest::newRow("chungnam-hongseong") << QStringLiteral("충남") << QPointF(126.6608, 36.6012);
    QTest::newRow("jeonbuk-jeonju") << QStringLiteral("전북") << QPointF(127.1480, 35.8242);
    QTest::newRow("jeonnam-mokpo") << QStringLiteral("전남") << QPointF(126.3922, 34.8118);
    QTest::newRow("gyeongbuk-andong") << QStringLiteral("경북") << QPointF(128.7294, 36.5684);
    QTest::newRow("gyeongnam-changwon") << QStringLiteral("경남") << QPointF(128.6811, 35.2278);
    QTest::newRow("jeju") << QStringLiteral("제주") << QPointF(126.5312, 33.4996);
    QTest::newRow("daegu-gunwi") << QStringLiteral("대구광역시") << QPointF(128.5730, 36.2428);
  }

  void overviewContainsRepresentativeCity() {
    QFETCH(QString, sido);
    QFETCH(QPointF, city);
    const auto bounds = KoreaRegionCatalog::overviewBounds(sido);
    QVERIFY(bounds.has_value());
    QVERIFY(bounds->west <= city.x() && city.x() <= bounds->east);
    QVERIFY(bounds->south <= city.y() && city.y() <= bounds->north);
    // Region navigation must frame a region, not a single geocoded street block.
    QVERIFY(bounds->east - bounds->west > 0.1);
    QVERIFY(bounds->north - bounds->south > 0.1);
    QVERIFY(bounds->east - bounds->west < 4.0);
    QVERIFY(bounds->north - bounds->south < 3.0);
  }

  void overviewRejectsUnrecognizedRegion() {
    QVERIFY(!KoreaRegionCatalog::overviewBounds(QString()));
    QVERIFY(!KoreaRegionCatalog::overviewBounds(QStringLiteral("부산광역시 중구")));
    QVERIFY(!KoreaRegionCatalog::overviewBounds(QStringLiteral("unknown")));
  }

  void popupFitsAvailableScreen() {
    QFETCH(QRect, available);
    QFETCH(QRect, anchor);
    KaRegionLocator locator;
    locator.openAddressPopup(QStringLiteral("부산광역시"));
    locator.placeAddressPopup(anchor, available);
    locator.m_popup->layout()->activate();
    QVERIFY2(available.contains(locator.m_popup->geometry()),
             qPrintable(QStringLiteral("popup=%1,%2 %3x%4")
               .arg(locator.m_popup->x()).arg(locator.m_popup->y())
               .arg(locator.m_popup->width()).arg(locator.m_popup->height())));
    for (QWidget* control : locator.m_addressControls) {
      QVERIFY(control->isVisibleTo(locator.m_popup));
      const QRect controlRect(control->mapTo(locator.m_popup, QPoint()), control->size());
      QVERIFY(locator.m_popup->rect().contains(controlRect));
      QVERIFY(control->width() >= control->minimumSizeHint().width());
    }
    // The same popup must return to a single row after moving to a wider screen.
    if (available.width() < 500) {
      QVERIFY(locator.m_addressControls.at(3)->y() > locator.m_addressControls.at(0)->y());
      locator.placeAddressPopup(QRect(1500, 60, 234, 74), QRect(0, 0, 1920, 1040));
      locator.m_popup->layout()->activate();
      const QRect input = locator.m_addressControls.at(0)->geometry();
      const QRect button = locator.m_addressControls.at(3)->geometry();
      QVERIFY(input.top() <= button.bottom() && button.top() <= input.bottom());
    }
    locator.closePanel();
  }

  void actualChipUsesItsScreenAndPreservesAddressInput() {
    QWidget host;
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    host.setGeometry(available.adjusted(10, 10, -10, -10));
    auto* locator = new KaRegionLocator(&host);
    locator->resize(locator->sizeHint());
    locator->move(host.width() - locator->width() - 5, 5);
    host.show();
    QCoreApplication::processEvents();
    const auto chips = locator->findChildren<QToolButton*>();
    auto it = std::find_if(chips.begin(), chips.end(), [](QToolButton* button) {
      return button->text() == QStringLiteral("부산");
    });
    QVERIFY(it != chips.end());
    (*it)->click();
    QCoreApplication::processEvents();
    QVERIFY(available.contains(locator->m_popup->geometry()));
    QSignalSpy search(locator, &KaRegionLocator::parcelSearchRequested);
    QSignalSpy generic(locator, &KaRegionLocator::searchRequested);
    locator->m_city->setCurrentIndex(1);
    locator->m_dong->setEditText(QStringLiteral("중앙동"));
    locator->m_lot->setText(QStringLiteral("123-4"));

    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QVERIFY(locator->m_popup->grab().save(QDir(output).filePath(QStringLiteral("address-popup-after.png"))));
    }
    locator->emitSearch();
    QCOMPARE(search.count(), 1);
    QCOMPARE(generic.count(), 0);
    const QString address = search.first().first().toString();
    QVERIFY(address.contains(QStringLiteral("부산광역시")));
    QVERIFY(address.contains(QStringLiteral("중앙동")));
    QVERIFY(address.contains(QStringLiteral("123-4")));
    QVERIFY(!locator->m_popup->isVisible());
    QVERIFY(!(*it)->isChecked());
  }

  void overflowFindMenuKeepsRegionPopupOnScreen() {
    KaBeginnerRibbon ribbon;
    ribbon.addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
    ribbon.addGroup(QStringLiteral("find"), QStringLiteral("찾기"));
    auto* locator = new KaRegionLocator(&ribbon);
    ribbon.addWidget(QStringLiteral("find"), locator);
    auto* extra = new QToolButton(&ribbon);
    extra->setText(QStringLiteral("웹자료"));
    ribbon.addWidget(QStringLiteral("find"), extra);
    ribbon.resize(120, ribbon.sizeHint().height());
    ribbon.show();
    QCoreApplication::processEvents();
    auto* overflow = ribbon.findChild<QToolButton*>(QStringLiteral("ribbonOverflow"));
    auto* menu = ribbon.findChild<QMenu*>(QStringLiteral("ribbonOverflowMenu"));
    auto* find = ribbon.findChild<QMenu*>(QStringLiteral("ribbonOverflowGroup_find"));
    QVERIFY(overflow && overflow->isVisible());
    QVERIFY(menu && find && find->menuAction()->isVisible());
    menu->popup(overflow->mapToGlobal(QPoint(0, overflow->height())));
    menu->setActiveAction(find->menuAction());
    QTest::keyClick(menu, Qt::Key_Right);
    QTRY_VERIFY(find->isVisible());
    QTRY_VERIFY(locator->isVisible());
    const auto chips = locator->findChildren<QToolButton*>();
    auto it = std::find_if(chips.begin(), chips.end(), [](QToolButton* button) {
      return button->text() == QStringLiteral("부산");
    });
    QVERIFY(it != chips.end());
    QVERIFY2((*it)->height() <= 28 && (*it)->width() <= 48,
             qPrintable(QStringLiteral("chip %1x%2 menu %3x%4 locator %5x%6")
                            .arg((*it)->width()).arg((*it)->height())
                            .arg(find->width()).arg(find->height())
                            .arg(locator->width()).arg(locator->height())));
    const QPoint chipBottom = (*it)->mapTo(find, (*it)->rect().bottomRight());
    QVERIFY2(find->rect().contains(chipBottom),
             qPrintable(QStringLiteral("chip clipped at %1,%2 menu %3x%4")
                            .arg(chipBottom.x()).arg(chipBottom.y())
                            .arg(find->width()).arg(find->height())));
    (*it)->click();
    QCoreApplication::processEvents();
    QVERIFY(locator->m_popup);
    QVERIFY2(locator->m_popup->isVisible(), "address popup closed as soon as the overflow menu opened it");
    QVERIFY2(!qobject_cast<QMenu*>(locator->m_popup->parentWidget()),
             qPrintable(QStringLiteral("popup parent=%1")
                            .arg(locator->m_popup->parentWidget()
                                     ? locator->m_popup->parentWidget()->metaObject()->className()
                                     : "null")));
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    QVERIFY2(available.contains(locator->m_popup->frameGeometry()),
             qPrintable(QStringLiteral("popup=%1,%2 %3x%4")
                            .arg(locator->m_popup->x()).arg(locator->m_popup->y())
                            .arg(locator->m_popup->width()).arg(locator->m_popup->height())));
    QVERIFY(locator->m_city && locator->m_city->isVisibleTo(locator->m_popup));
    locator->closePanel();
  }

  void emptyLotKeepsRegionSearch() {
    KaRegionLocator locator;
    locator.openAddressPopup(QStringLiteral("경상북도"));
    QSignalSpy generic(&locator, &KaRegionLocator::searchRequested);
    QSignalSpy parcel(&locator, &KaRegionLocator::parcelSearchRequested);
    locator.m_dong->setEditText(QStringLiteral("고아읍 봉한리"));
    locator.emitSearch();
    QCOMPARE(generic.count(), 1);
    QCOMPARE(parcel.count(), 0);
  }
};

QTEST_MAIN(RegionLocatorTest)
#include "test_region_locator.moc"
