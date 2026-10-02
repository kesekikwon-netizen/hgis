#include "KaCadImportNotice.h"

#include "KaUserError.h"
#include "core/CadDrawingLayers.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>
#include <QToolButton>

#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace KaCadImport {
namespace {

QString noticeName(const QString& title) { return QStringLiteral("cadNotice:") + title; }

}  // namespace

void dropNotices(QgsMessageBar* bar, const QString& title) {
  if (!bar) return;
  for (QgsMessageBarItem* item : bar->items())
    if (item->objectName() == noticeName(title)) bar->popWidget(item);
}

void showNotice(const Hooks& hooks, const QString& title, const QString& drawingId, const QString& sourcePath,
                const CadCrsResult& guess, const QString& usedAuthId, const QString& workAuthId, bool likely) {
  if (!hooks.messageBar) return;
  auto* widget = new QWidget();
  auto* row = new QHBoxLayout(widget);
  row->setContentsMargins(0, 0, 0, 0);
  auto* other = new QToolButton(widget);
  other->setObjectName(QStringLiteral("cadOtherCrs"));
  other->setText(QStringLiteral("다른 위치로 바꾸기"));
  other->setPopupMode(QToolButton::InstantPopup);
  auto* menu = new QMenu(other);
  const auto reimport = [hooks, sourcePath](const QString& authId) {
    if (!QFileInfo::exists(sourcePath)) {
      KaUserError::warn(hooks.window, {QStringLiteral("도면 불러오기"), QStringLiteral("원본 도면 파일을 찾지 못했습니다."),
                                       QDir::toNativeSeparators(sourcePath),
                                       QStringLiteral("원본 파일을 원래 자리에 두고 다시 고르세요.")});
      return;
    }
    if (hooks.reimport) hooks.reimport(sourcePath, authId);
  };
  for (const CadCrsCandidate& candidate : guess.candidates)
    if (candidate.authId != usedAuthId)
      menu->addAction(CadCrsGuess::describe(candidate), menu, [reimport, authId = candidate.authId] { reimport(authId); });
  menu->addSeparator();
  menu->addAction(QStringLiteral("좌표 없는 도면으로 보기"), menu, [reimport] { reimport(QString::fromLatin1(kNoCrs)); });
  other->setMenu(menu);
  auto* alignNow = new QPushButton(QStringLiteral("직접 맞추기"), widget);
  alignNow->setObjectName(QStringLiteral("cadAlignNow"));
  QObject::connect(alignNow, &QPushButton::clicked, widget, [hooks, drawingId] {
    QgsVectorLayer* layer = CadDrawingLayers::alignLayerOf(QgsProject::instance(), drawingId);
    if (layer && hooks.startAlign) hooks.startAlign(layer);
    if (!layer && hooks.window)  // 사용자가 도면 묶음을 지운 뒤
      hooks.window->statusBar()->showMessage(QStringLiteral("이 도면이 지도에 없습니다. 도면을 다시 불러와 주세요."), 10000);
  });
  row->addWidget(other);
  row->addWidget(alignNow);
  QString place = QStringLiteral("%1(%2)").arg(CadCrsGuess::label(usedAuthId), usedAuthId);
  for (const CadCrsCandidate& candidate : guess.candidates)
    if (candidate.authId == usedAuthId) place = candidate.region + QStringLiteral(" 부근 · ") + place;
  const QString text = likely ? QStringLiteral("위치 단서가 없어 가장 그럴듯한 자리(%1)에 올렸습니다. "
                                               "다른 곳이면 「다른 위치로 바꾸기」를 누르세요.").arg(place)
                              : QStringLiteral("도면을 %1로 읽어 %2로 바꿔 올렸습니다.").arg(place, workAuthId);
  auto* item = new QgsMessageBarItem(QStringLiteral("도면"), text, widget,
                                     likely ? Qgis::MessageLevel::Warning : Qgis::MessageLevel::Info, 0);
  item->setObjectName(noticeName(title));
  hooks.messageBar->pushItem(item);
}

}  // namespace KaCadImport
