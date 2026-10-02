#include "KaBasemapQuickCard.h"

#include "KaIcons.h"
#include "KaTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
struct Button {
  const char* id;
  const char* objectName;
  const char* text;
  const char* icon;
};
constexpr Button kButtons[] = {
    {"satellite", "basemapQuickSatellite", "위성", "satellite"},
    {"terrain", "basemapQuickTerrain", "지형", "contour"},
    {"old_map", "basemapQuickOldMap", "옛 지도", "old_map"},
};
}  // namespace

KaBasemapQuickCard::KaBasemapQuickCard(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("basemapQuickCard"));
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(12, 12, 12, 12);
  column->setSpacing(8);
  auto* head = new QHBoxLayout;
  head->setSpacing(8);
  auto* glyph = new QLabel(this);
  glyph->setPixmap(KaIcons::glyphPixmap(QStringLiteral("layer"), KaTheme::tokens().ink, 16, devicePixelRatioF()));
  glyph->setFixedSize(16, 16);
  auto* title = new QLabel(QStringLiteral("배경 지도"), this);
  title->setObjectName(QStringLiteral("basemapQuickTitle"));
  QFont font = title->font();
  font.setPixelSize(14);
  font.setBold(true);
  title->setFont(font);
  head->addWidget(glyph, 0, Qt::AlignVCenter);
  head->addWidget(title, 1);
  column->addLayout(head);
  // One short sentence: at 12 px in the 272 px pane a longer note wrapped to a third line that
  // the footer's size hint (measured at a wider width) did not reserve, so it was cut off.
  m_note = new QLabel(QStringLiteral("조사를 열면 위성과 지적이 올라옵니다."), this);
  m_note->setObjectName(QStringLiteral("basemapQuickNote"));
  m_note->setWordWrap(true);
  m_note->setToolTip(QStringLiteral("아래 단추는 리본의 「배경 지도」와 같은 단추입니다."));
  column->addWidget(m_note);
  auto* row = new QHBoxLayout;
  row->setSpacing(8);
  for (const Button& spec : kButtons) {
    const QString id = QString::fromLatin1(spec.id);
    auto* button = new QToolButton(this);
    button->setObjectName(QString::fromLatin1(spec.objectName));
    button->setText(QString::fromUtf8(spec.text));
    button->setIcon(KaIcons::icon(QString::fromLatin1(spec.icon)));
    button->setIconSize(QSize(16, 16));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCheckable(true);
    button->setMinimumSize(72, 34);
    button->setFocusPolicy(Qt::StrongFocus);
    connect(button, &QToolButton::clicked, this, [this, id, button](bool nowChecked) {
      {
        const QSignalBlocker quiet(button);
        button->setChecked(!nowChecked);  // the window answers with setChecked()
      }
      emit basemapRequested(id);
    });
    row->addWidget(button, 1);
    m_buttons.insert(id, button);
  }
  column->addLayout(row);
}

QStringList KaBasemapQuickCard::ids() {
  QStringList list;
  for (const Button& spec : kButtons) list << QString::fromLatin1(spec.id);
  return list;
}

QToolButton* KaBasemapQuickCard::button(const QString& id) const { return m_buttons.value(id, nullptr); }

bool KaBasemapQuickCard::isChecked(const QString& id) const {
  const QToolButton* button = m_buttons.value(id, nullptr);
  return button && button->isChecked();
}

QString KaBasemapQuickCard::sentence() const { return m_note->text(); }

void KaBasemapQuickCard::setChecked(const QString& id, bool on) {
  QToolButton* button = m_buttons.value(id, nullptr);
  if (!button || button->isChecked() == on) return;
  const QSignalBlocker quiet(button);
  button->setChecked(on);
}
