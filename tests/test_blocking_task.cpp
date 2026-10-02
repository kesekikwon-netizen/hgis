// 무거운 일(DWG 변환·DXF 읽기·변환본 쓰기)을 돌리는 동안의 진행 창 KaBlockingTask.
// 「취소」나 Esc 를 눌러도 일이 실제로 멈출 때까지 창을 닫지 않는다: 그 사이에 앱 창을 만지면
// 도면을 또 불러오거나 조사를 닫아, 아직 도는 일이 사라진 자료를 건드릴 수 있다.
#include <QAtomicInt>
#include <QDeadlineTimer>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QtTest>

#include "app/KaBlockingTask.h"

#include <qgsapplication.h>
#include <qgsfeedback.h>

namespace {

enum Phase { Waiting = 0, Started = 1, SawCancel = 2, Checked = 3 };

// 취소를 본 뒤에도 시험이 창을 확인할 때까지 멈추지 않는 일(멈추는 데 시간이 걸리는 DWG 변환처럼).
bool slowToStop(QgsFeedback* feedback, QAtomicInt* phase) {
  phase->storeRelease(Started);
  const QDeadlineTimer limit(10000);
  while (!feedback->isCanceled() && !limit.hasExpired()) QThread::msleep(5);
  phase->storeRelease(SawCancel);
  while (phase->loadAcquire() != Checked && !limit.hasExpired()) QThread::msleep(5);
  return true;
}

struct Watch {
  bool shownWhileStopping = false;
  QString label;
};

// 일이 시작되면 press 로 취소하고(대기 중인 작업은 취소하면 바로 끝나 시험이 안 된다),
// 일이 취소를 본 순간 창이 아직 보이는지 적는다.
void watchDialog(QWidget* parent, QTimer* poll, QAtomicInt* phase, Watch* watch,
                 const std::function<void(QProgressDialog*)>& press) {
  QObject::connect(poll, &QTimer::timeout, parent, [=, pressed = false]() mutable {
    auto* progress = parent->findChild<QProgressDialog*>();
    if (!progress) return;
    if (!pressed && phase->loadAcquire() >= Started) {
      pressed = true;
      press(progress);
    }
    if (phase->loadAcquire() == SawCancel) {
      watch->shownWhileStopping = progress->isVisible();
      watch->label = progress->labelText();
      phase->storeRelease(Checked);
    }
  });
  poll->start(20);
}

}  // namespace

class TestBlockingTask : public QObject {
  Q_OBJECT
 private slots:
  void cancel_keepsTheDialogUntilTheWorkStops() {
    QWidget parent;
    parent.show();
    QAtomicInt phase(Waiting);
    Watch watch;
    QTimer poll;
    watchDialog(&parent, &poll, &phase, &watch,
                [](QProgressDialog* progress) { progress->findChild<QPushButton*>()->click(); });
    const bool ok = KaBlockingTask::run(&parent, QStringLiteral("도면을 읽는 중…"),
                                        [&](QgsFeedback* feedback) { return slowToStop(feedback, &phase); });
    QVERIFY(!ok);
    QVERIFY(watch.shownWhileStopping);
    QCOMPARE(watch.label, QStringLiteral("취소하는 중…"));
  }

  void escape_cancelsAndKeepsTheDialog() {
    QWidget parent;
    parent.show();
    QAtomicInt phase(Waiting);
    Watch watch;
    QTimer poll;
    watchDialog(&parent, &poll, &phase, &watch,
                [](QProgressDialog* progress) { QTest::keyClick(progress, Qt::Key_Escape); });
    const bool ok = KaBlockingTask::run(&parent, QStringLiteral("도면을 읽는 중…"),
                                        [&](QgsFeedback* feedback) { return slowToStop(feedback, &phase); });
    QVERIFY(!ok);
    QVERIFY(watch.shownWhileStopping);
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestBlockingTask tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_blocking_task.moc"
