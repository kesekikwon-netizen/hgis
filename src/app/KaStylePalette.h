#pragma once

#include <QColor>
#include <QList>
#include <QStringList>

// The eight standard drawing colours (빨, 주, 노, 초, 파, 남, 보, 갈) of the survey-area window
// and the inspector's 스타일 tab.
namespace KaStylePalette {

inline QList<QColor> colors() {
  return {QColor(0xDC, 0x26, 0x26), QColor(0xEA, 0x58, 0x0C), QColor(0xCA, 0x8A, 0x04), QColor(0x16, 0xA3, 0x4A),
          QColor(0x25, 0x63, 0xEB), QColor(0x1E, 0x3A, 0x8A), QColor(0x7C, 0x3A, 0xED), QColor(0x92, 0x40, 0x0E)};
}

inline QStringList names() {
  return {QStringLiteral("빨강"), QStringLiteral("주황"), QStringLiteral("노랑"), QStringLiteral("초록"),
          QStringLiteral("파랑"), QStringLiteral("남색"), QStringLiteral("보라"), QStringLiteral("갈색")};
}

}  // namespace KaStylePalette
