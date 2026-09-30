#include "KaFileBrowserPanel.h"

#include <QDir>
#include <QDrag>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QListWidgetItem>
#include <QMimeData>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

KaFileListView::KaFileListView(QWidget* parent) : QListWidget(parent) {
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setDefaultDropAction(Qt::CopyAction);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setUniformItemSizes(true);
  setIconSize(QSize(0, 0));
}

void KaFileListView::startDrag(Qt::DropActions) {
  QList<QUrl> urls;
  const auto items = selectedItems();
  for (QListWidgetItem* it : items) {
    if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
    const QString p = it->data(Qt::UserRole).toString();
    if (!p.isEmpty()) urls.append(QUrl::fromLocalFile(p));
  }
  if (urls.isEmpty()) return;
  auto* md = new QMimeData;
  md->setUrls(urls);
  QDrag drag(this);
  drag.setMimeData(md);
  drag.exec(Qt::CopyAction);
}

QString KaFileBrowserPanel::resolvedDesktopPath() {
  static QString cached;
  if (!cached.isEmpty() && QFileInfo(cached).isDir())
    return cached;
  const QStringList candidates = {
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
      QDir::homePath() + QStringLiteral("/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/바탕 화면"),
  };
  for (const QString& c : candidates) {
    if (c.isEmpty()) continue;
    const QFileInfo fi(c);
    if (fi.exists() && fi.isDir()) {
      cached = QDir::cleanPath(fi.absoluteFilePath());
      return cached;
    }
  }
  cached = QDir::homePath();
  return cached;
}

QString KaFileBrowserPanel::findRemovableSdPath() {
#ifdef Q_OS_WIN
  for (wchar_t letter = L'D'; letter <= L'Z'; ++letter) {
    wchar_t root[] = {letter, L':', L'\\', L'\0'};
    if (GetDriveTypeW(root) == DRIVE_REMOVABLE)
      return QString::fromWCharArray(root);
  }
#endif
  const auto vols = QStorageInfo::mountedVolumes();
  for (const QStorageInfo& vol : vols) {
    if (!vol.isValid() || !vol.isReady()) continue;
    const QString root = QDir::fromNativeSeparators(vol.rootPath());
    if (root.compare(QStringLiteral("C:/"), Qt::CaseInsensitive) == 0) continue;
    const QString name = vol.name();
    if (name.contains(QStringLiteral("SD"), Qt::CaseInsensitive) ||
        name.contains(QStringLiteral("카드"), Qt::CaseInsensitive))
      return vol.rootPath();
  }
  return {};
}

KaFileBrowserPanel::KaFileBrowserPanel(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("filesCard"));
  auto* filesLay = new QVBoxLayout(this);
  filesLay->setContentsMargins(6, 6, 6, 6);
  filesLay->setSpacing(6);

  auto* capFiles = new QLabel(QStringLiteral("파일함"), this);
  capFiles->setObjectName(QStringLiteral("cardCaption"));
  capFiles->setProperty("class", QStringLiteral("cardCaptionFiles"));

  auto makeBtn = [this](const QString& text, const QString& objName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setText(text);
    b->setObjectName(objName);
    b->setToolTip(tip);
    return b;
  };

  m_btnPc = makeBtn(QStringLiteral("내 PC"), QStringLiteral("btnBrowsePc"),
                    QStringLiteral("컴퓨터 드라이브 목록으로 이동합니다"));
  connect(m_btnPc, &QToolButton::clicked, this, [this]() { goTo(QString()); });

  m_btnDesk = makeBtn(QStringLiteral("바탕화면"), QStringLiteral("btnBrowseDesktop"),
                      QStringLiteral("바탕화면 폴더로 이동합니다"));
  connect(m_btnDesk, &QToolButton::clicked, this, [this]() { goTo(resolvedDesktopPath()); });

  m_btnDocs = makeBtn(QStringLiteral("문서"), QStringLiteral("btnBrowseDocs"),
                      QStringLiteral("문서 폴더로 이동합니다"));
  connect(m_btnDocs, &QToolButton::clicked, this, [this]() {
    goTo(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  });

  m_btnDown = makeBtn(QStringLiteral("내려받기"), QStringLiteral("btnBrowseDownloads"),
                      QStringLiteral("내려받기 폴더로 이동합니다"));
  connect(m_btnDown, &QToolButton::clicked, this, [this]() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (d.isEmpty() || !QFileInfo(d).isDir())
      d = QDir::homePath() + QStringLiteral("/Downloads");
    goTo(d);
  });

  m_btnPick = makeBtn(QStringLiteral("폴더 지정"), QStringLiteral("btnBrowseFolder"),
                      QStringLiteral("조사 자료 폴더를 지정하여 엽니다"));
  connect(m_btnPick, &QToolButton::clicked, this, &KaFileBrowserPanel::pickFolder);

  m_btnSd = makeBtn(QStringLiteral("SD 카드"), QStringLiteral("btnBrowseSd"),
                    QStringLiteral("SD 카드·이동식 디스크를 찾아 엽니다"));
  connect(m_btnSd, &QToolButton::clicked, this, [this]() {
    const QString sd = findRemovableSdPath();
    if (sd.isEmpty())
      emit statusMessage(QStringLiteral("연결된 SD카드를 찾지 못했습니다."));
    else
      goTo(sd);
  });

  m_btnUp = makeBtn(QStringLiteral("상위"), QStringLiteral("btnBrowseUp"),
                    QStringLiteral("상위 폴더로 한 단계 이동합니다"));
  // Its accent-tinted face lives in ka-hgis.qss (QFrame#filesCard QToolButton#btnBrowseUp).
  connect(m_btnUp, &QToolButton::clicked, this, &KaFileBrowserPanel::goUp);

  m_pathBar1 = new QHBoxLayout();
  m_pathBar1->setContentsMargins(0, 0, 0, 0);
  m_pathBar1->setSpacing(3);

  m_pathBar2 = new QHBoxLayout();
  m_pathBar2->setContentsMargins(0, 0, 0, 0);
  m_pathBar2->setSpacing(3);

  updateButtonLayout(width() > 0 ? width() : 340);

  m_list = new KaFileListView(this);
  m_list->setObjectName(QStringLiteral("fileBrowser"));
  m_list->setToolTip(QStringLiteral("파일을 지도에 끌어 넣으면 레이어가 됩니다."));
  connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (item->data(Qt::UserRole + 1).toBool())
      goTo(path);
    else
      emit fileActivated(path);
  });

  auto* filesInner = new QFrame(this);
  filesInner->setObjectName(QStringLiteral("filesInner"));
  auto* filesInnerLay = new QVBoxLayout(filesInner);
  filesInnerLay->setContentsMargins(4, 4, 4, 4);
  filesInnerLay->addWidget(m_list, 1);

  filesLay->addWidget(capFiles);
  filesLay->addLayout(m_pathBar1);
  filesLay->addLayout(m_pathBar2);
  filesLay->addWidget(filesInner, 1);
}

void KaFileBrowserPanel::resizeEvent(QResizeEvent* event) {
  QFrame::resizeEvent(event);
  updateButtonLayout(width());
}

void KaFileBrowserPanel::showEvent(QShowEvent* event) {
  QFrame::showEvent(event);
  updateButtonLayout(width());
}

void KaFileBrowserPanel::fitShortcutFonts(int width) {
  auto fitRow = [](QHBoxLayout* lay, int rowWidth) {
    if (!lay || rowWidth <= 0) return;
    QList<QToolButton*> vis;
    for (int i = 0; i < lay->count(); ++i) {
      QLayoutItem* it = lay->itemAt(i);
      if (!it) continue;
      auto* b = qobject_cast<QToolButton*>(it->widget());
      if (b && !b->isHidden()) vis.append(b);
    }
    if (vis.isEmpty()) return;
    const int gaps = lay->spacing() * qMax(0, static_cast<int>(vis.size()) - 1);
    const int each = qMax(16, (rowWidth - gaps) / vis.size());
    for (QToolButton* b : vis) {
      int px = 12;
      for (; px >= 7; --px) {
        QFont f = b->font();
        f.setPixelSize(px);
        if (QFontMetrics(f).horizontalAdvance(b->text()) + 14 <= each)
          break;
      }
      QFont f = b->font();
      f.setPixelSize(px);
      b->setFont(f);
      b->setMinimumWidth(0);
      b->setMaximumWidth(each);
      // Only the fitted size is local; colours come from the application sheet.
      b->setStyleSheet(QStringLiteral("font-size: %1px;").arg(px));
    }
  };
  const int inner = qMax(40, width - 16);
  fitRow(m_pathBar1, inner);
  if (m_isTwoRows)
    fitRow(m_pathBar2, inner);
}

void KaFileBrowserPanel::updateButtonLayout(int width) {
  if (!m_pathBar1 || !m_pathBar2 || !m_btnPc) return;
  const bool wantTwoRows = (width > 0 && width < 420);
  if (wantTwoRows == m_isTwoRows && m_pathBar1->count() > 0) {
    fitShortcutFonts(width);
    return;
  }
  m_isTwoRows = wantTwoRows;

  // Clear existing items from layouts
  auto clearLayout = [](QHBoxLayout* lay) {
    while (QLayoutItem* item = lay->takeAt(0)) {
      delete item; // deletes spacers
    }
  };
  clearLayout(m_pathBar1);
  clearLayout(m_pathBar2);

  if (!m_isTwoRows) {
    // 1 row: [내 PC] [바탕화면] [문서] [내려받기] [폴더 지정] [상위]
    m_pathBar1->addWidget(m_btnPc);
    m_pathBar1->addWidget(m_btnDesk);
    m_pathBar1->addWidget(m_btnDocs);
    m_pathBar1->addWidget(m_btnDown);
    m_pathBar1->addWidget(m_btnSd);
    m_pathBar1->addWidget(m_btnPick);
    m_pathBar1->addWidget(m_btnUp);
    m_pathBar1->addStretch(1);
    m_btnDown->setVisible(true);
    m_btnSd->setVisible(true);
    m_btnPick->setVisible(true);
    m_btnUp->setVisible(true);
  } else {
    // 2 rows:
    // Row 1: [내PC] [바탕화면] [문서]
    m_pathBar1->addWidget(m_btnPc);
    m_pathBar1->addWidget(m_btnDesk);
    m_pathBar1->addWidget(m_btnDocs);
    m_pathBar1->addStretch(1);
    // Row 2: [내려받기] [SD 카드] [폴더 지정] [상위]
    m_pathBar2->addWidget(m_btnDown);
    m_pathBar2->addWidget(m_btnSd);
    m_pathBar2->addWidget(m_btnPick);
    m_pathBar2->addWidget(m_btnUp);
    m_pathBar2->addStretch(1);
  }
  fitShortcutFonts(width);
}

QStringList KaFileBrowserPanel::selectedFiles() const {
  QStringList out;
  if (!m_list) return out;
  const auto items = m_list->selectedItems();
  for (QListWidgetItem* it : items) {
    if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
    const QString p = it->data(Qt::UserRole).toString();
    if (!p.isEmpty()) out.append(p);
  }
  return out;
}

void KaFileBrowserPanel::goUp() {
  if (m_path.isEmpty()) {
    goTo(QString());
    return;
  }
  const QDir d(m_path);
  const QString parent = QDir::cleanPath(d.absolutePath() + QStringLiteral("/.."));
  if (parent == QDir::cleanPath(m_path) || parent.length() < 3)
    goTo(QString());
  else
    goTo(parent);
}

void KaFileBrowserPanel::pickFolder() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("조사 데이터 폴더 선택"),
      m_path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                       : m_path);
  if (!dir.isEmpty())
    goTo(dir);
}

void KaFileBrowserPanel::goTo(const QString& path) {
  if (!m_list) return;
  m_list->clear();

  QString p = QDir::fromNativeSeparators(path.trimmed());
  if (p.length() == 2 && p[1] == QLatin1Char(':'))
    p += QLatin1Char('/');

  auto addRow = [this](const QString& label, const QString& full, bool isDir) {
    auto* it = new QListWidgetItem(label);
    it->setData(Qt::UserRole, full);
    it->setData(Qt::UserRole + 1, isDir);
    it->setToolTip(QDir::toNativeSeparators(full));
    m_list->addItem(it);
  };

  if (p.isEmpty()) {
    m_path.clear();
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo& d : drives)
      addRow(QDir::toNativeSeparators(d.absoluteFilePath()),
             QDir::fromNativeSeparators(d.absoluteFilePath()), true);
    emit statusMessage(QStringLiteral("드라이브 목록 — 폴더를 더블클릭하세요"));
    return;
  }

  p = QDir::cleanPath(p);
  const QFileInfo fi(p);
  if (!fi.exists() || !fi.isDir()) {
    emit statusMessage(QStringLiteral("폴더 없음 → 드라이브 목록"));
    goTo(QString());
    return;
  }
  m_path = QDir::cleanPath(fi.absoluteFilePath());

  QDir dir(m_path);
  dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  dir.setSorting(QDir::Name | QDir::IgnoreCase);
  const QStringList folders = dir.entryList();
  int n = 0;
  for (const QString& name : folders) {
    if (n >= 250) break;
    if (name.compare(QLatin1String("$Recycle.Bin"), Qt::CaseInsensitive) == 0 ||
        name.compare(QLatin1String("System Volume Information"), Qt::CaseInsensitive) == 0)
      continue;
    addRow(QStringLiteral("[폴더] ") + name, dir.absoluteFilePath(name), true);
    ++n;
  }
  dir.setFilter(QDir::Files | QDir::NoSymLinks);
  dir.setNameFilters({QStringLiteral("*.shp"), QStringLiteral("*.dxf"), QStringLiteral("*.dwg"),
                      QStringLiteral("*.gpkg"), QStringLiteral("*.geojson"), QStringLiteral("*.json"),
                      QStringLiteral("*.tif"), QStringLiteral("*.tiff"), QStringLiteral("*.gtiff"),
                      QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")});
  const QStringList files = dir.entryList();
  for (const QString& name : files) {
    if (n >= 400) break;
    addRow(name, dir.absoluteFilePath(name), false);
    ++n;
  }
  emit statusMessage(QStringLiteral("경로: %1").arg(QDir::toNativeSeparators(m_path)));
}
