#include "HeritagePledge.h"

#include "HeritageLayoutNumbers.h"
#include "HeritageStyle.h"

#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>

#include <QRegularExpression>

namespace {

// Words of the pledge sentences that decide what may leave the office. Matching only
// selects sentences to quote; it never paraphrases them.
const char* const kUseWords[] = {"목적", "외부", "반출", "제3자", "제공", "배포", "공개",
                                 "복제", "유출", "게재", "인쇄", "도면", "보고서", "상업"};

}  // namespace

QString HeritagePledge::preDisclosure() {
  return QStringLiteral(
      "받기를 누르면 국가유산 인트라넷의 「원본자료 사용 서약서」에 자동으로 동의하고, "
      "동의 시각과 서약서 원문을 조사폴더 주변유적/receipts 에 남깁니다. "
      "받은 자료는 동국문화재연구원 소유입니다. 도면·보고서에 넣거나 밖으로 보내기 전에 "
      "서약 범위를 확인하세요.");
}

QStringList HeritagePledge::highlights(const QString& termsText, int maxLines, int maxChars) {
  QStringList out;
  if (termsText.trimmed().isEmpty() || maxLines <= 0) return out;
  static const QRegularExpression split(QStringLiteral("(?<=[.!?。])\\s+|[\\r\\n]+"));
  const QStringList sentences = termsText.split(split, Qt::SkipEmptyParts);
  for (const QString& raw : sentences) {
    const QString sentence = raw.simplified();
    if (sentence.size() < 6) continue;
    bool hit = false;
    for (const char* word : kUseWords)
      if (sentence.contains(QString::fromUtf8(word))) { hit = true; break; }
    if (!hit || out.contains(sentence)) continue;
    out.append(sentence.size() > maxChars ? sentence.left(maxChars - 1) + QChar(0x2026) : sentence);
    if (out.size() >= maxLines) break;
  }
  return out;
}

QString HeritagePledge::noticeAfterAgreement(const QString& termsText) {
  const QStringList lines = highlights(termsText);
  QString text = QStringLiteral("서약서에 자동 동의했습니다. 원문은 조사폴더 주변유적/receipts 에 남습니다.");
  if (lines.isEmpty())
    return text + QStringLiteral(" 이용 범위 문장을 찾지 못했으니 원문을 확인하세요.");
  return text + QStringLiteral("\n서약 요지(원문 그대로): ") + lines.join(QStringLiteral(" / "));
}

void HeritagePledge::markIntranetLayer(QgsMapLayer* layer, HeritageDataset dataset,
                                       const QString& regionLabel) {
  if (!layer) return;
  // Same tag, same logical key as HeritageStyle::apply: never a display name, so
  // HeritageLayoutNumbers::taggedDataset keeps working after this call.
  HeritageLayoutNumbers::tagDataset(layer, dataset);
  if (!regionLabel.isEmpty())
    layer->setCustomProperty(QStringLiteral("ka_hgis/heritage_region"), regionLabel);
}

bool HeritagePledge::isIntranetLayer(const QgsMapLayer* layer) {
  return HeritageLayoutNumbers::taggedDataset(layer).has_value();
}

bool HeritagePledge::projectHasIntranetLayers(const QgsProject* project) {
  if (!project) return false;
  const auto layers = project->mapLayers();
  for (QgsMapLayer* layer : layers)
    if (isIntranetLayer(layer)) return true;
  // Projects saved before the tag existed: a layer under a heritage kind group.
  const QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;
  for (QgsLayerTreeLayer* node : root->findLayers()) {
    for (QgsLayerTreeNode* up = node->parent(); up && up != root; up = up->parent()) {
      if (HeritageStyle::fromLayerName(up->name())) return true;
    }
  }
  return false;
}

QString HeritagePledge::checklistStateKey() { return QStringLiteral("HERITAGE_PLEDGE_SCOPE"); }

bool HeritagePledge::checklistPasses(const QgsProject* project) {
  return !projectHasIntranetLayers(project);
}
