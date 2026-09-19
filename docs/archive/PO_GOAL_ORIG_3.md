# GOAL-ORIG-3 — Layer panel: ONLY user SHP + basemap, drag reorder

**PO order (explicit user requirement — do exactly this)**

## Layer panel contents (STRICT)
Show ONLY:
1. **User-drawn / user-loaded SHP (and survey vector layers the user is editing)** — the geometries the user draws or opens
2. **User-chosen basemap** (OSM / VWorld sat / cadastral / etc. that user turned on)

**DELETE from layer dock UI (all of this text/chrome):**
- "🧭 7단계…" remnants (already gone — keep gone)
- Long help labels under the tree if any
- "VWorld 지도 투명도 (펼치기)" checkbox block from the **layer dock** (move opacity to 배경지도 menu or a small toolbar action — NOT cluttering layer list)
- Group propaganda text like "레이어 (조사 데이터 / 참조 지도)" if it confuses — prefer clean QGIS-like tree:
  - Either flat list, OR two plain groups without essay labels: `데이터` / `배경` (short names only)
- Checklist dock can stay separate; do not dump checklist text into layer panel
- No empty decorative labels, no emoji section titles in layer dock

## What appears when
| Action | Layer tree |
|--------|------------|
| Fresh start | Empty tree (or only basemap if user already added) |
| User draws / new survey domain layers with features OR empty editable layers user will fill | Those vector layers listed |
| User adds basemap | Basemap layer listed |
| User loads external SHP | That SHP listed |

If using GPKG domain 5 layers: they are the "user data" layers (user fills them by drawing). Still only those + basemaps — nothing else.

## Drag reorder (MANDATORY)
- User must **drag layers up/down with mouse** to change draw order (z-order / legend order)
- Use `QgsLayerTreeModel::AllowNodeReorder` (already mentioned in old plan)
- After drop: canvas refresh so overlapping SHP paint order matches tree (top of tree = drawn on top, match QGIS convention — document which way in report)
- Unit test: mimeData + dropMimeData changes root/group child order (QtTest)

## Also
- Remove any auto junk layers user did not ask for
- saveEdits / digitize still work on data layers
- ctest 100%
- No git commit/push

## Report
docs/PO_GOAL_ORIG_3_REPORT.md
A_LAYER_ONLY_SHP_BASEMAP / B_NO_EXTRA_TEXT / C_DRAG_REORDER / CTEST / EVIDENCE / READY

START NOW.
# ADDENDUM to GOAL-ORIG-3 — Layer panel is ADDITIVE ONLY

**User clarification (binding):**

## Layer panel = pile up ONLY when user adds something

| User action | Layer tree |
|-------------|------------|
| App start | **Completely empty** — no groups required, no placeholder layers, no empty "조사구역" stubs |
| User adds basemap (menu) | That ONE basemap layer appears |
| User adds/loads SHP | That SHP appears |
| User starts drawing on a new layer / creates layer / new survey layer that user actually uses | Layer appears **when created for use**, not a wall of 5 empty GPKG names forced at once if user did not ask — prefer: add layer when user picks "구역 그리기" first time (create-or-focus that layer), same for 유구/선/GPS |
| User never added X | X must **not** sit in the tree |

### Forbidden
- Auto-filling 5 empty domain layers into the tree on 새 조사 just to "show structure"
- Permanent empty groups "조사 데이터/참조 지도" with no children (either no groups, or groups only when they have layers)
- Any text/chrome that is not a layer name
- Layers the user did not add

### Still required
- Drag reorder among layers that ARE in the tree (overlap order)
- Only user SHP/data + user basemap
- ctest green

Update implementation + report section: `ADDITIVE_ONLY: YES`

If ORIG-3 already mid-flight, apply this addendum before finishing report.
