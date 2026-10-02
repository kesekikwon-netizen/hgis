#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <functional>
#include <qgsrectangle.h>

class QgsFeedback;

// Fetches one large WFS request range piece by piece. The whole range is tried
// first (fast path); a piece that is too large or too slow is split into four
// quarters, and a dropped connection is retried once. Nothing is returned unless
// every piece completed, so a caller keeps its existing map on any failure.
namespace ReferenceTiledFetch {

enum class Outcome {
  Complete,   // body holds this piece's complete FeatureCollection
  Cancelled,  // the user stopped the download
  Fatal,      // the server answered with something unusable; splitting cannot help
  TooLarge,   // more features than one request may carry; split, never repeat
  Slow,       // total deadline reached while data was still arriving; split, retry at the finest level
  Transient,  // connection dropped or went silent; retry the same piece once
};

struct Piece {
  Outcome outcome = Outcome::Fatal;
  QByteArray body;
  QString error;
};

struct Options {
  int maxDepth = 2;           // whole range, then 2x2, then 4x4 pieces at most
  int retriesPerPiece = 1;    // extra attempts for Transient/Slow failures
  qint64 budgetMs = 300000;   // no further split or retry after this wall time
};

struct Result {
  bool ok = false;
  bool cancelled = false;
  QByteArray body;            // merged FeatureCollection, or the single original body
  QString error;
  int requests = 0;
  int pieces = 0;             // completed pieces that were merged
};

using FetchPiece = std::function<Piece(const QgsRectangle& range)>;

Result fetch(const QgsRectangle& range, const FetchPiece& fetchPiece,
             QgsFeedback* feedback = nullptr, const Options& options = {});

// North-west, north-east, south-west, south-east quarters of the range.
QList<QgsRectangle> quarters(const QgsRectangle& range);

// Joins FeatureCollections. A feature returned by neighbouring pieces (same
// "id", or byte-identical JSON when it has no id) is kept once. The first body's
// other members (crs, type) are preserved and the count members are rewritten.
QByteArray mergeFeatureCollections(const QList<QByteArray>& bodies, QString* error = nullptr);

}  // namespace ReferenceTiledFetch
