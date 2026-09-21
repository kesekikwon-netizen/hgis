#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QUrl>

class QLabel;
class QProgressBar;
class QTabWidget;
class QTreeWidget;
class QWebEngineProfile;
class QWebEnginePage;
class QWebEngineDownloadRequest;
class QLineEdit;
class QTimer;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QCloseEvent;

class KaTopographicBrowser : public QDialog {
  Q_OBJECT
public:
  explicit KaTopographicBrowser(QWidget* parent = nullptr, const QString& downloadDirectory = {});
  ~KaTopographicBrowser() override;
  void navigate(const QUrl& url);
  void prepareSheets(double x5179, double y5179, double radiusMeters, const QStringList& numbers,
                     const QStringList& cachedNumbers = {});
  void stopAutomatic();
  void setCompactMode(bool compact);
  void setDownloadRoot(const QString& directory);
  void setCredentials(const QString& username, const QString& password);
  void setActivityStatus(const QString& message);
  void setActivityAttention(const QString& message);
  void setProcessing(bool processing);
  void setPreparationProgress(const QString& phase, const QString& detail);
  void reject() override;
  // Restricted to loopback and QStandardPaths test mode; never changes production trust.
  void setPortalTestUrl(const QUrl& url);

signals:
  void cancelRequested();
  void fileDownloaded(const QString& path);
  void importFolderRequested();
  void selectionUpdated(const QJsonObject& publicMetadata);
  void automationNeedsInput(const QString& message);
  void sheetDownloaded(const QString& path, const QJsonObject& officialRecord);

protected:
  void showEvent(QShowEvent* event) override;

private:
  void cancelFromUser();
  void fitToAvailableScreen();
  void updateProgress();
  void setPhase(const QString& phase);
  QString visiblePhase() const;
  void failAutomation(const QString& message);
  void replaceSession();
  QWebEnginePage* addPage(QWebEnginePage* opener = nullptr);
  void requestDownload(QWebEngineDownloadRequest* request);
  void offerExternalProgram(const QUrl& url);
  void closeTab(int index);
  void pollAutomation();
  void automationMessage(const QString& message);
  void startAutomation(const QJsonObject& scope);
  bool trustedPage(const QUrl& url) const;
  bool transferPage(const QUrl& url) const;
  QWebEngineProfile* m_profile = nullptr;
  QTabWidget* m_tabs = nullptr;
  QTreeWidget* m_downloads = nullptr;
  QLabel* m_status = nullptr;
  QLabel* m_compactStatus = nullptr;
  QLabel* m_currentStage = nullptr;
  QLabel* m_transferSummary = nullptr;
  QLabel* m_stageTrail = nullptr;
  QProgressBar* m_progress = nullptr;
  QWidget* m_details = nullptr;
  QScrollArea* m_detailsScroll = nullptr;
  QWidget* m_compactActions = nullptr;
  QPushButton* m_expandButton = nullptr;
  QPushButton* m_compactCancel = nullptr;
  bool m_compactMode = false;
  bool m_processing = false;
  bool m_cancelNotified = false;
  QString m_activityAttention;
  QString m_automationAttention;
  QString m_failurePhase;
  QString m_activityFailurePhase;
  QString m_phase = QStringLiteral("도엽 확인");
  QString m_preparationPhase;
  QString m_preparationDetail;
  QElapsedTimer m_phaseClock;
  QElapsedTimer m_preparationClock;
  QElapsedTimer m_transferClock;
  QString m_currentFileName;
  qint64 m_receivedBytes = 0;
  qint64 m_totalBytes = -1;
  int m_sheetFileCount = 0;
  int m_receivedFileCount = 0;
  bool m_suppliedCredentials = false;
  QString m_downloadDirectory;
  bool m_explicitNavigation = false;
  bool m_destroying = false;
  QUrl m_portalUrl;
  QTimer* m_automationTimer = nullptr;
  QLineEdit* m_birthDate = nullptr;
  QLineEdit* m_purpose = nullptr;
  QLineEdit* m_loginId = nullptr;
  QLineEdit* m_loginPassword = nullptr;
  quint64 m_loginAttempt = 0;
  QString m_automationScript;
  QString m_automationStage;
  QString m_lastAutomationMessage;
  QByteArray m_lastDiagnostic;
  QString m_scopeKey;
  QStringList m_requestedNumbers;
  QJsonObject m_scope;
  QJsonObject m_pendingScope;
  QJsonObject m_currentRecord;
  QJsonArray m_officialRecords;
  QJsonArray m_transferManifest;
  QHash<QString, QString> m_sheetDownloadDirectories;
  QPointer<QWebEnginePage> m_transferPage;
  QPointer<QWebEnginePage> m_orderPage;
  QPointer<QWebEnginePage> m_orderDownloadPage;
  quint64 m_automationGeneration = 0;
  int m_sheetIndex = 0;
  int m_waitTicks = 0;
  int m_expectedSheetFiles = 0;
  QString m_sheetDownloadDirectory;
  QStringList m_completedSheetPaths;
  bool m_scriptInFlight = false;
};
