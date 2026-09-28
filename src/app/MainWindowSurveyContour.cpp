#include "MainWindow.h"

#include "KaSurveyContourDialog.h"
#include "KaUserError.h"
#include "core/LayerOps.h"
#include "core/SurveyContourBuilder.h"
#include "core/SurveyContourStyle.h"
#include "core/SurveyPointReader.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStatusBar>

#include <qgsapplication.h>
#include <qgsmaplayer.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgstaskmanager.h>

namespace {

QString safeStem(QString name) {
  name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
  return name.isEmpty() ? QStringLiteral("측량") : name;
}

QString surveyAreaClipWkt(const QString& crsAuthId) {
  QgsGeometry area;
  const QgsCoordinateReferenceSystem dest(crsAuthId);
  for (QgsVectorLayer* layer : LayerOps::findAllByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
    if (!layer) continue;
    QgsFeature feature;
    QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setNoAttributes());
    while (it.nextFeature(feature)) {
      if (!feature.hasGeometry()) continue;
      QgsGeometry geometry = feature.geometry();
      if (layer->crs().isValid() && dest.isValid() && layer->crs().authid() != dest.authid()) {
        try {
          const QgsCoordinateTransform transform(layer->crs(), dest, QgsProject::instance()->transformContext());
          geometry.transform(transform);
        } catch (const QgsCsException&) {
          continue;
        }
      }
      area = area.isNull() ? geometry : area.combine(geometry);
    }
  }
  return area.isEmpty() ? QString() : area.asWkt();
}

class SurveyContourTask final : public QgsTask {
public:
  SurveyContourTask(SurveyContourJob job, std::function<void(const SurveyContourResult&)> done)
      : QgsTask(QStringLiteral("측량 등고선"), QgsTask::CanCancel),
        m_job(std::move(job)), m_done(std::move(done)) {}

protected:
  bool run() override {
    const SurveyReadReport report = SurveyPointReader::read(m_job.read);
    if (isCanceled()) {
      m_result.canceled = true;
      m_result.error = QStringLiteral("등고선 만들기를 취소했습니다.");
      return false;
    }
    if (!report.fatal.isEmpty()) {
      m_result.error = report.fatal;
      return false;
    }
    QVector<SurveyPoint> points;
    for (const SurveyPoint& point : report.points) {
      if (m_job.excludeSuspicious && point.suspicious) continue;
      points.push_back(point);
    }
    m_result = SurveyContourBuilder::build(points, m_job, [this] { return isCanceled(); },
                                           [this](double value) { setProgress(value); });
    return m_result.ok;
  }
  void finished(bool) override {
    if (m_done) m_done(m_result);
  }

private:
  SurveyContourJob m_job;
  SurveyContourResult m_result;
  std::function<void(const SurveyContourResult&)> m_done;
};

}  // namespace

void MainWindow::createSurveyContours() {
  if (m_surveyPath.isEmpty()) {
    KaUserError::warn(this, {
        QStringLiteral("측량 등고선"),
        QStringLiteral("조사를 연 뒤에 등고선을 만들 수 있습니다."),
        QStringLiteral("결과 파일은 조사 폴더 안에만 둡니다."),
        QStringLiteral("조사 열기 또는 새 조사로 시작한 뒤 다시 누르세요."),
    });
    return;
  }
  if (findChild<QProgressDialog*>(QStringLiteral("contourProgress"))) {
    statusBar()->showMessage(QStringLiteral("등고선을 만들고 있습니다. 끝나거나 취소를 누르세요."), 5000);
    return;
  }
  const QString crs = QgsProject::instance()->crs().isValid() ? QgsProject::instance()->crs().authid()
                                                              : QStringLiteral("EPSG:5186");
  KaSurveyContourDialog dialog(crs, this);
  if (dialog.exec() != QDialog::Accepted) return;

  SurveyContourJob job = dialog.job();
  const QString stem = safeStem(QFileInfo(job.read.path).completeBaseName());
  const QString folder = QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("측량등고선/") + stem);
  job.groupTitle = QStringLiteral("측량 등고선 · ") + stem;
  job.outputDir = QDir(folder).filePath(QStringLiteral("_new"));
  job.clipWkt = surveyAreaClipWkt(job.read.crsAuthId);
  auto* progress = new QProgressDialog(QStringLiteral("측량점으로 등고선을 계산하고 있습니다."),
                                       QStringLiteral("취소"), 0, 100, this);
  progress->setObjectName(QStringLiteral("contourProgress"));
  progress->setWindowModality(Qt::NonModal);
  progress->setMinimumDuration(0);
  progress->setAutoClose(false);
  const QPointer<MainWindow> window(this);
  const QPointer<QProgressDialog> dialogProgress(progress);
  const quint64 generation = m_surveyGeneration;
  const QString group = job.groupTitle;
  auto* task = new SurveyContourTask(std::move(job), [=](const SurveyContourResult& result) {
    if (dialogProgress) {
      dialogProgress->hide();
      dialogProgress->deleteLater();
    }
    if (!window || window->m_closingWindow || generation != window->m_surveyGeneration) return;
    if (!result.ok) {
      KaUserError::warn(window, {
          QStringLiteral("측량 등고선"),
          result.canceled ? QStringLiteral("등고선 만들기를 취소했습니다.")
                          : QStringLiteral("등고선을 만들지 못했습니다."),
          result.error,
          QStringLiteral("점 배치와 등고선 간격을 확인한 뒤 다시 시도하세요."),
      });
      return;
    }
    auto* project = QgsProject::instance();
    SurveyContourStyle::removeGroup(project, group);
    QString error;
    if (!SurveyContourBuilder::installFiles(QDir(folder).filePath(QStringLiteral("_new")), folder, &error) ||
        !SurveyContourStyle::apply(project, QDir(folder).filePath(QStringLiteral("contours.gpkg")), group, result,
                                   &error)) {
      KaUserError::warn(window, {
          QStringLiteral("측량 등고선"),
          QStringLiteral("계산은 끝났지만 지도에 올리지 못했습니다."),
          error,
          QStringLiteral("조사 폴더의 측량등고선 파일을 다시 열어 보세요."),
      });
      return;
    }
    QgsMapLayer* lines = nullptr;
    for (QgsMapLayer* layer : project->mapLayers()) {
      if (layer && layer->name() == QStringLiteral("등고선") &&
          layer->customProperty(QStringLiteral("ka_hgis/contour_group")).toString() == group)
        lines = layer;
    }
    if (lines && window->m_canvas) LayerOps::zoomToLayerMax(window->m_canvas, lines);
    if (window->m_canvas) LayerOps::syncMapCanvas(project, window->m_canvas, false);
    project->setDirty(true);
    window->statusBar()->showMessage(
        result.warning.isEmpty()
            ? QStringLiteral("등고선 %1줄, 색 구간 %2개를 올렸습니다.").arg(result.lineCount).arg(result.bandCount)
            : result.warning,
        8000);
  });
  connect(progress, &QProgressDialog::canceled, task, &QgsTask::cancel);
  connect(task, &QgsTask::progressChanged, progress, [dialogProgress](double value) {
    if (dialogProgress) dialogProgress->setValue(qBound(0, qRound(value), 100));
  });
  progress->show();
  QgsApplication::taskManager()->addTask(task);
}
