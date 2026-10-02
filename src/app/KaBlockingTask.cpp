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

}  // namespace

bool run(QWidget* parent, const QString& label, const std::function<bool(QgsFeedback*)>& work) {
  QgsFeedback feedback;
  QProgressDialog progress(label, QStringLiteral("취소"), 0, 0, parent);
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
    feedback.cancel();
    if (live) live->cancel();
  });
  QgsApplication::taskManager()->addTask(task);
  progress.show();
  if (!done) loop.exec();
  return ok && !feedback.isCanceled();
}

}  // namespace KaBlockingTask
