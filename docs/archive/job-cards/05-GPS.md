# 잡 카드 05 — GPS

> **설계 기록, 현재 UI 아님.** 2026-09-29 보관(F215). 왼쪽 7단계 레일·오른쪽 도움 패널·[도움말] 단추·새 조사 기본 EPSG:5179·그리는 중 속성 팝업은 현재 앱에 없거나 사용자 결정으로 뒤집혔다. 현재 사용법은 [docs/user/quick-start.md](../../user/quick-start.md), 제품 규칙은 `AGENTS.md`.

1. 기준점 추가: datum/ellipsoid/projection + accuracy_m/PDOP/fix_type
2. 최소 2점
3. CSV v2: id,x,y,datum,ellipsoid,projection,accuracy_m,pdop,fix_type
