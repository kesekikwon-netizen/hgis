#include "KaBlockingTask.h"

#include <QEventLoop>
#include <QPointer>
#include <QProgressDialog>

#include <qgsapplication.h>
#include <qgsfeedback.h>
#include <qgstaskmanager.h>

namespace KaBlockingTask {
namespace {

class Task final : public QgsTask {
 public:
  Task(const QString& label, const std::function<bool(QgsFeedback*)>& work, QgsFeedback* feedback)
      : QgsTask(label, QgsTask::CanCancel), m_work(work), m_feedback(feedback) {}

  void cancel() override {
    m_feedback->cancel();
    QgsTask::cancel();
  }

 protected:
  bool run() override { return m_work(m_feedback); }

 private:
  std::function<bool(QgsFeedback*)> m_work;
  QgsFeedback* m_feedback;
};

// 「취소」·Esc·창 닫기는 canceled 만 알리고 창은 그대로 둔다. 창은 일이 실제로 멈춰 run 이 끝날 때 닫힌다:
// 그 사이에 앱 창을 만지면 도면을 또 불러오거나 조사를 닫아, 아직 도는 일이 사라진 자료를 건드릴 수 있다.
class Progress final : public QProgressDialog {
 public:
  Progress(const QString& label, QWidget* parent) : QProgressDialog(label, QStringLiteral("취소"), 0, 0, parent) {
    disconnect(this, &QProgressDialog::canceled, this, nullptr);  // 기본 canceled → cancel() 은 창을 바로 숨긴다
  }
  void reject() override { emit canceled(); }
};

}  // namespace

bool run(QWidget* parent, const QString& label, const std::function<bool(QgsFeedback*)>& work) {
  QgsFeedback feedback;
  Progress progress(label, parent);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  QEventLoop loop;
  bool done = false;
  bool ok = false;
  auto* task = new Task(label, work, &feedback);
  const QPointer<QgsTask> live(task);  // 끝난 작업은 작업 관리자가 지운다
  QObject::connect(task, &QgsTask::taskCompleted, &loop, [&] {
    ok = true;
    done = true;
    loop.quit();
  });
  QObject::connect(task, &QgsTask::taskTerminated, &loop, [&] {
    done = true;
    loop.quit();
  });
  QObject::connect(&progress, &QProgressDialog::canceled, &loop, [&] {
    progress.setLabelText(QStringLiteral("취소하는 중…"));
    feedback.cancel();
    if (live) live->cancel();
  });
  QgsApplication::taskManager()->addTask(task);
  progress.show();
  if (!done) loop.exec();
  return ok && !feedback.isCanceled();
}

}  // namespace KaBlockingTask
