#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "core/RecentSurveys.h"

class TestRecent : public QObject {
  Q_OBJECT
private slots:
  void rememberPutsNewestFirstAndDropsMissing();
  void recentSurveyFollowsUsbDriveLetter();
  void forgetRemovesPath();
  void lastPath_isNewestRemembered();
  void takeSkipAutoRestore_clearsOneShot();
  void bootStaysOnHome_doesNotAutoOpenLastSurvey();
  void closeEvent_asksBeforeDiscardingUnsavedWork();
  void captureTool_dragsSavedPolygonVertex();
};

void TestRecent::recentSurveyFollowsUsbDriveLetter() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString real = QDir::fromNativeSeparators(QFileInfo(dir.filePath(QStringLiteral("안동.gpkg"))).absoluteFilePath());
  QVERIFY(QFile(real).open(QIODevice::WriteOnly));
  if (real.size() < 3 || real.at(1) != QLatin1Char(':')) QSKIP("드라이브 글자는 Windows 에만 있다");
  QChar unused;
  for (char c = 'Z'; c >= 'D'; --c) {
    if (!QFileInfo::exists(QStringLiteral("%1:/").arg(QLatin1Char(c)))) {
      unused = QLatin1Char(c);
      break;
    }
  }
  QVERIFY(!unused.isNull());
  // 지난번에는 USB 가 다른 글자였다(예: F:). 지금은 real 의 드라이브에 꽂혀 있다.
  const QString before = QString(unused) + real.mid(1);
  QCOMPARE(RecentSurveys::onAnotherDrive(before, {real.left(3)}), real);
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  st.setValue(QStringLiteral("RecentSurveys/items"), QStringList{before + QStringLiteral("\t안동\t1")});
  const auto items = RecentSurveys::load(st);
  QCOMPARE(items.size(), 1);
  QCOMPARE(QDir::fromNativeSeparators(items.first().path), real);
}

void TestRecent::rememberPutsNewestFirstAndDropsMissing() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("siteA.gpkg"));
  const QString b = dir.filePath(QStringLiteral("siteB.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QVERIFY(QFile(b).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("조사A"));
  RecentSurveys::remember(st, b, QStringLiteral("조사B"));
  auto items = RecentSurveys::load(st);
  QCOMPARE(items.size(), 2);
  QCOMPARE(items.at(0).name, QStringLiteral("조사B"));
  QCOMPARE(items.at(0).path, QFileInfo(b).absoluteFilePath());
  QCOMPARE(items.at(1).name, QStringLiteral("조사A"));
  QVERIFY(QFile::remove(a));
  items = RecentSurveys::load(st);
  QCOMPARE(items.size(), 1);
  QCOMPARE(items.at(0).name, QStringLiteral("조사B"));
}

void TestRecent::takeSkipAutoRestore_clearsOneShot() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  QVERIFY(!RecentSurveys::takeSkipAutoRestore(st));
  RecentSurveys::setSkipAutoRestore(st, true);
  QVERIFY(RecentSurveys::takeSkipAutoRestore(st));
  QVERIFY(!RecentSurveys::takeSkipAutoRestore(st));
}

void TestRecent::forgetRemovesPath() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("gone.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("지울조사"));
  QCOMPARE(RecentSurveys::load(st).size(), 1);
  RecentSurveys::forget(st, a);
  QCOMPARE(RecentSurveys::load(st).size(), 0);
}

void TestRecent::lastPath_isNewestRemembered() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString a = dir.filePath(QStringLiteral("old.gpkg"));
  const QString b = dir.filePath(QStringLiteral("new.gpkg"));
  QVERIFY(QFile(a).open(QIODevice::WriteOnly));
  QVERIFY(QFile(b).open(QIODevice::WriteOnly));
  QSettings st(dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
  RecentSurveys::remember(st, a, QStringLiteral("옛조사"));
  RecentSurveys::remember(st, b, QStringLiteral("새조사"));
  QCOMPARE(RecentSurveys::lastPath(st), QFileInfo(b).absoluteFilePath());
}

void TestRecent::bootStaysOnHome_doesNotAutoOpenLastSurvey() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(!src.contains(QLatin1String("QTimer::singleShot(0, this, &MainWindow::restoreLastSurvey)")),
           "프로그램 실행만으로 마지막 조사를 자동으로 열면 안 된다");
  QVERIFY2(!src.contains(QLatin1String("applyStartupMap();\n  refreshWorkPanel();")),
           "생성자에서 지도/배경지도 초기화를 시작하면 홈 시작이 느려진다");
  QFile h(QStringLiteral("src/app/MainWindow.h"));
  QVERIFY2(h.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.h");
  const QString hdr = QString::fromUtf8(h.readAll());
  QVERIFY2(hdr.contains(QLatin1String("m_restoreLastSurveyEnabled = false")),
           "자동 복원 기본값은 꺼짐이어야 한다");
}

// 20초 자동 저장은 없앴다. 저장은 사용자가 「저장」을 누를 때만 일어난다.
// 그래서 창을 끌 때 말없이 저장해서도, 말없이 버려서도 안 된다 — 물어야 한다.
void TestRecent::closeEvent_asksBeforeDiscardingUnsavedWork() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());

  QVERIFY2(!src.contains(QLatin1String("m_autosaveTimer")),
           "20초 자동 저장 타이머는 없어야 한다");

  const int close = src.indexOf(QLatin1String("void MainWindow::closeEvent"));
  QVERIFY2(close >= 0, "closeEvent");
  const QString fn = src.mid(close, 1200);
  QVERIFY2(fn.contains(QLatin1String("surveyHasUnsavedChanges")),
           "닫기 전에 저장 안 된 작업이 있는지 봐야 한다");
  QVERIFY2(fn.contains(QLatin1String("QMessageBox::Save")) &&
               fn.contains(QLatin1String("QMessageBox::Discard")) &&
               fn.contains(QLatin1String("QMessageBox::Cancel")),
           "저장 / 저장 안 함 / 취소를 물어야 한다");
  QVERIFY2(fn.contains(QLatin1String("persistSurveyWork")),
           "저장을 고르면 실제로 저장해야 한다");
  QVERIFY2(fn.contains(QLatin1String("event->ignore()")),
           "취소를 고르면 창이 닫히면 안 된다");

  QString persistSrc;
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/MainWindowSession.cpp"),
                              QStringLiteral("src/core/SurveySession.cpp")}) {
    QFile pf(path);
    QVERIFY2(pf.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
    persistSrc += QString::fromUtf8(pf.readAll());
  }
  QVERIFY2(persistSrc.contains(QLatin1String("bool MainWindow::persistSurveyWork()")),
           "persistSurveyWork");
  QVERIFY2(persistSrc.contains(QLatin1String("SurveySession::persistWork")),
           "저장 UI는 SurveySession::persistWork 를 써야 한다");
  QFile session(QStringLiteral("src/core/SurveySession.cpp"));
  QVERIFY2(session.open(QIODevice::ReadOnly | QIODevice::Text), "SurveySession.cpp");
  const QString sessionSrc = QString::fromUtf8(session.readAll());
  QVERIFY2(sessionSrc.contains(QLatin1String("persistWorkspace")),
           "저장은 다음 세대 GPKG에 쓰고 검증한 뒤 원본을 교체해야 한다");
  QVERIFY2(persistSrc.contains(QLatin1String("lastPath")),
           "최근 목록용 lastPath는 유지한다");
  QVERIFY2(persistSrc.contains(QLatin1String("m_surveySessionReady")),
           "열기에 실패한 홈 화면이 조사 파일을 덮어쓰면 안 된다");

  // 바탕화면이 OneDrive 로 리디렉션된 PC에서 새 조사가 그리로 가지 않아야 한다.
  QVERIFY2(persistSrc.contains(QLatin1String("preferredSurveyDir()")),
           "새 조사·다른 이름으로 저장은 마지막에 쓴 조사 폴더에서 시작해야 한다");
}

void TestRecent::captureTool_dragsSavedPolygonVertex() {
  QFile f(QStringLiteral("src/app/KaCaptureMapTool.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaCaptureMapTool.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("hitSavedVertex")),
           "그린 폴리곤의 꼭짓점을 다시 집을 수 있어야 한다");
  QVERIFY2(src.contains(QLatin1String("moveFeatureVertex")) ||
               src.contains(QLatin1String("moveVertex")),
           "집은 꼭짓점을 옮겨 도형을 고쳐야 한다");
}

QTEST_GUILESS_MAIN(TestRecent)
#include "test_recent.moc"
