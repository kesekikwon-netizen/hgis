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

QgsGeometry surveyAreaGeometry(const QString& crsAuthId) {
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
  return area;
}

class SurveyContourTask final : public QgsTask {
public:
  SurveyContourTask(SurveyContourJob job, std::function<void(const SurveyContourResult&)> done)
      : QgsTask(QStringLiteral("측량 등고선"), QgsTask::CanCancel),
        m_job(std::move(job)), m_done(std::move(done)) {}

protected:
  bool run() override {
    // The dialog already read and arranged the points; read again only when it did not.
    QVector<SurveyPoint> source = m_job.points;
    if (source.isEmpty()) {
      const SurveyReadReport report = SurveyPointReader::read(m_job.read);
      if (!report.fatal.isEmpty()) {
        m_result.error = report.fatal;
        return false;
      }
      source = report.points;
    }
    if (isCanceled()) {
      m_result.canceled = true;
      m_result.error = QStringLiteral("등고선 만들기를 취소했습니다.");
      return false;
    }
    QVector<SurveyPoint> points;
    for (const SurveyPoint& point : source) {
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
  // The dialog places points in this CRS, so the survey area is taken in the same CRS.
  const QgsGeometry area = surveyAreaGeometry(crs);
  KaSurveyContourDialog dialog(crs, this);
  if (!area.isEmpty()) dialog.setReferenceExtent(area.boundingBox());
  if (dialog.exec() != QDialog::Accepted) return;

  SurveyContourJob job = dialog.job();
  const QString stem = safeStem(QFileInfo(job.read.path).completeBaseName());
  const QString folder = QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("측량등고선/") + stem);
  job.groupTitle = QStringLiteral("측량 등고선 · ") + stem;
  job.outputDir = QDir(folder).filePath(QStringLiteral("_new"));
  job.clipWkt = area.isEmpty() ? QString() : area.asWkt();
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
    const QString gpkg = QDir(folder).filePath(QStringLiteral("contours.gpkg"));
    // On Windows the loaded layers hold the old files open; they must go before the swap.
    SurveyContourStyle::removeGroup(project, group);
    QString error;
    const bool installed =
        SurveyContourBuilder::installFiles(QDir(folder).filePath(QStringLiteral("_new")), folder, &error);
    if (!installed || !SurveyContourStyle::apply(project, gpkg, group, result, &error)) {
      // A failed swap keeps the previous files; show them again rather than leave a gap.
      const bool restored = !installed && SurveyContourStyle::reapply(project, gpkg, group);
      if (restored && window->m_canvas) LayerOps::syncMapCanvas(project, window->m_canvas, false);
      KaUserError::warn(window, {
          QStringLiteral("측량 등고선"),
          QStringLiteral("계산은 끝났지만 지도에 올리지 못했습니다."),
          error,
          restored ? QStringLiteral("이전 등고선을 다시 올려 두었습니다. 측량등고선 파일을 연 프로그램을 닫고 다시 만드세요.")
                   : QStringLiteral("조사 폴더의 측량등고선 파일을 다시 열어 보세요."),
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
    QString message = QStringLiteral("등고선 %1줄, 색 구간 %2개를 올렸습니다.").arg(result.lineCount).arg(result.bandCount);
    if (!result.warning.isEmpty()) message += QLatin1Char(' ') + result.warning;
    window->statusBar()->showMessage(message, 10000);
  });
  connect(progress, &QProgressDialog::canceled, task, &QgsTask::cancel);
  connect(task, &QgsTask::progressChanged, progress, [dialogProgress](double value) {
    if (dialogProgress) dialogProgress->setValue(qBound(0, qRound(value), 100));
  });
  progress->show();
  QgsApplication::taskManager()->addTask(task);
}
