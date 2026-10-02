#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QNetworkCookie>
#include <QDialog>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/HeritageFetchPlan.h"
#include "core/HeritageFormParser.h"
#include "core/HeritageHttpClient.h"
#include "core/HeritageIntranetFlow.h"
#include "core/HeritageStyle.h"

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QProgressBar;
class QTimer;
class QWebEngineView;
class QTabWidget;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineDownloadRequest;
class QShowEvent;

// 국가유산 GIS통합인트라넷 자동화 창.
//
// 원칙 (수치지형도 창과 같다):
// - 공식 화면은 숨기고 단계·받은 파일·실패 사유만 보여 준다
// - **실패를 성공으로 처리하지 않는다.** 스크립트가 요소를 못 찾으면 거기서 멈추고,
//   그 화면에 무엇이 있었는지(pageOutline)를 함께 보여 준다. 다음 선택자는 그걸 보고 정한다
// - 서약서 동의는 자동으로 하되 **동의 시각과 서약서 원문을 영수증으로 남긴다**
// - **시/군까지만** 고른다
// - 비밀번호는 화면·로그·진단문에 절대 담지 않는다
class KaHeritageBrowser : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageBrowser(QWidget* parent = nullptr);
  ~KaHeritageBrowser() override;

  void setDownloadRoot(const QString& directory);
  // 받을 대상. 시/군 하나와 자료 종류들. 새로 받기를 시작할 때마다 부른다(이전 계획은 지운다).
  void setTarget(const QString& sido, const QString& city,
                 const QVector<HeritageDataset>& datasets);
  // setTarget 다음에 부른다. 확인창에서 고른 이웃 시·군(F120)을 첫 시·군이 끝난 뒤 하나씩
  // 차례로 받고, 최근 받은 자료 다시 쓰기(F174)를 적용한다. allFinished 는 마지막 시·군 뒤에 한 번.
  void setFetchPlan(const HeritageFetchPlan& plan);
  // 여러 시·군을 이어 받을 때만 지금 시·군 이름("예천군"). 한 시·군이면 빈 값.
  // HeritageImport::loadDataset 의 regionLabel 로 넘겨 레이어 이름을 구분한다.
  QString regionLabelForImport() const;
  // 아직 받기를 시작하지 않았을 때 창에 상태만 보여 준다(시·군 판정 중 등).
  void showWaiting(const QString& message);
  void start();
  void stop();
  void rejectDataset(const QString& message, bool retryableDownload = false);

  HeritageStage stage() const { return m_stage; }
  // 멈춘 자리에서 화면에 무엇이 있었는지. 선택자를 정하는 데 쓴다. 비밀번호는 들어가지 않는다.
  QString lastOutline() const { return m_lastOutline; }

protected:
  void showEvent(QShowEvent* event) override;

signals:
  void stageChanged(HeritageStage stage, const QString& message);
  // 한 종류를 다 받았다. 호출자가 HeritageImport 로 지도에 올린다.
  void datasetReady(HeritageDataset dataset, const QStringList& files);
  // 여섯 종을 모두 마쳤다.
  void allFinished();
  void failed(const QString& message);
  void fileDownloaded(const QString& path);
  // 서약서에 동의한 시각과 그때 화면에 있던 원문. 호출자가 receipts/ 에 남긴다.
  void agreementAccepted(const QDateTime& when, const QString& termsText);

private:
  friend class HeritageDownloadRetryTest;
  // 이 시·군의 자료를 모두 마쳤다. 다음 이웃 시·군이 있으면 이어 받고, 없으면 allFinished.
  void finishTarget(const QString& message);
  // 최근 받은 자료를 먼저 다시 쓴다. 이 시·군 자료가 모두 해결되면 true.
  bool reuseRecentDatasets();
  QString targetPrefix() const;
  void updateNotice();
  void scheduleDownloadRetry(const QString& reason, bool responseOnly = false);
  void reopenDownloadSession();
  void closeDownloadPopups(bool includeFormPages);
  void setStage(HeritageStage stage, const QString& message);
  void fail(const QString& message);
  // 사이트가 보낸 요청을 조사폴더 receipts/ 에 남긴다. 남긴 경로를 돌려준다.
  QString saveRequestLog();
  // 진단 기록. 단계·스크립트 결과·내려받기·실패를 그때그때 파일에 남긴다.
  // 화면을 주고받지 않고 이 파일만 보면 어디서 왜 막혔는지 알 수 있다.
  // **비밀번호는 절대 넣지 않는다** — 스크립트 본문은 기록하지 않고 결과만 남긴다.
  void logLine(const QString& text);
  QString m_logPath;
  void runStage();
  void readDatasetBaseline();
  void advanceDataset();
  bool m_datasetSelectionStarted = false;
  bool m_downloadPageRequested = false;
  bool m_downloadNavigation = false;
  bool m_agreementSubmitted = false;
  void runScript(const QString& script, const std::function<void(const QString&)>& then,
                 bool requireForm = false);
  // 다운로드 폼(codedeta) 프레임이 있으면 거기서 돌린다.
  // requireForm 이면 폼이 없을 때 메인(지도)에 검색을 넣지 않는다.
  void runOnPreferredDocument(const QString& script,
                              const std::function<void(const QString&)>& then,
                              bool manageFlight, bool requireForm);
  void captureOutline(const std::function<void()>& then);
  void handleDownload(QWebEngineDownloadRequest* request);
  HeritageStage nextStage(HeritageStage stage) const;
  // 팝업까지 포함해 새 창을 탭으로 받는다. 이것이 없으면 서약서·다운로드 팝업이 통째로 버려진다.
  void ensureProfile();
  QWebEngineView* addPage(QWebEnginePage* opener = nullptr);
  // 지금 스크립트를 돌릴 페이지. 인트라넷 페이지 중 가장 최근에 뜬 것.
  QWebEnginePage* activePage() const;

  QWebEngineProfile* m_profile = nullptr;
  bool m_profileWired = false;
  class HeritageRequestLog* m_requestLog = nullptr;  // 사이트가 보낸 요청 기록
  // 로그인만 브라우저로 하고, 검색·다운로드는 HTTP 로 직접 한다.
  // 화면을 흉내 내지 않으므로 프레임·ajax 변화에 흔들리지 않는다.
  HeritageHttpClient* m_http = nullptr;
  QList<QNetworkCookie> m_cookies;
  bool m_httpStarted = false;
  bool m_listRequested = false;
  bool m_listSent = false;
  bool m_agreementRecorded = false;
  // 브라우저가 받아 온 목록 HTML 에서 읽은 검색 폼.
  HeritageForm m_form;
  QTabWidget* m_tabs = nullptr;
  // 작업 화면(첫 탭). 내려받기용 탭은 준비 상태를 건드리지 않는다.
  QWebEngineView* m_mainView = nullptr;
  QLabel* m_stageLabel = nullptr;
  QLabel* m_detailLabel = nullptr;
  // 오래 남겨 둘 안내(서약 요지 F169, 계속 재시도 중 F174). 단계 글이 덮어쓰지 않는다.
  QLabel* m_noticeLabel = nullptr;
  QString m_pledgeNote;
  QString m_retryNote;
  QProgressBar* m_progress = nullptr;
  QPlainTextEdit* m_outline = nullptr;
  QPushButton* m_stopButton = nullptr;
  QTimer* m_poll = nullptr;

  HeritageStage m_stage = HeritageStage::Idle;
  QString m_sido;
  QString m_city;
  QVector<HeritageDataset> m_datasets;
  int m_datasetIndex = 0;
  // 여러 시·군 받기(F120). 요청 단위는 여전히 시·군 하나씩이다.
  QVector<HeritageDataset> m_planDatasets;
  QList<HeritageCity> m_followUps;
  int m_targetIndex = 0;
  int m_targetCount = 1;
  bool m_pendingNext = false;
  bool m_reuseRecent = false;
  bool m_reuseProbe = false;
  bool m_reuseRejected = false;
  QString m_downloadRoot;
  QString m_lastOutline;
  QString m_lastAlert;  // 사이트가 마지막으로 띄운 알림 글
  int m_waitTicks = 0;
  // 단계가 바뀐 뒤 화면이 자리잡을 때까지 쉬는 횟수(폴링 1회 = 0.7초).
  // 사이트가 ajax 로 폼을 다시 그리므로 바로 다음 동작을 하면 빈 값을 잡는다.
  int m_settle = 0;
  // 받는 중인 파일 수. 0이 되고 조금 더 조용하면 그 종류를 마친 것으로 본다.
  int m_pendingDownloads = 0;
  QElapsedTimer m_downloadWait;
  quint64 m_downloadRequestBaseline = 0;
  bool m_downloadRequestObserved = false;
  QList<QPointer<QWebEngineDownloadRequest>> m_downloadRequests;
  int m_idleTicks = 0;
  QElapsedTimer m_downloadRetryWait;
  QString m_downloadRetryReason;
  bool m_responseOnlyRetry = false;
  bool m_downloadPageNeedsRecovery = false;
  int m_downloadRetryDelayMs = 0;
  quint64 m_downloadRetryCount = 0;
  QStringList m_currentFiles;
  QString m_downloadMode;  // "all" = 결과 전체 한 번에, "selected" = 쪽마다
  int m_currentPage = 1;
  int m_lastPage = 1;
  int m_totalBeforeSearch = -1;  // 조건을 걸기 전 검색결과 건수(전국)
  bool m_running = false;
  bool m_scriptInFlight = false;
  bool m_pageReady = false;  // loadFinished 전에는 스크립트를 돌리지 않는다
  bool m_frameChecked = false;  // 이 화면에서 프레임 탈출을 이미 봤는가
  bool m_openedFrameSrc = false;  // iframe src 를 탭으로 연 적
  QString m_lastCppProbe;         // 프레임 url/name/hint. 비밀번호 없음
  quint64 m_generation = 0;
};
