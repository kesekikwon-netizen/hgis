// Drawing studio: stored sheets (도면 목록), named layer sets and the optional
// title block. Split from KaDrawingStudio.cpp to keep that file smaller.
#include "KaDrawingStudio.h"

#include "KaDrawingLayerSets.h"
#include "KaDrawingSheetSet.h"
#include "KaTitleBlock.h"

#include <QDate>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>

#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutmeasurementconverter.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutrendercontext.h>
#include <qgslayoutsize.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>

namespace {
// Working sheet: the one the studio edits and the submission package exports.
const QString kWorkingSheet = QStringLiteral("user_sheet");
const QString kMapId = QStringLiteral("ka_map");
const QString kLegendId = QStringLiteral("ka_legend");
}  // namespace

void KaDrawingStudio::placeTitleBlock() {
  auto* ly = layout();
  auto* map = mapItem();
  if (!ly || !map) {
    showStatus(QStringLiteral("먼저 지도 칸을 그리세요."));
    return;
  }
  endActivateMap();
  QString title = KaTitleBlock::drawingTitleOf(ly);
  if (title.isEmpty()) title = QStringLiteral("조사도면");
  bool ok = false;
  title = QInputDialog::getText(this, QStringLiteral("표제란"),
                                QStringLiteral("도면명 (예: 조사구역도, 유적위치도, 유구배치도)"),
                                QLineEdit::Normal, title, &ok)
              .trimmed();
  if (!ok) return;
  const KaTitleBlock::Info info = KaTitleBlock::collect(m_project);
  const bool existed = ly->itemById(KaTitleBlock::itemId()) != nullptr;
  const QRectF page(0.0, 0.0, m_paperW, m_paperH);
  const QRectF mapRect(map->pos(), map->rect().size());
  KaTitleBlock::place(ly, KaTitleBlock::defaultRect(page, mapRect), title,
                      KaTitleBlock::text(title, info, QDate::currentDate(), kMapId));
  if (!existed) m_placeUndo.append(KaTitleBlock::itemId());
  m_placeKind = PlaceKind::TitleBlock;
  markUserComposed();
  finishPlace();
  showStatus(info.surveyName.isEmpty() && info.siteName.isEmpty()
                 ? QStringLiteral("표제란을 넣었습니다. 조사구역에 조사명·유적명이 없어 ―로 두었습니다.")
                 : QStringLiteral("표제란을 넣었습니다. 끌어 옮기세요. 필요 없으면 Delete로 지웁니다."));
}

void KaDrawingStudio::storeCurrentSheet() {
  if (!m_project || !layout()) {
    showStatus(QStringLiteral("보관할 용지가 없습니다."));
    return;
  }
  endActivateMap();
  QString suggestion = KaTitleBlock::drawingTitleOf(layout());
  if (suggestion.isEmpty())
    suggestion = QStringLiteral("도면 %1").arg(KaDrawingSheetSet::list(m_project).size() + 1);
  bool ok = false;
  const QString title =
      QInputDialog::getText(this, QStringLiteral("도면 보관"),
                            QStringLiteral("도면 이름 (예: 조사구역도 1:1000, 1호 주거지 실측도)"),
                            QLineEdit::Normal, suggestion, &ok)
          .trimmed();
  if (!ok || title.isEmpty()) return;
  if (KaDrawingSheetSet::contains(m_project, title) &&
      QMessageBox::question(this, QStringLiteral("도면 보관"),
                            QStringLiteral("「%1」 도면이 이미 있습니다. 지금 도면으로 바꿀까요?").arg(title),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;
  // The stored copy must match what is on screen.
  if (auto* map = mapItem(); map && m_layerSyncPending) {
    m_layerSyncPending = false;
    applyLayersToMap(map, true, false);
  }
  QString error;
  if (!KaDrawingSheetSet::store(m_project, kWorkingSheet, title, &error)) {
    QMessageBox::warning(this, QStringLiteral("도면 보관"), error);
    return;
  }
  // The sheet's layer combination is kept as a layer set of the same name.
  if (m_layerModel) KaDrawingLayerSets::save(m_project, title, m_layerModel);
  showStatus(QStringLiteral("도면 「%1」을 보관했습니다. 「도면 목록」에서 다시 엽니다.").arg(title));
}

void KaDrawingStudio::restoreStoredSheet(const QString& title) {
  if (!m_project || !KaDrawingSheetSet::contains(m_project, title)) return;
  if (QMessageBox::question(
          this, QStringLiteral("도면 열기"),
          QStringLiteral("지금 용지를 보관한 도면 「%1」로 바꿉니다. 지금 도면을 보관하지 않았다면 사라집니다. "
                         "레이어 켜짐도 그 도면의 세트로 바뀝니다. 바꿀까요?")
              .arg(title),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;
  endActivateMap();
  if (m_layerSyncTimer) m_layerSyncTimer->stop();
  if (m_scaleSyncTimer) m_scaleSyncTimer->stop();
  m_layerSyncPending = false;
  detachLayoutFromView();
  QString error;
  QgsPrintLayout* restored = KaDrawingSheetSet::restore(m_project, title, kWorkingSheet, &error);
  if (!restored) {
    attachLayoutToView();
    QMessageBox::warning(this, QStringLiteral("도면 열기"), error);
    return;
  }
  if (restored->pageCollection() && restored->pageCollection()->page(0)) {
    const QgsLayoutSize size = restored->renderContext().measurementConverter().convert(
        restored->pageCollection()->page(0)->pageSize(), Qgis::LayoutUnit::Millimeters);
    if (size.width() >= 20.0 && size.height() >= 20.0) {
      m_paperW = size.width();
      m_paperH = size.height();
    }
  }
  // Callout ground points and overlay caches belonged to the replaced sheet.
  m_coordMapPts.clear();
  m_coordFrameMap.clear();
  m_placeUndo.clear();
  m_aboveMapDigest.clear();
  m_legendRemoved = restored->itemById(kLegendId) == nullptr;
  attachLayoutToView();
  connectMapSignals(mapItem());
  relinkDecorations();
  // Reopening a sheet is an explicit choice, so its layer set is applied too.
  const bool layersApplied = m_layerModel && KaDrawingLayerSets::contains(m_project, title) &&
                             KaDrawingLayerSets::apply(m_project, title, m_layerModel);
  m_paperFitPending = true;
  zoomPaperVisible();
  syncMapFromLayers();
  syncHeritageNumbers(true);
  refreshScaleWidgets(true);
  showStatus(layersApplied
                 ? QStringLiteral("도면 「%1」을 열고 그때의 레이어 켜짐을 적용했습니다.").arg(title)
                 : QStringLiteral("도면 「%1」을 열었습니다.").arg(title));
}

void KaDrawingStudio::removeStoredSheet(const QString& title) {
  if (!m_project || !KaDrawingSheetSet::contains(m_project, title)) return;
  if (QMessageBox::question(this, QStringLiteral("도면 목록"),
                            QStringLiteral("보관한 도면 「%1」과 같은 이름의 레이어 세트를 지울까요? "
                                           "지금 열린 도면과 레이어는 그대로입니다.")
                                .arg(title),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;
  if (!KaDrawingSheetSet::remove(m_project, title)) return;
  // The layer set stored with the sheet goes with it.
  KaDrawingLayerSets::remove(m_project, title);
  showStatus(QStringLiteral("보관한 도면 「%1」을 지웠습니다.").arg(title));
}
