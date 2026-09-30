#include "ReferenceTiledFetch.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <qgsfeedback.h>

namespace ReferenceTiledFetch {
namespace {

struct State {
  const FetchPiece& fetchPiece;
  QgsFeedback* feedback = nullptr;
  const Options& options;
  QElapsedTimer clock;
  QList<QByteArray> bodies;
  Result result;

  bool withinBudget() const { return clock.elapsed() < options.budgetMs; }
  bool cancelled() const { return feedback && feedback->isCanceled(); }
};

bool splittable(const QgsRectangle& range) {
  return range.isFinite() && range.width() > 0. && range.height() > 0.;
}

// Returns false once the whole fetch has failed or was cancelled; the caller
// then discards every completed piece.
bool fetchRange(State& state, const QgsRectangle& range, int depth) {
  for (int attempt = 0;; ++attempt) {
    if (state.cancelled()) {
      state.result.cancelled = true;
      return false;
    }
    // The first request always runs; later pieces stop once the budget is spent.
    if (state.result.requests > 0 && !state.withinBudget()) {
      state.result.error = QStringLiteral("지도 자료를 받는 시간이 %1분을 넘어 멈췄습니다. 기존 지도는 유지됩니다.")
                               .arg(qMax<qint64>(1, state.options.budgetMs / 60000));
      return false;
    }
    Piece piece = state.fetchPiece(range);
    ++state.result.requests;
    if (state.cancelled()) piece.outcome = Outcome::Cancelled;
    switch (piece.outcome) {
      case Outcome::Complete:
        state.bodies.append(piece.body);
        return true;
      case Outcome::Cancelled:
        state.result.cancelled = true;
        state.result.error = piece.error;
        return false;
      case Outcome::Fatal:
        state.result.error = piece.error;
        return false;
      case Outcome::TooLarge:
      case Outcome::Slow:
        if (depth < state.options.maxDepth && state.withinBudget() && splittable(range)) {
          for (const QgsRectangle& quarter : quarters(range))
            if (!fetchRange(state, quarter, depth + 1)) return false;
          return true;
        }
        // A too-large answer is deterministic; only a slow one is worth repeating.
        if (piece.outcome == Outcome::Slow && attempt < state.options.retriesPerPiece &&
            state.withinBudget())
          continue;
        state.result.error = piece.error;
        return false;
      case Outcome::Transient:
        if (attempt < state.options.retriesPerPiece && state.withinBudget()) continue;
        state.result.error = piece.error;
        return false;
    }
    state.result.error = piece.error;
    return false;
  }
}

QString featureKey(const QJsonObject& feature) {
  const QJsonValue id = feature.value(QStringLiteral("id"));
  if (id.isString() && !id.toString().isEmpty()) return QStringLiteral("id:") + id.toString();
  if (id.isDouble()) return QStringLiteral("id:") + QString::number(id.toDouble(), 'g', 17);
  const QByteArray json = QJsonDocument(feature).toJson(QJsonDocument::Compact);
  return QStringLiteral("json:") +
         QString::fromLatin1(QCryptographicHash::hash(json, QCryptographicHash::Sha1).toHex());
}

}  // namespace

QList<QgsRectangle> quarters(const QgsRectangle& range) {
  const double midX = (range.xMinimum() + range.xMaximum()) / 2.;
  const double midY = (range.yMinimum() + range.yMaximum()) / 2.;
  return {QgsRectangle(range.xMinimum(), midY, midX, range.yMaximum()),
          QgsRectangle(midX, midY, range.xMaximum(), range.yMaximum()),
          QgsRectangle(range.xMinimum(), range.yMinimum(), midX, midY),
          QgsRectangle(midX, range.yMinimum(), range.xMaximum(), midY)};
}

QByteArray mergeFeatureCollections(const QList<QByteArray>& bodies, QString* error) {
  QJsonObject root;
  QJsonArray features;
  QSet<QString> seen;
  bool first = true;
  for (const QByteArray& body : bodies) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject object = document.object();
    if (!document.isObject() || !object.value(QStringLiteral("features")).isArray()) {
      if (error)
        *error = QStringLiteral("나눠 받은 지도 자료를 합치지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.");
      return {};
    }
    if (first) {
      root = object;
      first = false;
    }
    for (const QJsonValue& value : object.value(QStringLiteral("features")).toArray()) {
      const QJsonObject feature = value.toObject();
      const QString key = featureKey(feature);
      if (seen.contains(key)) continue;
      seen.insert(key);
      features.append(feature);
    }
  }
  root.insert(QStringLiteral("type"), QStringLiteral("FeatureCollection"));
  root.insert(QStringLiteral("features"), features);
  for (const auto* key : {"numberMatched", "numberReturned", "totalFeatures"})
    if (root.contains(QLatin1String(key))) root.insert(QLatin1String(key), features.size());
  return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result fetch(const QgsRectangle& range, const FetchPiece& fetchPiece, QgsFeedback* feedback,
             const Options& options) {
  State state{fetchPiece, feedback, options, {}, {}, {}};
  state.clock.start();
  const bool ok = fetchPiece && fetchRange(state, range, 0);
  Result result = state.result;
  if (!ok) {
    if (!result.cancelled && result.requests > 1)
      result.error = QStringLiteral("지도 자료를 %1번 나눠 요청했지만 끝까지 받지 못했습니다. 기존 지도는 유지됩니다. "
                                    "연결이 안정된 곳에서 다시 내려받으세요.\n%2")
                         .arg(result.requests)
                         .arg(result.error);
    result.body.clear();
    return result;
  }
  result.pieces = state.bodies.size();
  if (state.bodies.size() == 1) {
    result.body = state.bodies.first();
  } else {
    result.body = mergeFeatureCollections(state.bodies, &result.error);
    if (result.body.isEmpty()) return result;
  }
  result.ok = true;
  return result;
}

}  // namespace ReferenceTiledFetch
