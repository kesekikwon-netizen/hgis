#include "StandardScales.h"

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace StandardScales {
namespace {

struct Entry {
  int denominator;
  unsigned uses;
};

constexpr unsigned kSnap = Snap;
constexpr unsigned kChip = StudioChip;
constexpr unsigned kSection = Section;
constexpr unsigned kPrint = PrintEnlarge;

// Excavation plans/sections (1:10 .. 1:100), Korean cadastral sheets
// (1:500, 1:600, 1:1,200, 1:3,000) and topographic maps (1:5,000 .. 1:50,000).
// 1:25 is a section-only value: the snap keeps denominators ending on 10.
constexpr Entry kTable[] = {
    {10, kSnap | kSection},
    {20, kSnap | kChip | kSection},
    {25, kSection},
    {30, kSnap | kChip | kSection},
    {40, kSnap | kSection},
    {50, kSnap | kChip | kSection},
    {60, kSnap | kSection},
    {80, kSnap},
    {100, kSnap | kChip | kSection | kPrint},
    {150, kSnap},
    {200, kSnap | kChip | kSection | kPrint},
    {250, kSnap | kChip | kSection | kPrint},
    {300, kSnap | kChip},
    {400, kSnap | kChip},
    {500, kSnap | kChip | kPrint},
    {600, kSnap | kPrint},
    {1000, kSnap | kChip | kPrint},
    {1200, kSnap | kPrint},
    {2000, kSnap | kChip | kPrint},
    {2500, kSnap | kPrint},
    {3000, kSnap},
    {4000, kSnap},
    {5000, kSnap | kChip | kPrint},
    {10000, kSnap | kChip | kPrint},
    {20000, kSnap},
    {25000, kSnap | kChip | kPrint},
    {40000, kSnap},
    {50000, kSnap | kPrint},
    {100000, kSnap},
    {200000, kSnap},
    {500000, kSnap},
};

}  // namespace

QList<int> denominators(unsigned uses) {
  QList<int> out;
  for (const Entry& e : kTable)
    if (e.uses & uses) out.append(e.denominator);
  return out;
}

bool isStandard(double denominator, unsigned uses) {
  if (!(denominator > 0.0) || !std::isfinite(denominator)) return false;
  const long long rounded = std::llround(denominator);
  if (std::abs(denominator - double(rounded)) > 1e-6 * std::max(1.0, denominator)) return false;
  for (const Entry& e : kTable)
    if ((e.uses & uses) && e.denominator == rounded) return true;
  return false;
}

int snapUp(double raw, unsigned uses) {
  if (!(raw > 0.0) || !std::isfinite(raw)) return 0;
  int last = 0;
  for (const Entry& e : kTable) {
    if (!(e.uses & uses)) continue;
    last = e.denominator;
    if (double(e.denominator) + 1e-6 >= raw) return e.denominator;
  }
  return last;
}

QString label(double denominator) {
  return QStringLiteral("1:%1").arg(
      QLocale(QLocale::English).toString(qlonglong(std::llround(denominator))));
}

}  // namespace StandardScales
