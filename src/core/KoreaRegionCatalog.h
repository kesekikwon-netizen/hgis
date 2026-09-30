#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct KoreaRegionBounds {
  double west;
  double south;
  double east;
  double north;
};

struct KoreaSido {
  QString name;
  QString shortName;
  QStringList cities;
};

class KoreaRegionCatalog {
public:
  static QVector<KoreaSido> allSido();
  static QStringList sidoNames();
  static QStringList citiesOf(const QString& sidoName);
  static QStringList dongsOf(const QString& sidoName, const QString& cityName);
  static QString canonicalSido(const QString& name);
  // WGS84 mainland/Jeju overview framing, not administrative boundary geometry.
  static std::optional<KoreaRegionBounds> overviewBounds(const QString& sido);
  static QString composeAddress(const QString& sido, const QString& city, const QString& dong,
                                const QString& lot);

  // The 시·도/시·군·구 table is updatable data (evaluation F123). An optional
  // data/korea_regions.json (searched like data/korea_dongs.json) replaces the built-in
  // table after an administrative reform without a rebuild:
  //   {"version":1,"sido":[{"name":"경상북도","short":"경북","aliases":["경상북도청"],
  //                         "cities":["포항시", ...]}, ...]}
  // A missing or invalid file keeps the built-in table unchanged.
  // aliases: alternative 시·도 spelling → name (used by canonicalSido).
  static QVector<KoreaSido> parseTable(const QByteArray& json,
                                       QHash<QString, QString>* aliases = nullptr,
                                       QString* error = nullptr);
  // "built-in" or the path of the loaded data file.
  static QString tableSource();
};
