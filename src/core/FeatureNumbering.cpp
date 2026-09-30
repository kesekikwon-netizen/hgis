#include "FeatureNumbering.h"

#include "FeaturePresets.h"

#include <QHash>
#include <QRegularExpression>

#include <qgis.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>

namespace FeatureNumbering {
namespace {
QString cellText(const QgsFeature& f, int index) {
  if (index < 0) return {};
  const QVariant v = f.attribute(index);
  return v.isNull() ? QString() : v.toString().trimmed();
}

QString groupKey(const QString& kind) { return FeaturePresets::kindKey(kind); }

// Calls visit(feature, kindText, numberText) for every feature, reading only the two fields.
template <typename Visit>
void scan(const QgsVectorLayer* layer, const Fields& fields, Visit visit) {
  QgsFeatureRequest request;
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  request.setSubsetOfAttributes(fields.kind >= 0 ? QgsAttributeList{fields.number, fields.kind}
                                                 : QgsAttributeList{fields.number});
  QgsFeatureIterator it = layer->getFeatures(request);
  QgsFeature f;
  while (it.nextFeature(f)) visit(f, cellText(f, fields.kind), cellText(f, fields.number));
}
}  // namespace

QString normalized(const QString& number) {
  static const QRegularExpression space(QStringLiteral("\\s+"));
  QString t = number;
  t.remove(space);
  if (t.size() > 1 && t.startsWith(QStringLiteral("제")) && t.at(1).isDigit()) t.remove(0, 1);
  if (t.endsWith(QStringLiteral("호"))) t.chop(1);
  QString out;
  for (qsizetype i = 0; i < t.size();) {
    if (!t.at(i).isDigit()) {
      out += t.at(i).toUpper();
      ++i;
      continue;
    }
    QString run;
    for (; i < t.size() && t.at(i).isDigit(); ++i) run += QChar(u'0' + t.at(i).digitValue());
    while (run.size() > 1 && run.startsWith(QLatin1Char('0'))) run.remove(0, 1);
    out += run;
  }
  return out;
}

Parts parse(const QString& number) {
  Parts parts;
  const QString t = number.trimmed();
  qsizetype end = t.size();
  while (end > 0 && !t.at(end - 1).isDigit()) --end;
  if (end == 0) {
    parts.prefix = t;
    return parts;
  }
  qsizetype start = end;
  while (start > 0 && t.at(start - 1).isDigit()) --start;
  QString digits;
  for (qsizetype i = start; i < end; ++i) digits += QChar(u'0' + t.at(i).digitValue());
  bool ok = false;
  const qint64 value = digits.toLongLong(&ok);
  if (!ok) {
    parts.prefix = t;
    return parts;
  }
  parts.prefix = t.left(start);
  parts.value = value;
  parts.width = static_cast<int>(digits.size());
  parts.suffix = t.mid(end);
  return parts;
}

QString defaultTemplate(const QString& numberField) {
  if (numberField == QLatin1String("feature_no")) return QStringLiteral("%1호");
  if (numberField == QLatin1String("artifact_no")) return QStringLiteral("%1");
  if (numberField == QLatin1String("point_id")) return QStringLiteral("P%1");
  return {};
}

QString nextFrom(const QStringList& existing, const QString& fallbackTemplate) {
  Parts best;
  for (const QString& text : existing) {
    const Parts p = parse(text);
    if (p.value > best.value) best = p;
  }
  if (best.value < 0) return fallbackTemplate.isEmpty() ? QString() : fallbackTemplate.arg(1);
  const QString digits = QString::number(best.value + 1).rightJustified(best.width, QLatin1Char('0'));
  return best.prefix + digits + best.suffix;
}

Fields fieldsOf(const QgsVectorLayer* layer) {
  Fields out;
  if (!layer || !layer->isValid()) return out;
  const QgsFields fields = layer->fields();
  for (const QString& name : {QStringLiteral("feature_no"), QStringLiteral("artifact_no"),
                              QStringLiteral("section_id"), QStringLiteral("point_id")}) {
    const int index = fields.lookupField(name);
    if (index < 0) continue;
    out.number = index;
    out.numberName = fields.at(index).name();
    break;
  }
  // 유구번호 runs per kind (1호 주거지, 1호 수혈); 유물·단면·점 numbers run over the layer.
  if (out.numberName.compare(QLatin1String("feature_no"), Qt::CaseInsensitive) == 0)
    out.kind = fields.lookupField(QStringLiteral("kind"));
  return out;
}

QString suggestNext(const QgsVectorLayer* layer, const QString& kind) {
  const Fields fields = fieldsOf(layer);
  if (fields.number < 0) return {};
  const QString wanted = fields.kind >= 0 ? groupKey(kind) : QString();
  QStringList numbers;
  scan(layer, fields, [&](const QgsFeature&, const QString& k, const QString& n) {
    if (!n.isEmpty() && groupKey(k) == wanted) numbers << n;
  });
  return nextFrom(numbers, defaultTemplate(fields.numberName.toLower()));
}

QList<QgsFeatureId> sameNumber(const QgsVectorLayer* layer, const QString& kind, const QString& number,
                               QgsFeatureId exclude) {
  QList<QgsFeatureId> out;
  const Fields fields = fieldsOf(layer);
  const QString wanted = normalized(number);
  if (fields.number < 0 || wanted.isEmpty()) return out;
  const QString group = fields.kind >= 0 ? groupKey(kind) : QString();
  scan(layer, fields, [&](const QgsFeature& f, const QString& k, const QString& n) {
    if (f.id() != exclude && groupKey(k) == group && normalized(n) == wanted) out << f.id();
  });
  return out;
}

QList<Duplicate> duplicates(const QgsVectorLayer* layer) {
  QList<Duplicate> out;
  const Fields fields = fieldsOf(layer);
  if (fields.number < 0) return out;
  QHash<QString, QList<Duplicate>> groups;
  QStringList order;
  scan(layer, fields, [&](const QgsFeature& f, const QString& k, const QString& n) {
    const QString number = normalized(n);
    if (number.isEmpty()) return;
    const QString key = groupKey(k) + QChar(0x1F) + number;
    if (!groups.contains(key)) order << key;
    groups[key].append({f.id(), k.isEmpty() ? n : QStringLiteral("%1 %2").arg(k, n)});
  });
  for (const QString& key : std::as_const(order)) {
    const QList<Duplicate>& group = groups[key];
    if (group.size() > 1) out << group;
  }
  return out;
}

}  // namespace FeatureNumbering
