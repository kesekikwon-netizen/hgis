// App icon id -> Lucide / Strata SVG for the Mockup glyph style. The ids are the ribbon ids
// (MainWindowRibbon.cpp) plus the tab, map-control and panel ids of the mockup screens.
#include "KaIconsMockup.h"

#include <QHash>

namespace KaIconsMockup {

namespace {

struct Entry {
  const char* id;
  const char* svg;  // "<folder>/<name>" inside the :/ka-hgis/icons resource prefix
};

constexpr Entry kEntries[] = {
    // 조사
    {"new", "lucide/file-plus"},
    {"open", "lucide/folder"},
    {"save", "lucide/save"},
    {"save_unsaved", "lucide/save"},
    {"save_as", "lucide/save-pen"},
    // 기록
    {"select", "lucide/mouse-pointer-2"},
    {"measure", "ka/ka-ruler-horizontal"},
    {"draw_poly", "lucide/vector-polygon"},
    {"trench_grid", "lucide/grid-3x3"},
    {"buffer", "lucide/circle-dot-dashed"},
    // 자료 받기
    {"cadastral", "ka/ka-cadastral"},
    {"topo_download", "lucide/download"},
    {"heritage", "lucide/house"},
    // 배경 지도
    {"contour", "lucide/mountain"},
    {"dem", "lucide/mountain-snow"},
    {"survey_contour", "lucide/target"},
    {"soil", "lucide/layers"},
    {"paleo", "lucide/rotate-ccw-clock"},
    {"geology", "ka/ka-geology"},
    {"river", "lucide/waves-horizontal"},
    {"old_map", "lucide/map"},
    // 정합, 내보내기, 기타
    {"georef", "lucide/locate-fixed"},
    {"pdf", "lucide/file-text"},
    {"print", "lucide/printer"},
    {"section", "lucide/chart-line"},
    {"geotiff", "lucide/image"},
    {"export_convert", "lucide/box"},
    {"more", "lucide/ellipsis"},
    // 탭, 앱 바, 지도 조작, 화면 부품
    {"home", "lucide/house"},
    {"map", "lucide/map"},
    {"region", "lucide/map-pin"},
    {"search", "lucide/search"},
    {"undo", "lucide/undo-2"},
    {"redo", "lucide/redo-2"},
    {"zoom_in", "lucide/plus"},
    {"zoom_out", "lucide/minus"},
    {"zoom_fit", "lucide/maximize"},
    {"folder", "lucide/folder"},
    {"note", "lucide/clipboard-list"},
    {"layers", "lucide/layers"},
    {"warn", "lucide/triangle-alert"},
    {"snap", "lucide/magnet"},
    {"chevron_left", "lucide/chevron-left"},
    {"chevron_right", "lucide/chevron-right"},
    {"satellite", "lucide/satellite"},
};

}  // namespace

QString svgPathFor(const QString& id) {
  static const QHash<QString, QString> paths = [] {
    QHash<QString, QString> table;
    for (const Entry& entry : kEntries)
      table.insert(QString::fromLatin1(entry.id),
                   QStringLiteral(":/ka-hgis/icons/") + QString::fromLatin1(entry.svg) + QStringLiteral(".svg"));
    return table;
  }();
  return paths.value(id);
}

}  // namespace KaIconsMockup
