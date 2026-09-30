---
name: ten-out-of-ten
description: 앱 품질을 10점으로 올리는 작업 — 점수판(scorecard), P0~P7 작업 카드, 한글 경로 시험, 포터블 비밀번호, UX 시나리오, MainWindow 줄이기 — 을 할 때 사용한다. 사용자가 10점, 점수판, 작업 카드, P0~P7, MainWindow 분리를 말하면 연다.
---

# 10점 계획 작업 규칙

- 계획서는 `docs/superpowers/plans/2026-09-25-ten-out-of-ten.md`다. `plans/`는 .gitignore 대상이라 PC마다 따로 복사해 둔다. 없으면 사용자에게 받는다. AGENTS.md가 이 계획보다 우선한다.
- 한 채팅에 작업 카드 한 장. 고치기 전에 계획을 보여 주고 사용자의 "진행"을 기다린다. 카드가 서로 얽히면 되돌리기 어렵기 때문이다.
- 구조는 옮기기만 하고 동작을 바꾸지 않는다. 핫스팟(`MainWindow.cpp`, `LayerOps.cpp`)을 키우지 않는다.
- 계정 파일(`%LOCALAPPDATA%\ka-hgis\ka-hgis\*.ini`)과 사용자 `Documents` 폴더의 개인 메모 `.txt`는 열거나 바꾸지 않는다.
- 통과는 이 세션에서 Windows PC로 실제로 돌린 명령과 결과로만 말한다. 점수는 `scripts/scorecard.ps1` 결과를 인용한다. 커밋·푸시는 사용자가 말할 때만.
- C++를 바꾸면 `cpp-dev-loop` 스킬의 네 단계를 그대로 따른다.
