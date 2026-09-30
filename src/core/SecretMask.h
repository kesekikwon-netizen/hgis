#pragma once

// Removes key and account values from text before it is written to a log (evaluation F029).
// VWorld keys are GUID-shaped (8-4-4-4-12 hex) and travel inside tile URLs, WMS "KEY=" query
// items (percent-encoded as KEY%3D inside a QGIS "url=" parameter) and GDAL_WMS XML.
// Other services use apiKey=, serviceKey=, crtfc_key=, token= or a password field.
// Only the value is replaced; the rest of the line stays readable for diagnosis.
// Header-only so ka_core, the app and the Qt-only log tests share one rule.

#include <QRegularExpression>
#include <QString>

namespace SecretMask {

inline constexpr const char* kMasked = "****";

inline QString mask(const QString& text) {
  // Cheap pre-check: every masked form contains '-' (GUID), '=', "%3D" or "Basic/Bearer".
  if (text.isEmpty()) return text;
  const bool maybeGuid = text.contains(QLatin1Char('-'));
  const bool maybeParam = text.contains(QLatin1Char('=')) ||
                          text.contains(QLatin1String("%3D"), Qt::CaseInsensitive);
  const bool maybeAuth = text.contains(QLatin1String("Basic "), Qt::CaseInsensitive) ||
                         text.contains(QLatin1String("Bearer "), Qt::CaseInsensitive);
  if (!maybeGuid && !maybeParam && !maybeAuth) return text;

  QString out = text;
  if (maybeParam) {
    // name=value, name%3Dvalue (a URL inside a QGIS source) and name%253Dvalue (twice encoded).
    // The name must start a token ("layer_key=" is an app property, not a secret); an
    // encoded separator (%26 = '&', %3F = '?') in front also starts one.
    static const QRegularExpression param(
        QStringLiteral(
            // Plain literals on purpose: moc cannot tokenize raw string literals in headers it scans.
            "(?:(?<=%26)|(?<=%3F)|(?<![A-Za-z0-9_]))((?:api[_-]?key|service[_-]?key|auth[_-]?key|crtfc[_-]?key|"
            "access[_-]?token|refresh[_-]?token|token|key|secret|password|passwd|pwd|"
            "user[_-]?pw|userpw|pw)(?:=|%3D|%253D))((?:(?!%26|%2526)[^&\\s\"'<>;,)\\]])+)"),
        QRegularExpression::CaseInsensitiveOption);
    out.replace(param, QStringLiteral("\\1") + QLatin1String(kMasked));
  }
  if (maybeGuid) {
    // Also inside a percent-encoded URL ("…1.0.0%2F<key>%2FSatellite…").
    static const QRegularExpression guid(QStringLiteral(
        "(?:(?<=%2F)|(?<=%2f)|(?<=%3D)|(?<=%3d)|(?<![0-9A-Za-z]))"
        "[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
        "[0-9A-Fa-f]{12}(?![0-9A-Za-z])"));
    out.replace(guid, QLatin1String(kMasked));
  }
  if (maybeAuth) {
    static const QRegularExpression auth(QStringLiteral("\\b(Basic|Bearer)\\s+[A-Za-z0-9+/=._~-]{8,}"),
                                         QRegularExpression::CaseInsensitiveOption);
    out.replace(auth, QStringLiteral("\\1 ") + QLatin1String(kMasked));
  }
  return out;
}

}  // namespace SecretMask
