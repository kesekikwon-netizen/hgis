#include "KaHeritageBrowser.h"
#include "KaDownloadUi.h"
#include "KaWindowGeometry.h"
#include <QShowEvent>

#include "core/HeritageFormParser.h"
#include "core/HeritageIntranetFlow.h"

#include "core/HeritageIntranetSettings.h"

#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QLabel>
#include <QMutex>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QUuid>
#include <QVector>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFrame>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEnginePage>
#include <QWebEngineLoadingInfo>
#include <QWebEngineView>

#include <memory>
#include <optional>

// 사이트가 실제로 보내는 요청을 그대로 적는다.
//
// 프레임 안 DOM 을 찾아 누르는 방식은 화면 구조가 조금만 달라도 통째로 멈춘다.
// 그래서 「무엇을 찾을까」가 아니라 「무엇을 보냈나」를 기록한다.
// 여기서 나온 주소·파라미터를 알면 브라우저 없이 HTTP 로 바로 받을 수 있다.
//
// 값은 남기되 **보낸 본문은 적지 않는다** — 로그인 요청에 비밀번호가 들어가기 때문이다.
class HeritageRequestLog : public QWebEngineUrlRequestInterceptor {
public:
  explicit HeritageRequestLog(QObject* parent = nullptr)
      : QWebEngineUrlRequestInterceptor(parent) {}

  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    // 무엇을 남기고 무엇을 지울지는 한 곳에서 정한다(검사가 그 규칙을 붙잡는다).
    const auto type = info.resourceType();
    if (type == QWebEngineUrlRequestInfo::ResourceTypeImage ||
        type == QWebEngineUrlRequestInfo::ResourceTypeFontResource ||
        type == QWebEngineUrlRequestInfo::ResourceTypeStylesheet ||
        type == QWebEngineUrlRequestInfo::ResourceTypeScript)
      return;
    const QString line = HeritageIntranetFlow::redactRequestLine(
        QString::fromLatin1(info.requestMethod()),
        info.requestUrl().adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo));

    QMutexLocker lock(&m_mutex);
    if (info.requestUrl().host() == HeritageIntranetFlow::portalUrl().host() &&
        info.requestUrl().path() == HeritageIntranetFlow::downloadFilesAllPath())
      ++m_downloadRequestCount;
    if (!m_lines.isEmpty() && m_lines.last() == line) return;  // 같은 요청 반복은 한 줄로
    m_lines.append(line);
    while (m_lines.size() > 60) m_lines.removeFirst();
  }

  QStringList lines() const {
    QMutexLocker lock(&m_mutex);
    return m_lines;
  }

  quint64 downloadRequestCount() const {
    QMutexLocker lock(&m_mutex);
    return m_downloadRequestCount;
  }

  void clear() {
    QMutexLocker lock(&m_mutex);
    m_lines.clear();
  }

private:
  mutable QMutex m_mutex;  // 가로채기는 다른 스레드에서 불린다
  QStringList m_lines;
  quint64 m_downloadRequestCount = 0;
};

namespace {
constexpr int kPollMs = 700;

// 이 사이트는 window.open 으로 창을 연다(공지·서약서·다운로드).
// createWindow 를 구현하지 않으면 그 창이 통째로 버려진다 — 수치지형도 창과 같은 구조다.
class HeritagePage : public QWebEnginePage {
public:
  HeritagePage(QWebEngineProfile* profile, QObject* parent,
               std::function<QWebEngineView*(QWebEnginePage*)> spawn,
               std::function<void(const QString&)> note)
      : QWebEnginePage(profile, parent), m_spawn(std::move(spawn)), m_note(std::move(note)) {}

  QWebEnginePage* createWindow(WebWindowType) override {
    QWebEngineView* view = m_spawn ? m_spawn(this) : nullptr;
    return view ? view->page() : nullptr;
  }

  // 내려받기를 누르면 좌표체계 안내(EPSG:5179) 알림이 뜬다(2026-09-11 확인).
  // 아무도 「확인」을 누르지 않으면 거기서 멈춘다. 자동으로 닫되 무슨 말이었는지는 남긴다.
  void javaScriptAlert(const QUrl&, const QString& message) override {
    if (m_note) m_note(message);
  }
  bool javaScriptConfirm(const QUrl&, const QString& message) override {
    if (m_note) m_note(message);
    return true;  // 받기를 계속한다
  }
  bool javaScriptPrompt(const QUrl&, const QString& message, const QString&, QString*) override {
    // 값을 지어내지 않는다. 물어보는 것이 있으면 그렇다고 알리고 취소한다.
    if (m_note) m_note(message);
    return false;
  }

private:
  std::function<QWebEngineView*(QWebEnginePage*)> m_spawn;
  std::function<void(const QString&)> m_note;
};

constexpr int kMaxWaitTicks = 75;  // 수치지형도 창과 같은 여유(약 52초)

void collectFrames(const QWebEngineFrame& frame, QVector<QWebEngineFrame>* out) {
  if (!frame.isValid()) return;
  out->push_back(frame);
  const QList<QWebEngineFrame> kids = frame.children();
  for (const QWebEngineFrame& child : kids)
    collectFrames(child, out);
}

QVector<QWebEngineFrame> allFrames(QTabWidget* tabs) {
  QVector<QWebEngineFrame> frames;
  if (!tabs) return frames;
  for (int i = 0; i < tabs->count(); ++i) {
    auto* view = qobject_cast<QWebEngineView*>(tabs->widget(i));
    if (!view || !view->page()) continue;
    collectFrames(view->page()->mainFrame(), &frames);
  }
  return frames;
}

QVector<QWebEnginePage*> allPages(QTabWidget* tabs) {
  QVector<QWebEnginePage*> pages;
  if (!tabs) return pages;
  for (int i = 0; i < tabs->count(); ++i) {
    auto* view = qobject_cast<QWebEngineView*>(tabs->widget(i));
    if (view && view->page()) pages.push_back(view->page());
  }
  return pages;
}

void appendUnique(QVector<QWebEngineFrame>* frames, const QWebEngineFrame& frame) {
  if (!frames || !frame.isValid()) return;
  if (frames->contains(frame)) return;
  frames->push_back(frame);
}

void mergeNamedFrames(QWebEnginePage* page, const QJsonArray& list, QVector<QWebEngineFrame>* frames) {
  if (!page || !frames) return;
  for (const QJsonValue& value : list) {
    const QJsonObject row = value.toObject();
    const QStringList keys = {row.value(QStringLiteral("name")).toString(),
                              row.value(QStringLiteral("id")).toString()};
    for (const QString& key : keys) {
      if (key.trimmed().isEmpty()) continue;
      const std::optional<QWebEngineFrame> found = page->findFrameByName(key);
      if (found && found->isValid()) appendUnique(frames, *found);
    }
  }
}

bool isHeritageHost(const QUrl& url) {
  return url.host().endsWith(QStringLiteral("gis-heritage.go.kr"));
}

QString safeName(const QString& suggested) {
  QString name = QFileInfo(suggested).fileName().trimmed();
  name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
  return name.isEmpty() ? QStringLiteral("download.bin") : name;
}
}  // namespace

KaHeritageBrowser::KaHeritageBrowser(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("주변유적 받기"));
  setModal(false);
  KaWindowGeometry::placeDownload(this);

  auto* root = new QVBoxLayout(this);
  KaDownloadUi::configure(this, root, QStringLiteral("주변유적 다운로드"),
      QStringLiteral("국가유산 공간정보 인트라넷 · 선택한 시·군의 자료를 순서대로 받습니다."));
  auto* statusCard = KaDownloadUi::statusCard(this);
  auto* statusLayout = qobject_cast<QVBoxLayout*>(statusCard->layout());
  m_stageLabel = new QLabel(QStringLiteral("대기"), this);
  m_stageLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
  m_detailLabel = new QLabel(QString(), this);
  m_detailLabel->setWordWrap(true);
  m_stageLabel->setWordWrap(true);
  statusLayout->addWidget(m_stageLabel);
  statusLayout->addWidget(m_detailLabel);
  m_progress = new QProgressBar(this);
  m_progress->setRange(0, 0);
  KaDownloadUi::styleProgress(m_progress);
  statusLayout->addWidget(m_progress);
  root->addWidget(statusCard);

  m_http = new HeritageHttpClient(this);
  connect(m_http, &HeritageHttpClient::failed, this, [this](const QString& why) {
    fail(QStringLiteral("HTTP 경로: %1").arg(why));
  });
  connect(m_http, &HeritageHttpClient::pageReady, this,
          [this](const HeritageForm& form, int total) {
            m_totalBeforeSearch = total;
            m_detailLabel->setText(
                QStringLiteral("검색 폼을 읽었습니다(시·도 %1, 시·군·구 %2, 조건 전 %3건). %4 %5 검색합니다.")
                    .arg(form.sidoField, form.sigunguField)
                    .arg(total)
                    .arg(m_sido, m_city));
            m_http->search(m_sido, m_city);
          });
  connect(m_http, &HeritageHttpClient::searchReady, this,
          [this](int count, const QByteArray&) {
            setStage(HeritageStage::Download,
                     QStringLiteral("%1 %2 — 검색결과 %3건. 내려받습니다.")
                         .arg(m_sido, m_city)
                         .arg(count));
          });
  m_tabs = new QTabWidget(this);
  m_tabs->setDocumentMode(true);
  // 공식 사이트 화면은 숨긴다(수치지형도 창과 같은 원칙). 단계·진행만 보여 준다.
  // 막혔을 때만 「자세히」로 펼쳐 본다.
  m_tabs->setVisible(false);
  root->addWidget(m_tabs, 1);

  // 아직 확인하지 못한 단계에서 멈췄을 때, 그 화면에 무엇이 있었는지 보여 준다.
  m_outline = new QPlainTextEdit(this);
  m_outline->setReadOnly(true);
  m_outline->hide();
  m_outline->setMaximumHeight(140);
  m_outline->setPlaceholderText(
      QStringLiteral("진행 중 문제가 발생한 경우 확인할 상세 내용입니다."));
  root->addWidget(m_outline);

  auto* buttons = new QHBoxLayout;
  auto* detailsButton = new QPushButton(QStringLiteral("상세 보기"), this);
  detailsButton->setObjectName(QStringLiteral("heritageDetails"));
  detailsButton->setCheckable(true);
  detailsButton->setToolTip(QStringLiteral("공식 사이트 화면을 펼쳐 봅니다. 막혔을 때만 쓰면 됩니다."));
  connect(detailsButton, &QPushButton::toggled, this, [this](bool on) {
    if (on) ensureProfile();
    if (m_tabs) m_tabs->setVisible(on);
    if (m_outline) m_outline->setVisible(on);
    if (on) {
      resize(980, 720);
      KaWindowGeometry::fit(this);
    } else {
      KaWindowGeometry::placeDownload(this);
    }
  });
  buttons->addWidget(detailsButton);
  buttons->addStretch(1);
  m_stopButton = new QPushButton(QStringLiteral("취소"), this);
  connect(m_stopButton, &QPushButton::clicked, this, &KaHeritageBrowser::stop);
  buttons->addWidget(m_stopButton);
  root->addLayout(buttons);

  m_poll = new QTimer(this);
  m_poll->setInterval(kPollMs);
  connect(m_poll, &QTimer::timeout, this, &KaHeritageBrowser::runStage);
}

void KaHeritageBrowser::ensureProfile() {
  if (m_profile) return;
  m_profile = new QWebEngineProfile(QStringLiteral("ka-heritage"), this);
  m_requestLog = new HeritageRequestLog(this);
  m_profile->setUrlRequestInterceptor(m_requestLog);
  connect(m_profile->cookieStore(), &QWebEngineCookieStore::cookieAdded, this,
          [this](const QNetworkCookie& cookie) {
            m_cookies.removeIf([&cookie](const QNetworkCookie& c) { return c.name() == cookie.name(); });
            m_cookies.append(cookie);
          });
  connect(m_profile, &QWebEngineProfile::downloadRequested, this,
          &KaHeritageBrowser::handleDownload);
}

KaHeritageBrowser::~KaHeritageBrowser() { stop(); m_poll->stop(); }

void KaHeritageBrowser::showEvent(QShowEvent* event) {
  QDialog::showEvent(event);
  if (auto* details = findChild<QPushButton*>(QStringLiteral("heritageDetails"));
      !details || !details->isChecked())
    KaWindowGeometry::placeDownload(this);
  else
    KaWindowGeometry::fit(this);
}

void KaHeritageBrowser::setDownloadRoot(const QString& directory) {
  m_downloadRoot = directory;
}

void KaHeritageBrowser::setTarget(const QString& sido, const QString& city,
                                  const QVector<HeritageDataset>& datasets) {
  m_sido = sido.trimmed();
  m_city = city.trimmed();
  m_datasets = datasets;
  m_datasetIndex = 0;
}

void KaHeritageBrowser::logLine(const QString& text) {
  // 조사폴더가 없을 수도 있으니 리포의 고정 자리에도 함께 남긴다.
  QStringList targets;
  if (!m_downloadRoot.isEmpty()) {
    const QString dir =
        QDir(QDir(m_downloadRoot).absolutePath() + QStringLiteral("/../receipts")).absolutePath();
    if (QDir().mkpath(dir)) targets << QDir(dir).filePath(QStringLiteral("진단.txt"));
  }
  if (QDir().mkpath(QStringLiteral("build/qa")))
    targets << QStringLiteral("build/qa/heritage-debug.txt");

  const QString line = QStringLiteral("[%1] %2")
                           .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")))
                           .arg(text);
  for (const QString& path : targets) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) continue;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << line << Qt::endl;
  }
}

void KaHeritageBrowser::setStage(HeritageStage stage, const QString& message) {
  m_stage = stage;
  m_waitTicks = 0;
  if (stage == HeritageStage::SelectDataset) m_datasetSelectionStarted = false;
  if (stage == HeritageStage::OpenDownloadPage) m_downloadPageRequested = false;
  if (stage == HeritageStage::AgreeTerms) m_agreementSubmitted = false;
  if (stage != HeritageStage::Download) m_downloadNavigation = false;
  // 화면이 자리잡을 시간을 준다. ajax 로 폼을 다시 그리는 사이트라
  // 바로 다음 동작을 하면 빈 값을 잡는다(2026-09-12: 1초 만에 골라 전국을 요청했다).
  // 지금은 각 단계 스크립트가 고른 값을 되읽어 확인하므로(시군은 ok:<시도>|<시군>)
  // 빈 폼이면 그대로 기다린다. 그래서 첫 시도 대기는 줄이고 확인에 맡긴다.
  m_settle = 2;
  // 자료 종류를 바꾸면 폼 전체가 새로 그려지고 시/군 목록도 다시 채워진다. 더 기다린다.
  if (stage == HeritageStage::SelectRegion) m_settle = 4;
  logLine(QStringLiteral("단계 → %1 : %2")
              .arg(HeritageIntranetFlow::stageName(stage), message));
  m_stageLabel->setText(stage == HeritageStage::Done ? HeritageIntranetFlow::stageName(stage)
      : QStringLiteral("[%1/%2] %3 — %4")
            .arg(qMin(m_datasetIndex + 1, static_cast<int>(m_datasets.size()))).arg(m_datasets.size())
            .arg(m_datasetIndex < m_datasets.size() ? HeritageStyle::layerName(m_datasets.at(m_datasetIndex)) : QString(),
                 HeritageIntranetFlow::stageName(stage)));
  if (stage == HeritageStage::Done) {
    m_progress->setRange(0, 100);
    m_progress->setValue(100);
    m_progress->setFormat(QStringLiteral("완료"));
  } else if (stage == HeritageStage::Idle) {
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFormat(QStringLiteral("중지"));
  } else {
    m_progress->setRange(0, 0);
  }
  m_detailLabel->setText(message);
  emit stageChanged(stage, message);
}

QString KaHeritageBrowser::saveRequestLog() {
  if (!m_requestLog || m_downloadRoot.isEmpty()) return {};
  const QStringList sent = m_requestLog->lines();
  if (sent.isEmpty()) return {};
  // 조사폴더/주변유적/receipts/ 에 남긴다. 배포 경로가 아니다.
  const QDir root(m_downloadRoot);
  const QString dir = QDir(root.absolutePath() + QStringLiteral("/../receipts")).absolutePath();
  if (!QDir().mkpath(dir)) return {};
  const QString path =
      QDir(dir).filePath(QStringLiteral("requests-%1.txt")
                             .arg(QDateTime::currentDateTime().toString(
                                 QStringLiteral("yyyyMMdd-HHmmss"))));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QStringLiteral("국가유산 인트라넷이 보낸 요청 기록") << Qt::endl;
  out << QStringLiteral("기록 시각: ")
      << QDateTime::currentDateTime().toString(Qt::ISODate) << Qt::endl;
  out << QStringLiteral("로그인 요청의 내용은 남기지 않는다.") << Qt::endl;
  out << QStringLiteral("----") << Qt::endl;
  for (const QString& line : sent) out << line << Qt::endl;
  return path;
}

void KaHeritageBrowser::fail(const QString& message) {
  logLine(QStringLiteral("실패: %1").arg(message));
  m_running = false;
  ++m_generation;
  m_scriptInFlight = false;
  for (const auto& request : std::as_const(m_downloadRequests))
    if (request && !request->isFinished()) request->cancel();
  m_progress->setRange(0, 100);
  m_progress->setValue(0);
  m_progress->setFormat(QStringLiteral("실패"));
  m_poll->stop();
  m_stage = HeritageStage::Failed;
  m_stageLabel->setText(HeritageIntranetFlow::stageName(HeritageStage::Failed));
  m_detailLabel->setText(message);
  // 요청 기록을 파일로 남긴다. 화면에서 긁어 오지 않아도 되게.
  const QString logPath = saveRequestLog();
  if (!logPath.isEmpty())
    m_detailLabel->setText(QStringLiteral("%1\n요청 기록: %2").arg(message, logPath));
  // 멈춘 자리를 그대로 보여 준다. 무엇 때문에 못 갔는지 사람이 볼 수 있어야 한다.
  captureOutline([this, message]() {
    emit failed(message);
  });
}

void KaHeritageBrowser::stop() {
  if (!m_running) return;
  m_running = false;
  for (const auto& request : std::as_const(m_downloadRequests))
    if (request && !request->isFinished()) request->cancel();
  ++m_generation;
  m_scriptInFlight = false;
  m_poll->stop();
  setStage(HeritageStage::Idle, QStringLiteral("사용자가 중지했습니다."));
}

void KaHeritageBrowser::rejectDataset(const QString& message, bool retryableDownload) {
  if (!m_running) return;
  if (retryableDownload && m_stage == HeritageStage::Download)
    scheduleDownloadRetry(message);
  else
    fail(message);
}

void KaHeritageBrowser::scheduleDownloadRetry(const QString& reason, bool responseOnly) {
  if (!m_running || m_stage != HeritageStage::Download || !m_downloadRetryReason.isEmpty()) return;
  m_downloadRetryReason = reason;
  m_responseOnlyRetry = responseOnly;
  ++m_downloadRetryCount;
  constexpr int delays[] = {5000, 10000, 20000, 30000};
  m_downloadRetryDelayMs = delays[qMin<quint64>(m_downloadRetryCount - 1, 3)];
  m_downloadRetryWait.invalidate();
  m_progress->setRange(0, 0);
  m_detailLabel->setText(QStringLiteral("%1 · 정상 파일을 다시 받습니다 · 중지 가능").arg(reason));
  logLine(QStringLiteral("재수신 예약 %1회: %2").arg(m_downloadRetryCount).arg(reason));
}

void KaHeritageBrowser::closeDownloadPopups(bool includeFormPages) {
  for (int i = m_tabs->count() - 1; i >= 0; --i) {
    auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
    if (!view || view == m_mainView) continue;
    if (!includeFormPages && !view->property("heritageDownloadPopup").toBool()) continue;
    m_tabs->removeTab(i);
    view->deleteLater();
  }
}

void KaHeritageBrowser::reopenDownloadSession() {
  // 완료된 다른 자료와 현재 자료 번호는 유지한다. 실패한 문서/콜백은 재사용하지 않는다.
  ++m_generation;
  m_scriptInFlight = false;
  m_downloadRequests.clear();
  m_currentFiles.clear();
  m_downloadMode.clear();
  m_downloadRetryReason.clear();
  m_responseOnlyRetry = false;
  m_downloadPageNeedsRecovery = false;
  m_downloadRetryWait.invalidate();
  m_downloadNavigation = false;
  m_downloadRequestObserved = false;
  m_idleTicks = 0;
  m_totalBeforeSearch = -1;
  m_agreementSubmitted = false;
  m_agreementRecorded = false;
  m_openedFrameSrc = false;
  m_frameChecked = false;
  m_lastCppProbe.clear();
  m_mainView = nullptr;
  while (m_tabs->count() > 0) {
    QWidget* old = m_tabs->widget(0);
    m_tabs->removeTab(0);
    old->deleteLater();
  }
  m_pageReady = false;
  setStage(HeritageStage::Login,
           QStringLiteral("공식 화면을 다시 열어 같은 지역·자료를 다시 받습니다."));
  logLine(QStringLiteral("재수신: 공식 화면 재시작 · 현재 자료 %1 유지").arg(m_datasetIndex + 1));
  addPage()->load(HeritageIntranetFlow::loginUrl());
}

void KaHeritageBrowser::showWaiting(const QString& message) {
  m_stageLabel->setText(QStringLiteral("준비 중"));
  m_detailLabel->setText(message);
}

void KaHeritageBrowser::start() {
  stop();
  if (auto* details = findChild<QPushButton*>(QStringLiteral("heritageDetails"))) details->setChecked(false);
  KaWindowGeometry::placeDownload(this);
  m_downloadRequests.clear();
  m_pendingDownloads = 0;
  m_downloadRetryReason.clear();
  m_responseOnlyRetry = false;
  m_downloadPageNeedsRecovery = false;
  m_downloadRetryWait.invalidate();
  m_downloadRetryCount = 0;
  m_downloadNavigation = false;
  const auto account = HeritageIntranetSettings::credentials();
  if (account.username.trimmed().isEmpty() || account.password.isEmpty()) {
    fail(QStringLiteral("국가유산 인트라넷 계정이 없습니다. 더보기 → 국가유산 인트라넷 아이디·비밀번호에서 넣어 주세요."));
    return;
  }
  if (m_sido.isEmpty() || m_city.isEmpty()) {
    fail(QStringLiteral("받을 시·군이 정해지지 않았습니다."));
    return;
  }
  if (m_downloadRoot.isEmpty()) {
    fail(QStringLiteral("저장할 폴더가 정해지지 않았습니다."));
    return;
  }

  ++m_generation;
  m_scriptInFlight = false;
  m_openedFrameSrc = false;
  m_lastCppProbe.clear();
  m_running = true;
  m_datasetIndex = 0;
  m_lastOutline.clear();
  m_httpStarted = false;
  m_mainView = nullptr;
  m_listRequested = false;
  m_listSent = false;
  m_agreementRecorded = false;
  m_form = HeritageForm();
  m_outline->clear();
  logLine(QStringLiteral("=== 시작: %1 %2 · 자료 %3종 ===").arg(m_sido, m_city).arg(m_datasets.size()));
  setStage(HeritageStage::Login,
           QStringLiteral("%1 로그인 중입니다.").arg(HeritageIntranetSettings::describeForLog()));
  while (m_tabs->count() > 0) {
    QWidget* w = m_tabs->widget(0);
    m_tabs->removeTab(0);
    w->deleteLater();
  }
  m_pageReady = false;
  QWebEngineView* view = addPage();
  view->load(HeritageIntranetFlow::loginUrl());
  m_poll->start();
}

void KaHeritageBrowser::runScript(const QString& script,
                                  const std::function<void(const QString&)>& then,
                                  bool requireForm) {
  if (m_scriptInFlight) return;
  m_scriptInFlight = true;
  runOnPreferredDocument(script, then, true, requireForm);
}

void KaHeritageBrowser::runOnPreferredDocument(const QString& script,
                                               const std::function<void(const QString&)>& then,
                                               bool manageFlight, bool requireForm) {
  const quint64 generation = m_generation;
  const QPointer<KaHeritageBrowser> alive(this);
  const QVector<QWebEnginePage*> pages = allPages(m_tabs);

  auto finishEmpty = [this, alive, generation, then, manageFlight]() {
    if (!alive || generation != m_generation) return;
    if (manageFlight) m_scriptInFlight = false;
    if (then) then(QStringLiteral("not-found"));
  };

  auto invoke = [this, alive, generation, script, then, manageFlight](QWebEngineFrame frame) {
    if (!alive || generation != m_generation) return;
    auto done = [this, alive, generation, then, manageFlight](const QVariant& value) {
      if (!alive || generation != m_generation) return;
      if (manageFlight) m_scriptInFlight = false;
      const QString result = value.toString();
      // 스크립트 **본문은 남기지 않는다**(로그인 스크립트에 비밀번호가 들어 있다). 결과만 남긴다.
      if (!result.isEmpty() && result.size() < 400)
        logLine(QStringLiteral("  [%1] 결과: %2")
                    .arg(HeritageIntranetFlow::stageName(m_stage), result));
      else if (!result.isEmpty())
        logLine(QStringLiteral("  [%1] 결과: %2바이트")
                    .arg(HeritageIntranetFlow::stageName(m_stage))
                    .arg(result.size()));
      if (then) then(result);
    };
    if (frame.isValid()) {
      frame.runJavaScript(script, done);
      return;
    }
    QWebEnginePage* page = activePage();
    if (!page) {
      if (manageFlight) m_scriptInFlight = false;
      if (then) then(QString());
      return;
    }
    page->runJavaScript(script, done);
  };

  auto hintFrames = [this, alive, generation, invoke, finishEmpty, manageFlight, requireForm,
                     then](QVector<QWebEngineFrame> frames, int jsFrames, QWebEnginePage* srcPage,
                           const QJsonArray& srcList) {
    if (!alive || generation != m_generation) return;
    QVector<QWebEngineFrame> valid;
    for (const QWebEngineFrame& frame : frames) {
      if (frame.isValid()) valid.push_back(frame);
    }
    const int n = valid.size();
    if (n == 0) {
      if (requireForm) {
        finishEmpty();
        return;
      }
      invoke(QWebEngineFrame());
      return;
    }
    auto pending = std::make_shared<int>(n);
    auto form = std::make_shared<QWebEngineFrame>();
    auto bestAt = std::make_shared<int>(n);
    auto probe = std::make_shared<QJsonArray>();
    for (int i = 0; i < n; ++i) {
      QWebEngineFrame copy = valid.at(i);
      copy.runJavaScript(
          HeritageIntranetFlow::formHintScript(),
          [this, alive, generation, pending, form, bestAt, copy, invoke, finishEmpty,
           manageFlight, requireForm, then, i, n, jsFrames, srcPage, srcList,
           probe](const QVariant& value) {
            if (!alive || generation != m_generation) return;
            const QString hint = value.toString();
            QJsonObject row;
            row.insert(QStringLiteral("url"), copy.url().toString());
            row.insert(QStringLiteral("name"), copy.name());
            row.insert(QStringLiteral("htmlName"), copy.htmlName());
            row.insert(QStringLiteral("hint"), hint);
            row.insert(QStringLiteral("main"), copy.isMainFrame());
            probe->append(row);
            if (hint == QLatin1String("codedeta") && copy.isValid() && i < *bestAt) {
              *form = copy;
              *bestAt = i;
            }
            if (--(*pending) != 0) return;
            QJsonObject wrap;
            wrap.insert(QStringLiteral("cppFrames"), n);
            wrap.insert(QStringLiteral("jsFrames"), jsFrames);
            wrap.insert(QStringLiteral("cpp"), *probe);
            wrap.insert(QStringLiteral("html"), srcList);
            m_lastCppProbe = QString::fromUtf8(QJsonDocument(wrap).toJson(QJsonDocument::Compact));
            if (*bestAt < n) {
              invoke(*form);
              return;
            }
            if (requireForm) {
              finishEmpty();
              return;
            }
            invoke(QWebEngineFrame());
          });
    }
  };

  auto frames = std::make_shared<QVector<QWebEngineFrame>>();
  for (const QWebEngineFrame& frame : allFrames(m_tabs)) appendUnique(frames.get(), frame);

  if (pages.isEmpty()) {
    hintFrames(*frames, 0, nullptr, {});
    return;
  }

  auto pending = std::make_shared<int>(pages.size());
  auto jsMax = std::make_shared<int>(0);
  auto srcPage = std::make_shared<QPointer<QWebEnginePage>>(pages.last());
  auto srcList = std::make_shared<QJsonArray>();
  for (QWebEnginePage* page : pages) {
    const QPointer<QWebEnginePage> guardedPage(page);
    QWebEngineFrame main = page->mainFrame();
    if (!main.isValid()) {
      if (--(*pending) == 0) hintFrames(*frames, *jsMax, srcPage->data(), *srcList);
      continue;
    }
    main.runJavaScript(
        HeritageIntranetFlow::frameInventoryScript(),
        [this, alive, generation, pending, frames, jsMax, srcPage, srcList, guardedPage, hintFrames,
         manageFlight](const QVariant& value) {
          if (!alive || generation != m_generation) return;
          if (!guardedPage) {
            if (--(*pending) == 0) hintFrames(*frames, *jsMax, srcPage->data(), *srcList);
            return;
          }
          const QJsonObject info = QJsonDocument::fromJson(value.toString().toUtf8()).object();
          *jsMax = qMax(*jsMax, info.value(QStringLiteral("jsFrames")).toInt());
          const QJsonArray list = info.value(QStringLiteral("list")).toArray();
          mergeNamedFrames(guardedPage.data(), list, frames.get());
          if (list.size() > srcList->size()) {
            *srcList = list;
            *srcPage = guardedPage;
          }
          if (--(*pending) == 0) hintFrames(*frames, *jsMax, srcPage->data(), *srcList);
        });
  }
}

void KaHeritageBrowser::captureOutline(const std::function<void()>& then) {
  runOnPreferredDocument(
      HeritageIntranetFlow::pageOutlineScript(),
      [this, then](const QString& value) {
        m_lastOutline = value;
        QJsonDocument doc = QJsonDocument::fromJson(m_lastOutline.toUtf8());
        if (doc.isObject() && !m_lastCppProbe.isEmpty()) {
          const QJsonDocument probe = QJsonDocument::fromJson(m_lastCppProbe.toUtf8());
          if (probe.isObject()) {
            QJsonObject object = doc.object();
            const QJsonObject extra = probe.object();
            object.insert(QStringLiteral("cppFrames"), extra.value(QStringLiteral("cppFrames")));
            object.insert(QStringLiteral("jsFrames"), extra.value(QStringLiteral("jsFrames")));
            object.insert(QStringLiteral("cpp"), extra.value(QStringLiteral("cpp")));
            object.insert(QStringLiteral("html"), extra.value(QStringLiteral("html")));
            doc.setObject(object);
            m_lastOutline = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
          }
        }
        QString text = doc.isNull() ? m_lastOutline
                                    : QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        // 화면에서 무엇을 찾았는지보다 **무엇을 보냈는지**가 더 확실한 단서다.
        if (m_requestLog) {
          const QStringList sent = m_requestLog->lines();
          if (!sent.isEmpty())
            text += QStringLiteral("\n\n== 사이트가 보낸 요청 (최근 %1개) ==\n%2")
                        .arg(sent.size())
                        .arg(sent.join(QLatin1Char('\n')));
        }
        m_outline->setPlainText(text);
        if (then) then();
      },
      false, false);
}

HeritageStage KaHeritageBrowser::nextStage(HeritageStage stage) const {
  switch (stage) {
    case HeritageStage::Login: return HeritageStage::DismissTutorial;
    case HeritageStage::DismissTutorial: return HeritageStage::OpenDownloadPage;
    case HeritageStage::OpenDownloadPage: return HeritageStage::AgreeTerms;
    case HeritageStage::AgreeTerms: return HeritageStage::SelectDataset;
    case HeritageStage::SelectDataset: return HeritageStage::SelectRegion;
    case HeritageStage::SelectRegion: return HeritageStage::Search;
    case HeritageStage::Search: return HeritageStage::CollectResults;
    case HeritageStage::CollectResults: return HeritageStage::Download;
    default: return HeritageStage::Done;
  }
}

void KaHeritageBrowser::runStage() {
  if (!m_running) return;
  if (m_stage == HeritageStage::Download && !m_downloadRetryReason.isEmpty()) {
    // 다른 전송이 남아 있으면 끝까지 기다린 뒤 현재 종류를 새 요청으로 다시 받는다.
    if (m_pendingDownloads > 0 || m_scriptInFlight) return;
    if (!m_downloadRetryWait.isValid()) m_downloadRetryWait.start();
    const qint64 remaining = m_downloadRetryDelayMs - m_downloadRetryWait.elapsed();
    if (remaining > 0) {
      m_detailLabel->setText(QStringLiteral("%1 — %2초 후 공식 화면에서 다시 받기 (%3회) · 중지 가능")
          .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)))
          .arg((remaining + 999) / 1000).arg(m_downloadRetryCount));
      return;
    }
    reopenDownloadSession();
    return;
  }
  // ZIP 생성 대기는 화면 탐색 제한이나 수신 후의 짧은 안정화 대기와 다르다.
  // 요청은 한 번만 보내고 Chromium의 실제 파일 수신 신호를 기다린다.
  if (m_stage == HeritageStage::Download && !m_downloadMode.isEmpty()) {
    m_waitTicks = 0;
    if (!m_downloadRequestObserved && m_requestLog->downloadRequestCount() > m_downloadRequestBaseline) {
      m_downloadRequestObserved = true;
      logLine(QStringLiteral("공식 전체다운로드 요청 전송 확인 · 파일 응답 대기"));
      saveRequestLog();
    }
    if (m_pendingDownloads > 0) return;
    if (m_currentFiles.isEmpty()) {
      const qint64 elapsed = m_downloadWait.elapsed();
      // 이 시간은 국가유산 서버가 ZIP 을 만드는 시간이다(측정 42~115초). 우리가 줄일
      // 수 없으므로 무엇을 기다리는지 밝혀 둔다.
      const QString phase = m_downloadRequestObserved
          ? QStringLiteral("국가유산 서버가 파일을 만드는 중")
          : QStringLiteral("버튼 실행 완료 · 다운로드 요청 미확인");
      m_detailLabel->setText(QStringLiteral("%1 — %2 · %3초 경과(보통 40~120초) · 중지 가능")
                                .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)), phase)
                                .arg(elapsed / 1000));
      return;
    }
  }
  if (++m_waitTicks > kMaxWaitTicks) {
    fail(QStringLiteral("「%1」 단계에서 화면이 예상과 달라 멈췄습니다. 아래 내용을 보고 알려 주세요.")
             .arg(HeritageIntranetFlow::stageName(m_stage)));
    return;
  }
  if (m_scriptInFlight) return;
  // 로그인 리다이렉트 뒤 지도/타일 로딩이 남아도 인증된 DOM은 먼저 준비될 수 있다.
  // 이 두 단계는 실제 로그인 폼/로그아웃 표시를 확인하며 전체 loadFinished를 기다리지 않는다.
  const bool receivedFilesReady = m_stage == HeritageStage::Download &&
      !m_downloadMode.isEmpty() && m_pendingDownloads == 0 && !m_currentFiles.isEmpty();
  if (!m_pageReady && !receivedFilesReady && m_stage != HeritageStage::Login &&
      m_stage != HeritageStage::DismissTutorial) return;
  if (m_settle > 0) {
    --m_settle;
    return;  // 화면이 자리잡기를 기다린다
  }

  // frameset 위에서는 어떤 요소도 못 찾는다. 안쪽 프레임 주소로 직접 들어간다.
  // **페이지가 새로 뜰 때 한 번만** 본다. 매 번 돌리면 단계 스크립트가 영영 돌지 못한다.
  // 프레임 안으로 **이동하지 않는다.** 이 사이트의 루트는 frameset(top 0% + main)이라
  // 안쪽 주소로 옮겨 가면 사이트 자신의 스크립트가 깨지거나 빈 top 프레임이 열린다.
  // 안쪽 문서는 runOnPreferredDocument / cands() 로 이동 없이 읽는다.
  //

  switch (m_stage) {
    case HeritageStage::Login: {
      // 이미 들어가 있으면 다시 로그인하지 않는다(세션이 살아 있을 수 있다).
      runScript(HeritageIntranetFlow::loginProbeScript(), [this](const QString& probe) {
        if (probe == QLatin1String("past-login")) {
          m_pageReady = true;
          setStage(HeritageStage::DismissTutorial, QStringLiteral("이미 로그인되어 있습니다."));
          return;
        }
        if (probe != QLatin1String("still-login")) return;
        const auto account = HeritageIntranetSettings::credentials();
        // 이 스크립트에는 비밀번호가 들어 있다. 어디에도 보관하거나 찍지 않는다.
        runScript(HeritageIntranetFlow::loginScript(account.username, account.password),
                  [this](const QString& result) {
                    if (result == QLatin1String("no-form")) return;  // 아직 로딩 중일 수 있다
                    if (result == QLatin1String("no-goLogin")) {
                      fail(QStringLiteral("로그인 화면이 바뀌었습니다(goLogin 없음)."));
                      return;
                    }
                    if (result.startsWith(QLatin1String("error:"))) {
                      fail(QStringLiteral("로그인 중 오류가 났습니다: %1").arg(result.mid(6)));
                      return;
                    }
                    if (result == QLatin1String("submitted"))
                      setStage(HeritageStage::DismissTutorial,
                               QStringLiteral("로그인 결과를 기다리는 중입니다."));
                  });
      });
      return;
    }
    case HeritageStage::DismissTutorial: {
      // 로그인이 실제로 됐는지부터 본다. 아직 로그인 칸이 보이면 실패다.
      runScript(HeritageIntranetFlow::loginProbeScript(), [this](const QString& result) {
        if (result == QLatin1String("still-login")) {
          if (m_waitTicks > 8)
            fail(QStringLiteral("로그인이 되지 않았습니다. 아이디·비밀번호를 확인해 주세요."));
          return;
        }
        if (result != QLatin1String("past-login")) return;
        m_pageReady = true;
        runScript(HeritageIntranetFlow::dismissTutorialScript(), [this](const QString& closed) {
          // 닫혔는지 확인하기 전에는 다음으로 가지 않는다.
          // 튜토리얼이 떠 있으면 상단 메뉴를 누를 수 없다.
          if (closed == QLatin1String("closed")) {
            m_detailLabel->setText(QStringLiteral("튜토리얼을 닫는 중입니다…"));
            return;  // 다음 차례에 사라졌는지 다시 본다
          }
          if (closed == QLatin1String("none"))
            setStage(HeritageStage::OpenDownloadPage,
                     QStringLiteral("국가유산자료 다운로드 화면을 엽니다."));
        });
      });
      return;
    }
    case HeritageStage::OpenDownloadPage:
      // 눌렀다고 도착한 것으로 치지 않는다. 화면이 실제로 바뀐 뒤에 넘어간다.
      runScript(HeritageIntranetFlow::downloadPageProbeScript(), [this](const QString& probe) {
        if (probe == QLatin1String("page-error")) {
          fail(QStringLiteral("국가유산 사이트가 찾을 수 없는 페이지를 표시했습니다."));
          return;
        }
        if (probe == QLatin1String("ready")) {
          setStage(HeritageStage::AgreeTerms, QStringLiteral("서약서에 동의합니다."));
          return;
        }
        if (m_downloadPageRequested) return;
        runScript(HeritageIntranetFlow::openDownloadPageScript(), [this](const QString& result) {
          if (result.startsWith(QLatin1String("error:"))) {
            fail(QStringLiteral("국가유산 다운로드 화면 열기에 실패했습니다: %1").arg(result.mid(6)));
            return;
          }
          if (result == QLatin1String("opening") || result == QLatin1String("already-open"))
            m_downloadPageRequested = true;
          if (result == QLatin1String("not-found"))
            m_detailLabel->setText(QStringLiteral("메뉴에서 「국가유산자료 다운로드」를 찾는 중…"));
          else
            m_detailLabel->setText(QStringLiteral("다운로드 화면을 여는 중…"));
        });
      });
      return;
    case HeritageStage::AgreeTerms:
      if (m_agreementSubmitted) {
        runScript(HeritageIntranetFlow::formHintScript(), [this](const QString& result) {
          if (result == QLatin1String("codedeta"))
            setStage(HeritageStage::SelectDataset, QStringLiteral("서약 완료 · 자료 목록이 도착했습니다."));
        });
        return;
      }
      runScript(HeritageIntranetFlow::agreeTermsScript(), [this](const QString& result) {
        if (result == QLatin1String("already-open")) {
          // 이미 동의한 세션은 서약서 없이 다운로드 폼이 바로 열린다.
          setStage(HeritageStage::SelectDataset, QStringLiteral("자료 종류를 고릅니다."));
          return;
        }
        const QJsonObject receipt = QJsonDocument::fromJson(result.toUtf8()).object();
        if (receipt.value(QStringLiteral("status")).toString() == QLatin1String("agreed")) {
          m_agreementSubmitted = true;
          emit agreementAccepted(QDateTime::currentDateTime(), receipt.value(QStringLiteral("text")).toString());
          m_detailLabel->setText(QStringLiteral("서약서를 제출했습니다. 다운로드 목록 응답을 기다립니다…"));
          return;
        }
        if (result == QLatin1String("checked-no-confirm"))
          fail(QStringLiteral("서약서에 체크했지만 「확인」 버튼을 찾지 못했습니다."));
      });
      return;
    case HeritageStage::SelectDataset: {
      if (m_datasetIndex >= m_datasets.size()) {
        setStage(HeritageStage::Done, QStringLiteral("모두 마쳤습니다."));
        m_running = false;
        m_poll->stop();
        emit allFinished();
        return;
      }
      if (!m_datasetSelectionStarted) {
        runScript(HeritageIntranetFlow::selectDatasetScript(m_datasets.at(m_datasetIndex)),
                  [this](const QString& result) {
                    if (result == QLatin1String("already-selected")) {
                      readDatasetBaseline();
                    } else if (result == QLatin1String("selected")) {
                      m_datasetSelectionStarted = true;
                      m_detailLabel->setText(QStringLiteral("자료 목록 응답을 기다립니다…"));
                    } else if (result.startsWith(QLatin1String("error:"))) {
                      fail(QStringLiteral("자료 목록 요청에 실패했습니다."));
                    }
                  }, true);
      } else {
        runScript(HeritageIntranetFlow::datasetReadyScript(m_datasets.at(m_datasetIndex)),
                  [this](const QString& ready) {
                    if (ready == QLatin1String("ready")) readDatasetBaseline();
                  }, true);
      }
      return;
    }
    case HeritageStage::SelectRegion:
      runScript(HeritageIntranetFlow::selectRegionScript(m_sido, m_city),
                [this](const QString& result) {
                  if (result == QLatin1String("no-sido")) {
                    fail(QStringLiteral("시·도 목록에서 「%1」을 찾지 못했습니다. 표기가 다릅니다.")
                             .arg(m_sido));
                    return;
                  }
                  if (result == QLatin1String("not-found")) {
                    m_detailLabel->setText(QStringLiteral("다운로드 폼에서 시·도 칸을 찾는 중…"));
                    if (m_waitTicks > 20)
                      fail(QStringLiteral("다운로드 폼에서 시·도 칸을 찾지 못했습니다."));
                    return;
                  }
                  if (result == QLatin1String("selected")) {
                    // 「골랐다」를 한 번 보고 믿지 않는다. 탭이 바뀌면 폼이 다시 그려지고
                    // 시/군 목록은 ajax 로 나중에 채워진다(2026-09-12: 1초 만에 selected 를
                    // 받았지만 실제로는 비어 전국을 요청했다). 값이 남아 있는지 다시 본다.
                    runScript(HeritageIntranetFlow::verifyRegionScript(),
                              [this](const QString& check) {
                                if (!check.startsWith(QLatin1String("ok:"))) {
                                  m_detailLabel->setText(
                                      QStringLiteral("시·군이 아직 폼에 남지 않았습니다. 다시 고릅니다…"));
                                  return;  // 다음 차례에 다시 고른다
                                }
                                setStage(HeritageStage::Search,
                                         QStringLiteral("검색합니다. (%1)").arg(check.mid(3)));
                              },
                              true);
                    return;
                  }
                  if (result == QLatin1String("no-sigungu-value")) {
                    fail(QStringLiteral("시·군 값이 폼에 들어가지 않았습니다. "
                                        "전국 자료를 받지 않기 위해 멈춥니다."));
                    return;
                  }
                  // 시/군/구는 시/도를 고른 뒤 Ajax 로 채워진다. 몇 개나 찼는지 보여 준다.
                  if (result == QLatin1String("sigungu-pending")) {
                    m_detailLabel->setText(QStringLiteral("시·군·구 칸이 아직 없습니다…"));
                    if (m_waitTicks > 20)
                      fail(QStringLiteral("다운로드 폼에서 시·군·구 칸을 찾지 못했습니다."));
                    return;
                  }
                  if (result.startsWith(QLatin1String("sigungu:"))) {
                    const int count = result.mid(8).toInt();
                    m_detailLabel->setText(
                        QStringLiteral("시·군·구 목록 %1개에서 「%2」을(를) 찾는 중…")
                            .arg(count)
                            .arg(m_city));
                    if (count > 0 && m_waitTicks > 12)
                      fail(QStringLiteral("시·군·구 목록 %1개에 「%2」이(가) 없습니다. 표기를 확인해야 합니다.")
                               .arg(count)
                               .arg(m_city));
                  }
                },
                true);
      return;
    case HeritageStage::Search:
      runScript(HeritageIntranetFlow::searchScript(),
                [this](const QString& result) {
                  if (result == QLatin1String("not-found")) {
                    if (m_waitTicks > 15)
                      fail(QStringLiteral("다운로드 폼에서 「검색」을 찾지 못했습니다."));
                    return;
                  }
                  if (result != QLatin1String("searching")) return;
                  // 「10개씩 보기」를 건드리지 않는다. 목록이 다시 그려지면서
                  // 전체다운로드 함수가 읽는 요소가 사라져 null.value 로 터진다(2026-09-12).
                  // 전체다운로드는 쪽 크기와 무관하게 검색결과 전부를 받는다.
                  setStage(HeritageStage::CollectResults, QStringLiteral("결과를 모읍니다."));
                },
                true);
      return;
    case HeritageStage::CollectResults:
      runScript(HeritageIntranetFlow::resultPageInfoScript(),
                [this](const QString& result) {
        const QJsonObject info = QJsonDocument::fromJson(result.toUtf8()).object();
        if (!info.value(QStringLiteral("searchReady")).toBool()) return;
        const int rows = info.value(QStringLiteral("rows")).toInt();
        const int total = info.value(QStringLiteral("total")).toInt(-1);
        if (total == 0 && m_totalBeforeSearch > 0) {
          logLine(QStringLiteral("%1: 검색 응답 완료 · 해당 시·군 자료 0건")
                      .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
          advanceDataset();
          return;
        }
        if (rows <= 0) return;
        if (total < 0 || m_totalBeforeSearch <= 0) {
          // 아직 갱신 중일 수 있다. 건수를 못 읽었다고 바로 포기하지 않는다.
          if (m_waitTicks > 20)
            fail(QStringLiteral("검색 건수를 읽지 못해 전국 받기를 하지 않습니다"
                                "(검색 전 %1건 · 검색 후 %2건).")
                     .arg(m_totalBeforeSearch)
                     .arg(total));
          return;
        }
        if (total == m_totalBeforeSearch) {
          // 검색 직후에는 화면이 아직 옛 건수를 보이고 있다(2026-09-12: 8302 를 읽고 막았는데
          // 실제로는 곧 90 으로 바뀌었다). 몇 번 더 보고 나서 판단한다.
          m_detailLabel->setText(
              QStringLiteral("검색 결과를 기다립니다… (아직 %1건)").arg(total));
          if (m_waitTicks > 20)
            fail(QStringLiteral("시·군 조건이 걸리지 않았습니다(검색결과가 %1건 그대로입니다). "
                                "전국 자료를 받지 않기 위해 멈춥니다.")
                     .arg(total));
          return;
        }
        m_lastPage = qMax(1, info.value(QStringLiteral("lastPage")).toInt(1));
        m_currentPage = 1;
        m_currentFiles.clear();
        m_pendingDownloads = 0;
        m_idleTicks = 0;
        m_downloadMode.clear();
        setStage(HeritageStage::Download,
                 QStringLiteral("%1 — 결과 %2줄 · 전체 %3쪽, 내려받습니다.")
                     .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)))
                     .arg(rows)
                     .arg(m_lastPage));
                },
                true);
      return;

    case HeritageStage::Download: {
      if (m_pendingDownloads > 0) {
        m_idleTicks = 0;
        m_waitTicks = 0;  // 큰 파일은 오래 걸린다. 받는 중이면 기다린다
        return;
      }
      if (!m_downloadMode.isEmpty()) {
        // 파일 수신 완료 뒤에만 추가 파일이 오는지 잠깐 기다린다.
        if (++m_idleTicks < 6) return;
        if (m_currentFiles.isEmpty()) {
          fail(QStringLiteral("「%1」에서 내려받기를 눌렀지만 파일이 오지 않았습니다.")
                   .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
          return;
        }
        // 쪽마다 받는 방식이면 다음 쪽으로 넘긴다. 한 쪽만 받고 끝내지 않는다.
        if (m_downloadMode == QLatin1String("selected") && m_currentPage < m_lastPage) {
          const int next = m_currentPage + 1;
          runScript(HeritageIntranetFlow::goToPageScript(next),
                    [this, next](const QString& moved) {
                      if (moved == QLatin1String("moved")) {
                        m_currentPage = next;
                        m_downloadMode.clear();
                        m_idleTicks = 0;
                        m_waitTicks = 0;
                        m_detailLabel->setText(
                            QStringLiteral("%1쪽 / %2쪽").arg(next).arg(m_lastPage));
                      } else {
                        m_lastPage = m_currentPage;
                      }
                    },
                    true);
          return;
        }
        // 받은 것이 진짜인지 확인하고 넘어간다. 0바이트 빈 파일을 받은 적이 있다(2026-09-12).
        QStringList good;
        qint64 totalBytes = 0;
        for (const QString& path : std::as_const(m_currentFiles)) {
          const QFileInfo info(path);
          if (!info.exists() || info.size() <= 0) {
            scheduleDownloadRetry(QStringLiteral("저장된 파일이 없거나 비어 있습니다."));
            return;
          }
          good << path;
          totalBytes += info.size();
        }
        if (good.isEmpty()) {
          scheduleDownloadRetry(QStringLiteral("「%1」에서 받은 파일이 모두 비어 있습니다.")
                   .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
          return;
        }
        m_currentFiles = good;
        logLine(QStringLiteral("확인: %1 파일 %2개 · %3KB")
                    .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex)))
                    .arg(good.size())
                    .arg(totalBytes / 1024));

        // 이 종류는 끝났다. 지도에 올리게 넘기고 다음 종류로 간다.
        m_progress->setRange(0, 0);
        m_detailLabel->setText(QStringLiteral("전송 종료 · ZIP 전체 읽기 및 지도 적재 확인 중"));
        emit datasetReady(m_datasets.at(m_datasetIndex), m_currentFiles);
        if (!m_running || m_stage == HeritageStage::Failed || !m_downloadRetryReason.isEmpty()) return;
        for (const QString& path : std::as_const(m_currentFiles)) emit fileDownloaded(path);
        advanceDataset();
        return;
      }
      // 다운로드 이벤트가 스크립트 완료 콜백보다 먼저 올 수 있다.
      // 버튼을 누르기 전에 수신 준비를 하고, 성공은 실제 파일 완료로만 판정한다.
      m_downloadMode = QStringLiteral("all");
      m_downloadWait.start();
      m_downloadRequestBaseline = m_requestLog->downloadRequestCount();
      m_downloadRequestObserved = false;
      m_downloadNavigation = true;
      m_idleTicks = 0;
      runScript(HeritageIntranetFlow::downloadAllScript(),
                [this](const QString& result) {
                  if (!m_running || m_stage != HeritageStage::Download) return;
                  if (result == QLatin1String("not-found")) {
                    fail(QStringLiteral("현재 검색 결과에서 전체다운로드 버튼을 찾지 못했습니다. "
                                        "다운로드는 시작되지 않았습니다."));
                    return;
                  }
                  if (result == QLatin1String("no-region")) {
                    fail(QStringLiteral("「%1」에서 시·군이 폼에 들어가지 않았습니다. "
                                        "전국 자료를 받지 않기 위해 멈춥니다.")
                             .arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
                    return;
                  }
                  if (result.startsWith(QLatin1String("error:"))) {
                    fail(QStringLiteral("공식 전체다운로드 버튼 실행 중 사이트 오류가 발생했습니다 (%1). "
                                        "파일 수신이 완료되지 않아 다음 자료로 넘어가지 않습니다.")
                             .arg(result.mid(6)));
                    return;
                  }
                  if (result != QLatin1String("all")) {
                    fail(QStringLiteral("현재 검색 결과의 전체다운로드 버튼을 정확히 확인하지 못했습니다."));
                    return;
                  }
                  m_waitTicks = 0;
                  logLine(QStringLiteral("공식 전체다운로드 버튼 1회 실행 · 실제 요청 및 파일 수신 확인 대기"));
                  m_detailLabel->setText(QStringLiteral("전체다운로드 버튼 실행 · 파일 수신 확인 대기"));
                },
                true);
      return;
    }
    default:
      return;
  }
}

void KaHeritageBrowser::readDatasetBaseline() {
  runScript(HeritageIntranetFlow::resultPageInfoScript(), [this](const QString& value) {
    const int total = QJsonDocument::fromJson(value.toUtf8()).object()
                          .value(QStringLiteral("total")).toInt(-1);
    if (total <= 0) return;
    m_totalBeforeSearch = total;
    setStage(HeritageStage::SelectRegion, QStringLiteral("%1 %2 선택").arg(m_sido, m_city));
  }, true);
}

void KaHeritageBrowser::advanceDataset() {
  closeDownloadPopups(true);
  m_downloadRetryCount = 0;
  m_downloadRetryReason.clear();
  m_responseOnlyRetry = false;
  m_downloadRetryWait.invalidate();
  ++m_datasetIndex;
  if (m_datasetIndex >= m_datasets.size()) {
    setStage(HeritageStage::Done, QStringLiteral("자료 %1종 처리를 마쳤습니다. 자료가 있는 종류는 다운로드와 지도 적재를 완료했습니다.").arg(m_datasets.size()));
    m_running = false;
    m_poll->stop();
    emit allFinished();
    return;
  }
  if (m_downloadPageNeedsRecovery) {
    reopenDownloadSession();
    return;
  }
  m_totalBeforeSearch = -1;
  setStage(HeritageStage::SelectDataset,
           QStringLiteral("다음 자료: %1").arg(HeritageStyle::layerName(m_datasets.at(m_datasetIndex))));
}

void KaHeritageBrowser::handleDownload(QWebEngineDownloadRequest* request) {
  if (!request || request->state() != QWebEngineDownloadRequest::DownloadRequested) return;
  if (request->page() && !allPages(m_tabs).contains(request->page())) {
    request->cancel();  // 닫은 이전 시도의 지연 응답을 새 자료에 섞지 않는다.
    return;
  }
  if (!m_running || m_stage != HeritageStage::Download) {
    request->cancel();
    return;
  }
  if (m_downloadRoot.isEmpty()) {
    request->cancel();
    fail(QStringLiteral("저장할 폴더가 없어 내려받기를 취소했습니다."));
    return;
  }
  const QString directory =
      QDir(m_downloadRoot).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
  if (!QDir().mkpath(directory)) {
    request->cancel();
    fail(QStringLiteral("저장 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요."));
    return;
  }
  const QString name = safeName(request->suggestedFileName());
  // 팝업 HTML/오류 신호보다 파일 전환 알림이 늦게 올 수 있다.
  // 실제 파일이 도착하면 화면 응답만으로 예약했던 재시도는 철회한다.
  if (m_responseOnlyRetry) {
    m_downloadRetryReason.clear();
    m_downloadRetryWait.invalidate();
    m_responseOnlyRetry = false;
    logLine(QStringLiteral("실제 파일 수신 시작 · 화면 응답 재시도 예약 해제"));
  }
  request->setDownloadDirectory(directory);
  request->setDownloadFileName(name);
  ++m_pendingDownloads;
  m_idleTicks = 0;
  logLine(QStringLiteral("파일 수신 시작: %1 · 요청 후 %2초")
              .arg(name).arg(m_downloadWait.elapsed() / 1000));
  m_detailLabel->setText(QStringLiteral("%1 — 파일 수신 중").arg(name));
  const quint64 generation = m_generation;
  m_downloadRequests.append(request);
  const auto showProgress = [this, request, generation, name]() {
    if (!m_running || generation != m_generation || request->isFinished()) return;
    const qint64 received = request->receivedBytes();
    const qint64 total = request->totalBytes();
    if (total > 0) {
      m_progress->setRange(0, 100);
      m_progress->setValue(qBound(0, static_cast<int>(100.0 * received / total), 100));
      m_progress->setFormat(QStringLiteral("수신 %p% · 완료 신호 대기"));
    } else {
      m_progress->setRange(0, 0);
    }
    const QString amount = total > 0
        ? QStringLiteral("%1 / %2 KB").arg(received / 1024).arg(total / 1024)
        : QStringLiteral("%1 KB 수신").arg(received / 1024);
    m_detailLabel->setText(QStringLiteral("%1 — %2 · 완료 확인 대기").arg(name, amount));
  };
  connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this, showProgress);
  connect(request, &QWebEngineDownloadRequest::totalBytesChanged, this, showProgress);
  connect(request, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, request, generation]() {
    if (!m_running || generation != m_generation) return;
    if (!request->isFinished()) return;
    if (!m_downloadRequests.removeOne(QPointer<QWebEngineDownloadRequest>(request))) return;
    if (m_pendingDownloads > 0) --m_pendingDownloads;
    if (request->state() != QWebEngineDownloadRequest::DownloadCompleted) {
      const QString reason = QStringLiteral("파일을 끝까지 받지 못했습니다: %1")
                                 .arg(request->interruptReasonString());
      switch (request->interruptReason()) {
        case QWebEngineDownloadRequest::FileTooShort:
        case QWebEngineDownloadRequest::NetworkFailed:
        case QWebEngineDownloadRequest::NetworkTimeout:
        case QWebEngineDownloadRequest::NetworkDisconnected:
        case QWebEngineDownloadRequest::NetworkServerDown:
        case QWebEngineDownloadRequest::ServerFailed:
        case QWebEngineDownloadRequest::ServerBadContent:
        case QWebEngineDownloadRequest::ServerUnreachable:
          scheduleDownloadRetry(reason);
          break;
        default:
          fail(reason);
          break;
      }
      return;
    }
    const QString path =
        QDir(request->downloadDirectory()).filePath(request->downloadFileName());
    const QFileInfo saved(path);
    logLine(QStringLiteral("전송 종료: %1 · 상태 %2 · 예상 %3 · 수신 %4 · 저장 %5 바이트")
        .arg(request->downloadFileName()).arg(static_cast<int>(request->state()))
        .arg(request->totalBytes()).arg(request->receivedBytes()).arg(saved.size()));
    if (!saved.isFile() || saved.size() <= 0) {
      scheduleDownloadRetry(QStringLiteral("다운로드 완료 신호를 받았지만 저장된 파일이 없거나 비어 있습니다."));
      return;
    }
    if (saved.size() != request->receivedBytes() ||
        (request->totalBytes() > 0 && saved.size() != request->totalBytes())) {
      scheduleDownloadRetry(QStringLiteral("예상·수신·저장 파일 크기가 일치하지 않습니다."));
      return;
    }
    m_currentFiles << path;
    logLine(QStringLiteral("받음: %1").arg(path));
    // 재수신 여부는 ZIP/SHP 검사 결과로 결정하므로 그때까지 검색 폼을 보존한다.
    m_idleTicks = 0;
    m_detailLabel->setText(QStringLiteral("받은 파일 %1개").arg(m_currentFiles.size()));
  });
  request->accept();
}

QWebEngineView* KaHeritageBrowser::addPage(QWebEnginePage* opener) {
  Q_UNUSED(opener);
  ensureProfile();
  const quint64 generation = m_generation;
  auto* view = new QWebEngineView(m_tabs);
  view->setProperty("heritageDownloadPopup", m_running && m_stage == HeritageStage::Download);
  auto* page = new HeritagePage(
      m_profile, view, [this](QWebEnginePage*) { return addPage(); },
      [this](const QString& message) {
        // 사이트가 띄운 알림. 단계 표시에 남겨 무엇 때문에 멈췄는지 보이게 한다.
        const QString one = QString(message).replace(QLatin1Char('\n'), QLatin1Char(' ')).trimmed();
        m_lastAlert = one;
        m_detailLabel->setText(QStringLiteral("사이트 알림: %1").arg(one.left(120)));
      });
  view->setPage(page);
  const int index = m_tabs->addTab(view, QStringLiteral("국가유산 인트라넷"));
  m_tabs->setCurrentIndex(index);
  // 내려받기용으로 여는 탭은 화면이 뜨는 것이 아니라 파일이 떨어지는 것이라
  // loadFinished 가 오지 않는다. 그 탭 때문에 준비 상태를 내리면 흐름이 영영 멈춘다
  // (2026-09-12: 「받은 파일 1개」에서 다음 종류로 넘어가지 못했다).
  // 그래서 **작업 화면(첫 탭)만** 준비 상태를 관리한다.
  const bool isMain = (m_mainView == nullptr);
  if (isMain) m_mainView = view;
  connect(view, &QWebEngineView::loadStarted, this, [this, view, generation]() {
    if (generation != m_generation || m_tabs->indexOf(view) < 0) return;
    if (view != m_mainView) return;
    if (m_downloadNavigation) {
      m_downloadNavigation = false;
      logLine(QStringLiteral("파일 요청 시작 · 작업 화면 준비 상태 유지"));
      return;
    }
    m_pageReady = false;
    logLine(QStringLiteral("작업 화면 로딩 시작"));
    m_frameChecked = false;  // 새 화면이면 프레임을 다시 본다
  });
  connect(view, &QWebEngineView::loadFinished, this, [this, view, generation](bool ok) {
    if (generation != m_generation || m_tabs->indexOf(view) < 0) return;
    if (view == m_mainView)
      logLine(QStringLiteral("작업 화면 로딩 종료: %1").arg(ok ? QStringLiteral("성공") : QStringLiteral("중단/실패")));
    if (!ok || view != m_mainView) return;
    m_pageReady = true;
    m_frameChecked = false;
    m_waitTicks = 0;
    m_tabs->setCurrentWidget(view);
  });
  connect(page, &QWebEnginePage::loadingChanged, this,
          [this, view, generation](const QWebEngineLoadingInfo& info) {
    if (!m_running || generation != m_generation || m_tabs->indexOf(view) < 0) return;
    const bool downloadStage = m_stage == HeritageStage::Download && !m_downloadMode.isEmpty();
    const bool downloadUrl = info.url().host() == HeritageIntranetFlow::portalUrl().host() &&
        info.url().path() == HeritageIntranetFlow::downloadFilesAllPath();
    if (downloadStage && downloadUrl) view->setProperty("heritageFileResponse", true);
    const bool fileResponse = downloadStage && view->property("heritageFileResponse").toBool();
    if (fileResponse) {
      // 응답 본문/쿠키/위치 헤더는 기록하지 않는다. 실제 전송 관련 헤더만 보존한다.
      QStringList headers;
      const auto responseHeaders = info.responseHeaders();
      for (auto it = responseHeaders.cbegin(); it != responseHeaders.cend(); ++it) {
        const QByteArray name = it.key().toLower();
        if (name != "content-length" && name != "content-type" &&
            name != "content-encoding" && name != "transfer-encoding") continue;
        const QString value = QString::fromLatin1(it.value()).replace(QLatin1Char('\r'), QLatin1Char(' '))
            .replace(QLatin1Char('\n'), QLatin1Char(' ')).left(120);
        headers << QStringLiteral("%1=%2").arg(QString::fromLatin1(name), value);
      }
      logLine(QStringLiteral("파일 응답: 상태 %1 · 다운로드 %2 · 오류영역 %3 · 코드 %4 · %5")
          .arg(static_cast<int>(info.status())).arg(info.isDownload())
          .arg(static_cast<int>(info.errorDomain())).arg(info.errorCode())
          .arg(headers.isEmpty() ? QStringLiteral("전송 헤더 없음") : headers.join(QStringLiteral("; "))));
    }
    if (info.isDownload()) return;
    if (downloadStage) {
      // 화면 오류가 Chromium의 별도 파일 전송까지 취소하지 않게 한다.
      // 실제 전송/저장 파일이 있으면 그 완료 및 ZIP 검사 결과를 우선한다.
      const bool mainFailed = view == m_mainView &&
          info.status() == QWebEngineLoadingInfo::LoadFailedStatus && info.errorCode() != -3;
      if (mainFailed) m_downloadPageNeedsRecovery = true;
      if (m_pendingDownloads > 0 || !m_currentFiles.isEmpty()) return;
      if (fileResponse && info.status() == QWebEngineLoadingInfo::LoadSucceededStatus &&
          !info.url().isEmpty() && info.url().scheme() != QLatin1String("about")) {
        scheduleDownloadRetry(QStringLiteral("파일 대신 웹페이지가 돌아왔습니다. 공식 화면에서 다시 받습니다."), true);
      } else if (fileResponse && info.status() == QWebEngineLoadingInfo::LoadFailedStatus &&
                 info.errorCode() != -3) {
        scheduleDownloadRetry(QStringLiteral("파일 응답 오류 %1. 공식 화면에서 다시 받습니다.").arg(info.errorCode()), true);
      } else if (mainFailed) {
        logLine(QStringLiteral("작업 화면 오류 %1 · 진행 중 파일 응답은 취소하지 않습니다.").arg(info.errorCode()));
        // 다운로드 팝업이 응답 중이면 계속 기다린다. 그 창도 없는 경우에만 복구한다.
        bool responsePending = false;
        for (int i = 0; i < m_tabs->count(); ++i) {
          const auto* candidate = m_tabs->widget(i);
          if (candidate != view && candidate->property("heritageFileResponse").toBool())
            responsePending = true;
        }
        if (!responsePending)
          scheduleDownloadRetry(QStringLiteral("작업 화면 연결 오류. 공식 화면에서 다시 받습니다."), true);
      }
      return;
    }
    if (view != m_mainView) return;
    if (info.status() != QWebEngineLoadingInfo::LoadFailedStatus || info.errorCode() == -3) return;
    fail(QStringLiteral("국가유산 사이트 화면을 불러오지 못했습니다. 오류 코드 %1 · %2")
             .arg(info.errorCode()).arg(info.url().path()));
  });
  connect(page, &QWebEnginePage::windowCloseRequested, this, [this, view, generation]() {
    if (generation != m_generation || m_tabs->indexOf(view) < 0) return;
    if (m_running && m_stage == HeritageStage::Download) return;
    const int at = m_tabs->indexOf(view);
    if (at >= 0) {
      m_tabs->removeTab(at);
      view->deleteLater();
    }
  });
  connect(view, &QWebEngineView::renderProcessTerminated, this,
          [this, view, generation](QWebEnginePage::RenderProcessTerminationStatus status, int code) {
            if (!m_running || generation != m_generation || m_tabs->indexOf(view) < 0) return;
            if (m_stage == HeritageStage::Download) {
              m_downloadPageNeedsRecovery = true;
              logLine(QStringLiteral("사이트 화면 종료: 상태 %1 · 코드 %2 · 진행 중 파일 전송은 보존합니다.")
                  .arg(static_cast<int>(status)).arg(code));
              if (m_pendingDownloads == 0 && m_currentFiles.isEmpty())
                scheduleDownloadRetry(QStringLiteral("파일 응답 화면이 종료되었습니다. 공식 화면에서 다시 받습니다."), true);
              return;
            }
            fail(QStringLiteral("인트라넷 연결이 끊겼습니다. 다시 눌러 주세요."));
          });
  return view;
}

QWebEnginePage* KaHeritageBrowser::activePage() const {
  if (!m_tabs) return nullptr;
  // 가장 최근에 뜬 인트라넷 페이지. 팝업이 떠 있으면 그쪽이 먼저다.
  for (int i = m_tabs->count() - 1; i >= 0; --i) {
    auto* view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
    if (!view) continue;
    if (view->url().host().endsWith(QStringLiteral("gis-heritage.go.kr"))) return view->page();
  }
  auto* current = qobject_cast<QWebEngineView*>(m_tabs->currentWidget());
  return current ? current->page() : nullptr;
}
