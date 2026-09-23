#include "MainWindow.h"
#include "KaDownloadUi.h"
#include "KaReferenceDownloadJob.h"
#include "core/CadastralImport.h"
#include "core/CadastralPortal.h"
#include "core/LayerOps.h"
#include "core/VworldSettings.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

bool MainWindow::configureCadastralAccount() {
  const auto previous = CadastralPortal::credentials();
  QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("VWorld 지적도 계정"));
  auto* form = new QFormLayout(&dialog);
  auto* id = new QLineEdit(previous.username, &dialog);
  auto* password = new QLineEdit(previous.password, &dialog); password->setEchoMode(QLineEdit::Password);
  form->addRow(QStringLiteral("아이디"), id); form->addRow(QStringLiteral("비밀번호"), password);
  form->addRow(new QLabel(QStringLiteral("이 PC의 개인 설정에 저장하여 다음 다운로드에 사용합니다."), &dialog));
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted) return false;
  QString error;
  if (!CadastralPortal::saveCredentials({id->text(), password->text()}, &error)) {
    QMessageBox::warning(this, dialog.windowTitle(), error);
    return false;
  }
  return true;
}

void MainWindow::configureCadastralStyle() {
  QList<QgsVectorLayer*> layers;
  for (auto* layer : QgsProject::instance()->mapLayers())
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer); vector && vector->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool()) layers.append(vector);
  if (layers.isEmpty()) { QMessageBox::information(this, QStringLiteral("지적도"), QStringLiteral("지적도를 먼저 받아 주세요.")); return; }
  QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("지적도 선 색·지번"));
  auto* form = new QFormLayout(&dialog);
  QColor color(layers.first()->customProperty(QStringLiteral("ka_hgis/cadastral_color"), QStringLiteral("#000000")).toString());
  auto* choose = new QPushButton(color.name(), &dialog);
  connect(choose, &QPushButton::clicked, &dialog, [&] {
    const auto selected = QColorDialog::getColor(color, &dialog, QStringLiteral("경계선 색"));
    if (selected.isValid()) { color = selected; choose->setText(color.name()); }
  });
  form->addRow(QStringLiteral("경계선 0.2mm"), choose);
  auto* labels = new QCheckBox(QStringLiteral("지번 표시"), &dialog); labels->setChecked(layers.first()->labelsEnabled());
  form->addRow(labels);
  auto* hint = new QLabel(QStringLiteral("지번은 1:10,000보다 확대했을 때 표시됩니다."), &dialog); form->addRow(hint);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted) return;
  for (auto* layer : layers) CadastralImport::applyStyle(layer, color, labels->isChecked());
  QgsProject::instance()->setDirty(true);
  LayerOps::refreshCanvasIfIdle(m_canvas);
}

void MainWindow::downloadCadastral() {
  if (m_isOpeningSurvey || m_closingWindow) return;
  if (m_referenceDownload) { statusBar()->showMessage(QStringLiteral("지도 자료를 준비 중입니다. 완료를 기다리거나 취소하세요."), 5000); return; }
  if (m_surveyPath.isEmpty() || !QFileInfo::exists(m_surveyPath)) {
    QMessageBox::information(this, QStringLiteral("지적도"), QStringLiteral("먼저 조사를 열거나 저장하고 조사구역을 그려 주세요.")); return;
  }
  auto* project = QgsProject::instance();
  CadastralPortal::Request request;
  request.surveyCrs = project->crs(); request.workCrs = project->crs(); request.context = project->transformContext();
  QVector<QgsGeometry> parts;
  try {
    for (auto* layer : project->mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (!vector || LayerOps::layerKeyOf(vector) != QLatin1String("survey_area")) continue;
      QgsFeatureRequest filter; filter.setNoAttributes();
      auto features = vector->getFeatures(filter); QgsFeature feature;
      const QgsCoordinateTransform transform(vector->crs(), request.workCrs, request.context);
      while (features.nextFeature(feature)) {
        QgsGeometry geometry = feature.geometry();
        if (geometry.isEmpty()) continue;
        geometry.transform(transform); parts.append(geometry);
      }
    }
  } catch (const QgsCsException&) {
    QMessageBox::warning(this, QStringLiteral("지적도"), QStringLiteral("조사구역 좌표계를 확인하지 못했습니다.")); return;
  }
  if (parts.isEmpty()) { QMessageBox::information(this, QStringLiteral("지적도"), QStringLiteral("조사구역을 먼저 그려 주세요. 경계에서 주변 5km의 지적도를 받습니다.")); return; }
  request.survey = QgsGeometry::unaryUnion(parts);
  request.apiKey = VworldSettings::loadApiKey(); request.credentials = CadastralPortal::credentials();
  if (request.credentials.username.isEmpty() || request.credentials.password.isEmpty()) {
    configureCadastralAccount(); request.credentials = CadastralPortal::credentials();
    if (request.credentials.username.isEmpty() || request.credentials.password.isEmpty()) return;
  }
  request.directory = QFileInfo(m_surveyPath).absoluteDir().filePath(QStringLiteral("지적도"));
  auto* dialog = new KaDownloadProgressDialog(QStringLiteral("지적도 다운로드"),
      QStringLiteral("VWorld · 조사구역 경계에서 주변 5km의 지적도를 준비합니다."), this);
  dialog->setRange(0, 100); dialog->setValue(0);
  const QPointer<KaDownloadProgressDialog> progress(dialog); const QPointer<MainWindow> window(this);
  const quint64 generation = m_surveyGeneration;
  auto prepare = [request, progress](QgsFeedback*, const std::function<bool()>& canceled) {
    return CadastralPortal::prepare(request, canceled, [progress](int value, const QString& phase) {
      if (progress) QMetaObject::invokeMethod(progress, [progress, value, phase] {
        if (progress) { progress->setLabelText(phase); progress->setValue(value); }
      }, Qt::QueuedConnection);
    });
  };
  auto complete = [window, progress, generation](const PreparedReferenceMap& result) {
    if (progress) { progress->hide(); progress->deleteLater(); }
    if (!window) return;
    window->m_referenceDownload = nullptr;
    if (window->m_closingWindow || generation != window->m_surveyGeneration) return;
    if (result.status == PreparedReferenceMap::Status::Cancelled) { window->statusBar()->showMessage(QStringLiteral("지적도 받기를 취소했습니다."), 5000); return; }
    QString error = result.error;
    auto* layer = result.isReady() ? CadastralImport::addPrepared(QgsProject::instance(), window->m_canvas, result, &error) : nullptr;
    if (!layer && result.accountRejected) {
      // The account entry lives in a gallery sub-menu, so offer it right where the login failed.
      QMessageBox box(QMessageBox::Warning, QStringLiteral("지적도 받기"), error, QMessageBox::NoButton, window);
      auto* reenter = box.addButton(QStringLiteral("아이디·비밀번호 다시 입력"), QMessageBox::AcceptRole);
      box.addButton(QStringLiteral("닫기"), QMessageBox::RejectRole);
      box.exec();
      if (box.clickedButton() == reenter && window && window->configureCadastralAccount()) window->downloadCadastral();
      return;
    }
    if (!layer) { QMessageBox::warning(window, QStringLiteral("지적도 받기"), error.isEmpty() ? QStringLiteral("지적도를 준비하지 못했습니다.") : error); return; }
    if (window->m_layerTree) window->m_layerTree->setCurrentLayer(layer);
    QgsProject::instance()->setDirty(true);
    window->statusBar()->showMessage(QStringLiteral("조사 주변 5km 지적도를 추가했습니다. 지적도 옆 메뉴에서 선 색·지번 표시를 바꿀 수 있습니다."), 10000);
  };
  auto* job = new KaReferenceDownloadJob(QStringLiteral("조사 주변 지적도"), std::move(prepare), std::move(complete));
  m_referenceDownload = job;
  connect(dialog, &KaDownloadProgressDialog::canceled, job, &QgsTask::cancel);
  dialog->show(); QgsApplication::taskManager()->addTask(job);
}
