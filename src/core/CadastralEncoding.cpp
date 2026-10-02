#include "CadastralEncoding.h"

#include <QFile>
#include <QFileInfo>

namespace {

QString sibling(const QFileInfo& shp, const char* suffix) {
  const QString base = shp.absolutePath() + QLatin1Char('/') + shp.completeBaseName() + QLatin1Char('.');
  const QString lower = base + QString::fromLatin1(suffix);
  if (QFileInfo::exists(lower)) return lower;
  const QString upper = base + QString::fromLatin1(suffix).toUpper();
  return QFileInfo::exists(upper) ? upper : QString();
}

// Length of a valid UTF-8 multi-byte sequence starting at i, 0 when it is not one.
int utf8Sequence(const unsigned char* bytes, int i, int size) {
  const unsigned char c = bytes[i];
  int length = 0;
  if (c >= 0xC2 && c <= 0xDF) length = 2;
  else if ((c & 0xF0) == 0xE0) length = 3;
  else if (c >= 0xF0 && c <= 0xF4) length = 4;
  if (length == 0 || i + length > size) return 0;
  for (int k = 1; k < length; ++k)
    if ((bytes[i + k] & 0xC0) != 0x80) return 0;
  return length;
}

}  // namespace

QString CadastralEncoding::detectDbfEncoding(const QByteArray& sample) {
  const auto* bytes = reinterpret_cast<const unsigned char*>(sample.constData());
  const int size = static_cast<int>(sample.size());
  int multiByte = 0;
  for (int i = 0; i < size; ++i) {
    if (bytes[i] < 0x80) continue;
    const int length = utf8Sequence(bytes, i, size);
    // A cut-off sequence at the very end of the sample is not evidence against UTF-8.
    if (length == 0) {
      if (size - i < 4) break;
      return QStringLiteral("CP949");
    }
    ++multiByte;
    i += length - 1;
  }
  // Only clear UTF-8 text overrides the Korean public-data default.
  return multiByte >= 4 ? QStringLiteral("UTF-8") : QStringLiteral("CP949");
}

QString CadastralEncoding::fallbackFor(const QString& sourcePath) {
  const QFileInfo shp(sourcePath);
  if (shp.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) != 0) return {};
  if (!sibling(shp, "cpg").isEmpty()) return {};
  const QString dbf = sibling(shp, "dbf");
  if (dbf.isEmpty()) return {};
  QFile file(dbf);
  if (!file.open(QIODevice::ReadOnly)) return {};
  // Skip the binary header (record count, lengths): only record text says anything.
  const QByteArray header = file.read(32);
  if (header.size() < 12) return QStringLiteral("CP949");
  const auto* h = reinterpret_cast<const unsigned char*>(header.constData());
  const int headerLength = h[8] | (h[9] << 8);
  if (headerLength > 0 && !file.seek(headerLength)) return QStringLiteral("CP949");
  return detectDbfEncoding(file.read(32768));
}
