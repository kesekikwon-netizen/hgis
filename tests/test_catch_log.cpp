// 4-3: every src/ catch (...) must write a session log. Nested handlers are
// checked on their own body, not the inner catch.
#include "core/KaSessionLog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool isSpace(QChar c) { return c.isSpace(); }

int matchingBrace(const QString& text, int openIdx) {
  int depth = 0;
  bool escape = false;
  QChar inStr;
  const int n = text.size();
  for (int i = openIdx; i < n; ++i) {
    const QChar ch = text.at(i);
    if (!inStr.isNull()) {
      if (escape) escape = false;
      else if (ch == QLatin1Char('\\')) escape = true;
      else if (ch == inStr) inStr = QChar();
      continue;
    }
    if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
      inStr = ch;
      continue;
    }
    if (i + 1 < n && ch == QLatin1Char('/') && text.at(i + 1) == QLatin1Char('/')) {
      i = text.indexOf(QLatin1Char('\n'), i);
      if (i < 0) break;
      continue;
    }
    if (i + 1 < n && ch == QLatin1Char('/') && text.at(i + 1) == QLatin1Char('*')) {
      i = text.indexOf(QStringLiteral("*/"), i + 2);
      if (i < 0) break;
      ++i;
      continue;
    }
    if (ch == QLatin1Char('{')) ++depth;
    else if (ch == QLatin1Char('}')) {
      --depth;
      if (depth == 0) return i;
    }
  }
  return -1;
}

int openBraceAfter(const QString& text, int from) {
  int i = from;
  while (i < text.size() && isSpace(text.at(i))) ++i;
  if (i >= text.size() || text.at(i) != QLatin1Char('{')) return -1;
  return i;
}

QString stripNestedCatch(const QString& body) {
  QString out = body;
  const QRegularExpression catchRe(QStringLiteral(R"(catch\s*\(\s*\.\.\.\s*\))"));
  while (true) {
    const QRegularExpressionMatch m = catchRe.match(out);
    if (!m.hasMatch()) break;
    const int ob = openBraceAfter(out, m.capturedEnd());
    if (ob < 0) break;
    const int cb = matchingBrace(out, ob);
    if (cb < 0) break;
    out.remove(m.capturedStart(), cb - m.capturedStart() + 1);
  }
  return out;
}

bool ownBodyLogs(const QString& body) {
  const QString own = stripNestedCatch(body);
  return own.contains(QLatin1String("logLine")) || own.contains(QLatin1String("KaSessionLog::line")) ||
         own.contains(QLatin1String("appendUtf8"));
}

QStringList silentCatchSites(const QString& srcRoot) {
  QStringList silent;
  QDirIterator it(srcRoot, QStringList() << QStringLiteral("*.cpp"), QDir::Files,
                  QDirIterator::Subdirectories);
  const QRegularExpression catchRe(QStringLiteral(R"(catch\s*\(\s*\.\.\.\s*\))"));
  while (it.hasNext()) {
    const QString path = it.next();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      silent << path + QStringLiteral(":open-fail");
      continue;
    }
    const QString text = QString::fromUtf8(f.readAll());
    int from = 0;
    while (true) {
      const QRegularExpressionMatch m = catchRe.match(text, from);
      if (!m.hasMatch()) break;
      from = m.capturedEnd();
      const int ob = openBraceAfter(text, m.capturedEnd());
      if (ob < 0) {
        silent << path + QStringLiteral(":parse");
        continue;
      }
      const int cb = matchingBrace(text, ob);
      if (cb < 0) {
        silent << path + QStringLiteral(":brace");
        continue;
      }
      const QString body = text.mid(ob + 1, cb - ob - 1);
      if (!ownBodyLogs(body)) {
        const int line = text.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        silent << QStringLiteral("%1:%2").arg(path, QString::number(line));
      }
    }
  }
  return silent;
}

}  // namespace

class TestCatchLog : public QObject {
  Q_OBJECT
private slots:
  void defaultMaxBytes_is10MiB();
  void sessionLog_writesLine();
  void sessionLog_rotatesWhenOverMax();
  void srcCatchHandlers_allLog();
};

void TestCatchLog::defaultMaxBytes_is10MiB() {
  qunsetenv("KA_HGIS_LOG_MAX_BYTES");
  QCOMPARE(KaSessionLog::kDefaultMaxBytes, 10LL * 1024 * 1024);
  QCOMPARE(KaSessionLog::maxBytes(), KaSessionLog::kDefaultMaxBytes);
  const QString hint = KaSessionLog::dumpHint();
  QVERIFY2(hint.contains(QStringLiteral("10MB")), qPrintable(hint));
  QVERIFY2(hint.contains(QStringLiteral(".dmp")), qPrintable(hint));
  QVERIFY2(hint.contains(QStringLiteral("session.old.log")), qPrintable(hint));
}

void TestCatchLog::sessionLog_writesLine() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  qputenv("KA_HGIS_LOG_DIR", tmp.path().toLocal8Bit());
  KaSessionLog::line(QStringLiteral("[except] test-probe"));
  QFile log(tmp.filePath(QStringLiteral("session.log")));
  QVERIFY2(log.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(tmp.path()));
  const QString text = QString::fromUtf8(log.readAll());
  QVERIFY2(text.contains(QStringLiteral("[except] test-probe")), qPrintable(text));
}

void TestCatchLog::sessionLog_rotatesWhenOverMax() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  qputenv("KA_HGIS_LOG_DIR", tmp.path().toLocal8Bit());
  qputenv("KA_HGIS_LOG_MAX_BYTES", "2048");
  QCOMPARE(KaSessionLog::maxBytes(), 2048);

  for (int i = 0; i < 80; ++i) {
    KaSessionLog::line(QStringLiteral("[rotate-probe] %1 %2").arg(i).arg(QString(48, QLatin1Char('x'))));
  }

  QFile oldLog(tmp.filePath(QStringLiteral("session.old.log")));
  QVERIFY2(oldLog.exists() && oldLog.size() > 0, "session.old.log must appear after exceeding max");
  QFile cur(tmp.filePath(QStringLiteral("session.log")));
  QVERIFY(cur.exists());
  qunsetenv("KA_HGIS_LOG_MAX_BYTES");
}

void TestCatchLog::srcCatchHandlers_allLog() {
  const QString root = QStringLiteral("src");
  QVERIFY2(QDir(root).exists(), "CTest WORKING_DIRECTORY must be the repo root");
  const QStringList silent = silentCatchSites(root);
  QVERIFY2(silent.isEmpty(), qPrintable(silent.join(QLatin1Char('\n'))));
}

#include "test_catch_log.moc"

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  TestCatchLog test;
  return QTest::qExec(&test, argc, argv);
}
