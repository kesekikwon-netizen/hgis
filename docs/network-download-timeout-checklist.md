# 내려받기 시간 제한 점검표 (P3-4)

작성 2026-09-25. 서비스마다 취소·시간 제한·재시도·인터넷 없을 때 문구를 적는다.
빈칸이 있으면 이 카드를 끝내지 않는다.

| 서비스 | 취소 | 시간 제한 | 재시도 | 인터넷 없을 때 문구 | 근거 |
| --- | --- | --- | --- | --- | --- |
| AdminBoundaryService | `cancel()` + 진행 창 | deadline + `setTransferTimeout` 기본 20s | 사용자 다시 불러오기 | 「인터넷 연결을 확인…」 | `AdminBoundaryService.cpp`; `boundaryDeadlineAndServiceErrorAllowRetry` |
| HeritageHttpClient | 상위 `KaHeritageBrowser::stop` | `setTransferTimeout` 30s | 브라우저 다시 받기 | 실패 시 `errorString` + 한글 안내 | `HeritageHttpClient.cpp` `kTimeoutMs`; `heritage_download_retry` |
| HeritageRegionResolver | `cancel()` | deadline + `setTransferTimeout` 기본 15s | 다시 판정 / 시·군 직접 고르기 | 「시·군을 직접 고르세요」 | `HeritageRegionResolver.cpp` |
| LocationSearch | `cancel()` + 진행 창 | deadline + `setTransferTimeout` 기본 20s | 다시 검색 | 「인터넷 연결을 확인…」 | `LocationSearch.cpp`; `locationDeadline*` |
| PreparedReferenceMap | `QgsFeedback` | `setTransferTimeout` + **절대** QTimer 기본 15s | 사용자 다시 받기 | 「지도 서버…인터넷 연결…」 | `PreparedReferenceMap.cpp`; `trickleResponseHasAbsoluteDeadline` |
| SoilMapService | feedback → PreparedReferenceMap | 위와 동일(15s) | 다시 내려받기 | PreparedReferenceMap·토양 범위 문구 | `SoilMapService::prepare` |
| GeologyMapService | feedback → PreparedReferenceMap | 위와 동일(15s) | 다시 내려받기 | PreparedReferenceMap·지질 문구 | `GeologyMapService::prepare` |
| RiverMapService | feedback → PreparedReferenceMap | 위와 동일(15s) | 다시 내려받기 | 「인터넷 연결을 확인…」 | `RiverMapService.cpp` |
| DemDownloadService | feedback / 콜백 | `GDAL_HTTP_CONNECTTIMEOUT=5`, `GDAL_HTTP_TIMEOUT=15`, `MAX_RETRY=0` | 사용자 다시 받기(자동 재시도 0) | 「인터넷 연결을 확인…」 | `DemDownloadService.cpp` `ThreadConfiguration` |
| PaleoLandformService | 해당 없음(로컬 토양 시드만) | 해당 없음(HTTP 없음) | 해당 없음 | 해당 없음 | `PaleoLandformService.cpp` |
| TopographicCatalog | feedback / 콜백 | 해당 없음(로컬 색인만) | 캐시 다시 만들기 | 해당 없음(네트워크 없음) | `TopographicCatalog.cpp` |
| CadastralPortal | poll cancel | idle 40s/120s + **절대** 마감(동일 값, readyRead로 리셋 안 함) + `setTransferTimeout` | 다시 받기 | 「인터넷 연결과 계정…」 / 「응답 시간이 초과…」 | `CadastralPortal.cpp` `Session::request`; `trickleResponseHasAbsoluteDeadline` |

검증: `ctest --test-dir build -C Release -R "^cadastral$" --output-on-failure`
