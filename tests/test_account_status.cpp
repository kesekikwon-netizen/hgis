#include <QtTest>

#include "core/AccountStatus.h"

// F172: one read-only view of which keys/accounts are set. The probe is injected so the
// test never reads the user's settings, registry or account files.
class AccountStatusTest : public QObject {
  Q_OBJECT
private slots:
  void listsEverySourceInMenuOrder() {
    const auto entries = AccountStatus::snapshot([](AccountStatus::Source) { return false; });
    QCOMPARE(entries.size(), 5);
    QCOMPARE(entries.at(0).source, AccountStatus::Source::VworldKey);
    QCOMPARE(entries.at(1).source, AccountStatus::Source::HistoryGisKey);
    QCOMPARE(entries.at(2).source, AccountStatus::Source::CadastralAccount);
    QCOMPARE(entries.at(3).source, AccountStatus::Source::TopographicAccount);
    QCOMPARE(entries.at(4).source, AccountStatus::Source::HeritageAccount);
    for (const auto& e : entries) {
      QVERIFY(!e.label.isEmpty());
      QVERIFY(!e.usedFor.isEmpty());
      // The existing menu locations stay the entry points.
      QVERIFY2(e.menuPath.startsWith(QStringLiteral("더보기 → ")), qPrintable(e.menuPath));
      QVERIFY(!e.ready);
    }
  }

  void summaryNamesOnlyMissingSources() {
    const auto entries = AccountStatus::snapshot([](AccountStatus::Source s) {
      return s == AccountStatus::Source::VworldKey || s == AccountStatus::Source::HeritageAccount;
    });
    QCOMPARE(AccountStatus::readyCount(entries), 2);
    const QString line = AccountStatus::summaryLine(entries);
    QVERIFY2(line.startsWith(QStringLiteral("자료 연결 5곳 중 2곳 설정됨")), qPrintable(line));
    QVERIFY(line.contains(QStringLiteral("수치지형도 계정")));
    QVERIFY(line.contains(QStringLiteral("VWorld 지적도 계정")));
    QVERIFY(!line.contains(QStringLiteral("국가유산 인트라넷 계정")));
    const auto all = AccountStatus::snapshot([](AccountStatus::Source) { return true; });
    QCOMPARE(AccountStatus::summaryLine(all), QStringLiteral("자료 연결 5곳 중 5곳 설정됨"));
  }

  void detailLinesUseStateWordsOnly() {
    const auto entries = AccountStatus::snapshot([](AccountStatus::Source s) {
      return s == AccountStatus::Source::TopographicAccount;
    });
    const QStringList lines = AccountStatus::detailLines(entries);
    QCOMPARE(lines.size(), 5);
    QCOMPARE(lines.at(3), QStringLiteral("수치지형도 계정 — 설정됨 (더보기 → 수치지형도 아이디·비밀번호)"));
    QVERIFY(lines.at(0).contains(AccountStatus::stateText(false)));
    QCOMPARE(AccountStatus::stateText(true), QStringLiteral("설정됨"));
    QCOMPARE(AccountStatus::stateText(false), QStringLiteral("미설정"));
  }

  void nullProbeReportsMissing() {
    const auto entries = AccountStatus::snapshot(AccountStatus::Probe());
    QCOMPARE(AccountStatus::readyCount(entries), 0);
  }
};

QTEST_GUILESS_MAIN(AccountStatusTest)
#include "test_account_status.moc"
