#pragma once
// Pixel helpers for the icon glyph tests (test_icons_outline.cpp): render an id, count and
// compare pixels, scan the app sources for the icon ids they name.
#include <QColor>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include <QString>

#include "app/KaIcons.h"

namespace IconPixels {

inline QImage render(const QString& id, int px = 64, QIcon::Mode mode = QIcon::Normal,
                     QIcon::State state = QIcon::Off) {
  return KaIcons::icon(id).pixmap(QSize(px, px), 1.0, mode, state).toImage().convertToFormat(QImage::Format_ARGB32);
}

inline bool near(const QColor& a, const QColor& b, int tolerance) {
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

inline bool isGrey(const QColor& c) { return qAbs(c.red() - c.green()) <= 2 && qAbs(c.green() - c.blue()) <= 2; }

template <typename Match>
int countPixels(const QImage& image, int minAlpha, Match match) {
  int n = 0;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x) {
      const QColor c = image.pixelColor(x, y);
      if (c.alpha() >= minAlpha && match(c)) ++n;
    }
  return n;
}

inline int covered(const QImage& image, int minAlpha = 128) {
  return countPixels(image, minAlpha, [](const QColor&) { return true; });
}

// Share of covered pixels whose coverage or colour differs clearly between two renders.
inline double differingShare(const QImage& a, const QImage& b) {
  int both = 0;
  int differing = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) {
      const QColor p = a.pixelColor(x, y);
      const QColor q = b.pixelColor(x, y);
      const bool pOn = p.alpha() >= 128;
      const bool qOn = q.alpha() >= 128;
      if (!pOn && !qOn) continue;
      ++both;
      if (pOn != qOn || qAbs(p.red() - q.red()) + qAbs(p.green() - q.green()) + qAbs(p.blue() - q.blue()) > 60)
        ++differing;
    }
  return both ? double(differing) / both : 0.0;
}

// True when the outer `band` pixels on every side are fully transparent.
inline bool bandIsClear(const QImage& image, int band) {
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if ((x < band || y < band || x >= image.width() - band || y >= image.height() - band) &&
          image.pixelColor(x, y).alpha() != 0)
        return false;
  return true;
}

// Icon ids named as string literals in the app sources (the scan test_icons.cpp uses).
inline QSet<QString> usedIconIds() {
  QSet<QString> ids;
  const QRegularExpression patterns[] = {
      QRegularExpression(QStringLiteral(R"re(KaIcons::(?:icon|strongIcon)\(QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re(addIcon\(\s*QStringLiteral\("\w+"\),\s*QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re((?:addWeb|makeOutput|addBottom)\(QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re(paintPrimary\(\w+,\s*QStringLiteral\("(\w+)"\))re")),
  };
  QDirIterator it(QStringLiteral("src/app"), {QStringLiteral("*.cpp")}, QDir::Files);
  while (it.hasNext()) {
    QFile file(it.next());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const QString text = QString::fromUtf8(file.readAll());
    for (const auto& pattern : patterns)
      for (auto m = pattern.globalMatch(text); m.hasNext();) ids.insert(m.next().captured(1));
  }
  return ids;
}

}  // namespace IconPixels
