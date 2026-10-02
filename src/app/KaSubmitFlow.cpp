// 검수·제출 흐름: MainWindow members behind the ribbon chip 「검수·제출」 (Ctrl+E). No package while an
// error remains or the rule file is missing; the check runs before any file dialog. Saving happens
// only when the user picks 「저장하고 만들기」.
#include "MainWindow.h"
#include "KaSubmitDialog.h"
#include "KaUserError.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/SubmitReadme.h"
#include "core/SurveyFacts.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QTabWidget>
#include <algorithm>

#if KA_HGIS_HAS_QGIS
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#endif

bool MainWindow::ensureChecklistRules() {
  if (!m_checklist) return false;
  if (m_checklist->ruleCount() == 0) m_checklist->loadRules(rulesPath());
  return m_checklist->ruleCount() > 0;
}

QVector<CheckResult> MainWindow::evaluateChecklist() {
  if (!ensureChecklistRules()) {
    m_lastChecklistErrors = 1;  // no rules: blocked, never "0 errors"
    return {};
  }
  const QVector<CheckResult> results = m_checklist->evaluate(buildProjectState());
  m_lastChecklistErrors = ChecklistEngine::failedCount(results, QStringLiteral("error"));
  // Home page 「제출 준비」: only an explicit check with rules is recorded (never on save).
  SurveyFacts::noteCheck(m_surveyPath, m_lastChecklistErrors, ChecklistEngine::failedCount(results, QStringLiteral("warn")));
  return results;
}

void MainWindow::openSubmitReview() {
  if (m_submitBusy) return;
  KaSubmitDialog dialog(this);
  const auto refresh = [this, &dialog]() {
    const bool rules = ensureChecklistRules();
    dialog.setResults(evaluateChecklist(), rules, rules ? QString() : QDir::toNativeSeparators(rulesPath()));
    statusBar()->showMessage(QStringLiteral("검수: 오류 %1 · 주의 %2")
                                 .arg(dialog.errorCount()).arg(dialog.warnCount()), 8000);
  };
  refresh();
#if KA_HGIS_HAS_QGIS
  auto* current = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  dialog.setConvertLayerName(current && current->isValid() ? current->name() : QString());
#endif
  connect(&dialog, &KaSubmitDialog::recheckRequested, &dialog, [&refresh]() { refresh(); });
  const int code = dialog.exec();
  if (code != QDialog::Accepted) return;
  switch (dialog.choice()) {
  case KaSubmitDialog::Choice::MakePackage: makeSubmitPackage(); break;
  case KaSubmitDialog::Choice::ConvertLayer: convertSelectedTo5179(); break;
  case KaSubmitDialog::Choice::GoToTargets: goToCheckTargets(dialog.chosenResult()); break;
  case KaSubmitDialog::Choice::RunAction: runCheckAction(dialog.chosenResult().action); break;
  case KaSubmitDialog::Choice::None: break;
  }
}

void MainWindow::makeSubmitPackage() {
  if (m_submitBusy) return;
  // 1. 차단 검사를 파일 대화상자보다 먼저 한다. 규칙이 없으면 오류로 본다.
  if (!ensureChecklistRules()) {
    m_lastChecklistErrors = 1;
    KaUserError::warn(this, {QStringLiteral("검수·제출"),
                             QStringLiteral("검수 규칙 파일을 찾지 못해 제출 꾸러미를 만들지 않았습니다."),
                             QStringLiteral("규칙 없이 만들면 검수 오류가 있어도 통과한 것처럼 됩니다. 찾아본 곳: %1")
                                 .arg(QDir::toNativeSeparators(rulesPath())),
                             QStringLiteral("프로그램 폴더에 data\\rules\\drawing_checklist.v1.json 이 있는지 확인하고, "
                                            "없으면 프로그램을 다시 설치하세요."),
                             {}});
    statusBar()->showMessage(QStringLiteral("제출 차단: 검수 규칙 파일 없음"), 8000);
    return;
  }
  const QVector<CheckResult> results = evaluateChecklist();
  const int errors = ChecklistEngine::failedCount(results, QStringLiteral("error"));
  if (errors > 0) {
    statusBar()->showMessage(QStringLiteral("제출 차단: 검수 오류 %1건").arg(errors), 8000);
    openSubmitReview();  // the list with 「위치 보기」; its package button stays disabled
    return;
  }
#if KA_HGIS_HAS_QGIS
  QgsProject* project = QgsProject::instance();
  if (!LayoutService::isComposedStudioSheet(project, QStringLiteral("user_sheet"))) {
    if (KaUserError::warn(this, {QStringLiteral("검수·제출"),
                                 QStringLiteral("「도면」에서 만든 용지가 없어 제출 꾸러미를 만들지 않았습니다."),
                                 QStringLiteral("꾸러미의 조사도면.pdf는 「도면」에서 만든 용지로 만듭니다."),
                                 QStringLiteral("「도면」에서 용지에 지도를 올린 뒤 다시 만드세요."),
                                 QStringLiteral("도면 열기")}) == KaUserError::Result::ActionChosen)
      openLayoutDesigner();
    return;
  }
  // 2. 저장하지 않은 편집: 묻기만 한다. 자동 저장은 하지 않는다.
  bool unsaved = surveyHasUnsavedChanges();
  if (unsaved) {
    QMessageBox box(QMessageBox::Question, QStringLiteral("검수·제출"),
                    QStringLiteral("저장하지 않은 편집이 있습니다.\n제출 꾸러미는 지금 화면의 도형으로 만들어져 "
                                   "조사 파일(마지막 저장본)과 다를 수 있습니다."),
                    QMessageBox::NoButton, this);
    QPushButton* save = box.addButton(QStringLiteral("저장하고 만들기"), QMessageBox::AcceptRole);
    QPushButton* asIs = box.addButton(QStringLiteral("저장하지 않고 만들기"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.exec();
    if (box.clickedButton() == save) {
      saveProject();
      unsaved = surveyHasUnsavedChanges();
      if (unsaved) {
        notify(Notice::Warning, QStringLiteral("검수·제출"),
               QStringLiteral("저장하지 못해 제출 꾸러미를 만들지 않았습니다. 저장 문제를 해결한 뒤 다시 만드세요."));
        return;
      }
    } else if (box.clickedButton() != asIs) {
      return;
    }
  }
  // 3. 인코딩과 위치.
  bool ok = false;
  const QString enc = QInputDialog::getItem(this, QStringLiteral("SHP 인코딩"),
                                            QStringLiteral("SHP 글자 인코딩 (모르면 UTF-8)"),
                                            {QStringLiteral("UTF-8"), QStringLiteral("EUC-KR")}, 0, false, &ok);
  if (!ok || enc.isEmpty()) return;
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("제출 꾸러미를 만들 위치"),
                                                        preferredSurveyDir());
  if (dir.isEmpty()) return;
  QString surveyName = m_surveyPath.isEmpty() ? QString() : QFileInfo(m_surveyPath).completeBaseName();
  const QString part = SubmitReadme::folderNamePart(surveyName);
  const QString base = QStringLiteral("제출_%1%2").arg(part.isEmpty() ? QString() : part + QLatin1Char('_'),
                                                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
  QString packageName = base;
  for (int n = 2; QFileInfo::exists(QDir(dir).filePath(packageName)) && n < 100; ++n)
    packageName = QStringLiteral("%1_%2").arg(base).arg(n);

  // 4. 진행 창과 취소. 스테이징 폴더에서 만들고 끝나면 한 번에 확정한다.
  QProgressDialog progress(QStringLiteral("제출 꾸러미를 만들고 있습니다…"), QStringLiteral("취소"), 0, 1, this);
  progress.setWindowTitle(QStringLiteral("검수·제출"));
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.show();
  SubmitPackageInfo info;
  info.surveyName = surveyName;
  info.surveyPath = m_surveyPath;
  info.unsavedEditsIncluded = unsaved;
  info.progress = [&progress](int done, int total, const QString& step) {
    progress.setMaximum(std::max(1, total));
    progress.setLabelText(QStringLiteral("제출 꾸러미: %1").arg(step));
    progress.setValue(std::min(done, total));
    return !progress.wasCanceled();
  };
  QScopedValueRollback<bool> busy(m_submitBusy, true);
  m_packageCreated = false;
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      project, QDir(dir).filePath(packageName), enc, ChecklistEngine::summaryText(results),
      /*blockOnError=*/true, errors > 0, &err, info);
  progress.hide();
  if (out.isEmpty()) {
    if (progress.wasCanceled()) {
      statusBar()->showMessage(QStringLiteral("제출 꾸러미 만들기를 취소했습니다."), 6000);
      return;
    }
    KaUserError::warn(this, {QStringLiteral("검수·제출"), QStringLiteral("제출 꾸러미를 만들지 못했습니다."), err,
                             QStringLiteral("저장 위치와 도면 용지를 확인한 뒤 다시 만드세요. 이전에 만든 꾸러미는 그대로입니다."),
                             {}});
    return;
  }
  m_packageCreated = true;
  SurveyFacts::notePackaged(m_surveyPath);  // home page step 3 「도면 · 제출」 counts as done from here
  statusBar()->showMessage(QStringLiteral("제출 꾸러미: %1").arg(QDir::toNativeSeparators(out)), 8000);
  notify(Notice::Success, QStringLiteral("검수·제출"),
         QStringLiteral("제출 꾸러미를 만들었습니다. 인트라넷에는 SHP를 하나씩 올리세요."),
         QDir::toNativeSeparators(out));
#endif
}

void MainWindow::goToCheckTargets(const CheckResult& result) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_viewTabs || !m_mapPage) return;
  QgsProject* project = QgsProject::instance();
  QgsVectorLayer* layer = nullptr;
  QgsFeatureIds ids;
  bool wholeLayer = false;
  for (const CheckTarget& t : result.targets) {
    auto* vl = qobject_cast<QgsVectorLayer*>(project->mapLayer(t.layerId));
    if (!vl || (layer && vl != layer)) continue;
    layer = vl;
    if (t.featureId >= 0) ids.insert(t.featureId);
    else wholeLayer = true;
  }
  if (!layer) {
    statusBar()->showMessage(QStringLiteral("해당 도형을 지도에서 찾지 못했습니다. 「다시 검수」로 확인하세요."), 6000);
    return;
  }
  m_viewTabs->setCurrentWidget(m_mapPage);
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  QgsRectangle box;
  if (!ids.isEmpty()) {
    layer->selectByIds(ids);
    QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setFilterFids(ids).setNoAttributes());
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
      if (box.isNull()) box = f.geometry().boundingBox();
      else box.combineExtentWith(f.geometry().boundingBox());
    }
  } else if (wholeLayer) {
    box = layer->extent();
  }
  if (box.isNull() || !box.isFinite()) {
    statusBar()->showMessage(QStringLiteral("「%1」에서 %2개를 골라 두었습니다. 도형이 비어 지도에 보이지 않으니 "
                                            "속성표에서 지우세요.").arg(layer->name()).arg(ids.size()), 12000);
    return;
  }
  QgsRectangle ext = m_canvas->mapSettings().layerExtentToOutputExtent(layer, box);
  ext.grow(std::max({ext.width(), ext.height(), 20.0}) * 0.3);
  m_canvas->setExtent(ext);
  LayerOps::refreshCanvasIfIdle(m_canvas);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  if (!ids.isEmpty()) m_canvas->flashFeatureIds(layer, ids);
  statusBar()->showMessage(ids.isEmpty()
                               ? QStringLiteral("「%1」 레이어로 이동했습니다: %2").arg(layer->name(), result.messageKo)
                               : QStringLiteral("「%1」의 해당 도형 %2개로 이동해 골라 두었습니다: %3")
                                     .arg(layer->name()).arg(ids.size()).arg(result.messageKo),
                           12000);
#else
  Q_UNUSED(result);
#endif
}

void MainWindow::runCheckAction(const QString& action) {
  if (action == QLatin1String("open_layout")) return openLayoutDesigner();
  if (action == QLatin1String("open_section")) return openSectionDesigner();
  // Map tools work on the map tab; the user picked the tool, so show that tab.
  if (action != QLatin1String("set_work_crs") && m_viewTabs && m_mapPage) m_viewTabs->setCurrentWidget(m_mapPage);
  if (action == QLatin1String("draw_survey_area")) startEditSurveyArea();
  else if (action == QLatin1String("draw_feature")) startEditFeaturePoly();
  else if (action == QLatin1String("add_control_point")) addControlPoint();
  else if (action == QLatin1String("edit_attributes")) startAttributeEditTool();
  else if (action == QLatin1String("select_tool")) startSelectTool();
  else if (action == QLatin1String("vertex_edit")) {  // same entry as the layer menu 「꼭짓점 수정」
    startSelectTool();
    statusBar()->showMessage(QStringLiteral("고칠 도형을 클릭한 뒤 꼭짓점을 끌어 옮기세요."), 8000);
  } else if (action == QLatin1String("set_work_crs")) {
    bool ok = false;
    const QString pick = QInputDialog::getItem(
        this, QStringLiteral("작업 좌표계"), QStringLiteral("현장 위치에 맞는 작업 좌표계를 고르세요."),
        {QStringLiteral("EPSG:5186 중부원점"), QStringLiteral("EPSG:5187 동부원점")},
        m_workCrs == QLatin1String("EPSG:5186") ? 0 : 1, false, &ok);
    if (!ok) return;
    if (pick.startsWith(QLatin1String("EPSG:5186"))) setWorkCrs5186();
    else setWorkCrs5187();
  }
}

QString MainWindow::missingAttributeCounterText() const {
#if KA_HGIS_HAS_QGIS
  int missing = 0;
  for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(QgsProject::instance(), QStringLiteral("feature_poly"))) {
    QgsFeatureRequest request;
    request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
    QgsFeatureIterator it = layer->getFeatures(request);
    QgsFeature f;
    while (it.nextFeature(f)) {
      const int kind = f.fields().lookupField(QStringLiteral("kind"));
      const int period = f.fields().lookupField(QStringLiteral("period"));
      if (kind < 0 || period < 0 || f.attribute(kind).toString().trimmed().isEmpty() ||
          f.attribute(period).toString().trimmed().isEmpty())
        ++missing;
    }
  }
  return missing > 0 ? QStringLiteral(" · 종류·시대 빈 유구 %1개(제출 전 「검수·제출」에서 확인)").arg(missing)
                     : QString();
#else
  return {};
#endif
}
