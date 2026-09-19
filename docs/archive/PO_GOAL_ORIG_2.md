# GOAL-ORIG-2 — Layer panel like QGIS + ONE VWorld key store

**PO finding (user complaint valid):**

## 1) Layer panel confusion
QGIS layer panel shows map layers currently in the project.
ka-hgis MUST behave the same mental model:

| When | 조사 데이터 group | 참조 지도 group |
|------|-------------------|-----------------|
| App start (no survey) | empty (or hidden empty group OK) | empty until user picks basemap |
| After 새 조사 | 5 layers: 조사구역, 유구면, 유구선, 단면선, GPS기준점 (empty features, editable) | keep any basemap user already added |
| After 배경지도 → 위성 | unchanged survey layers | VWorld 위성 (and/or 지적) listed under 참조 지도, checked ON, visible on canvas |
| User draws polygon | feature appears on survey layer; layer stays in tree | basemap stays under |

Status bar / empty canvas tip when no layers: Korean "새 조사로 데이터 레이어를 만들거나, 배경지도 메뉴에서 참조 지도를 추가하세요."

## 2) API key still prompted AFTER user gave key — ROOT CAUSE
TWO different stores:

| Reader | QSettings key | Registry path |
|--------|---------------|---------------|
| VworldSettings::loadApiKey | `VWorld/ApiKey` | HKCU\Software\ka-hgis\ka-hgis\VWorld\ApiKey |
| LocationSearch::vworldApiKey | `vworld/apiKey` | HKCU\Software\ka-hgis\ka-hgis\vworld\apiKey |

PO had written key only to wrong/legacy path earlier; VWorld\ApiKey was EMPTY so every 위성/지적 call ran vworldApiKeyOrPrompt().

### FIX (code — mandatory)
1. **Single SSOT:** Only `VworldSettings::loadApiKey/saveApiKey` (`VWorld/ApiKey`).
2. `LocationSearch::vworldApiKey()` must call `VworldSettings::loadApiKey()` (env override OK first).
3. `LocationSearch::setVworldApiKey` and Help dialog both call `VworldSettings::saveApiKey` only.
4. On loadApiKey: if new key empty, **migrate** once from legacy `vworld/apiKey` and from env `VWORLD_API_KEY`, then save to SSOT.
5. After save, basemap add must NOT prompt if key non-empty.
6. If VWorld HTTP returns InvalidParameterValue (bad key), show Korean: "등록되지 않은 VWorld 키입니다. 도움말→키 설정에서 확인" — do not silent gray.

## 3) Verify
- ctest 100%
- Manual logic: loadApiKey returns non-empty when registry VWorld\ApiKey set
- Unit test: migrate legacy key path
- Write docs/PO_GOAL_ORIG_2_REPORT.md

No commit/push. START NOW.
