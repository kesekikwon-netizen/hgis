---
name: ka-hgis-gis
description: ka-hgis의 GIS 동작(QGIS 레이어, CRS 5186/5187/5179, 편집 버퍼, GeoPackage 저장·열기, 정합, 지도·PDF·SHP 내보내기, 범례 그룹)을 구현·진단·리뷰할 때 반드시 사용한다. 사용자가 지도, 레이어, 좌표계, 디지타이즈, 내보내기, VWorld, 배경지도, 도면이 이상하다고 말하면 이 스킬부터 연다. 일반 C++ 문법 질문이나 GIS와 무관한 편집에는 쓰지 않는다.
---

# ka-hgis-gis (Claude Code 연결)

이 스킬의 정본은 `.agents/skills/ka-hgis-gis/SKILL.md`다. Cursor와 Claude Code가 같은 원본을 쓰도록 여기서는 내용을 복사하지 않는다.

1. `.agents/skills/ka-hgis-gis/SKILL.md`를 Read로 끝까지 읽고 그 지시를 따른다.
2. 원본이 가리키는 `references/`, `scripts/` 등은 `.agents/skills/ka-hgis-gis/` 기준 상대 경로다.
3. 원본의 "Cursor", "Task subagent" 표현은 CLAUDE.md의 `<harness_override>`대로 Claude Code와 `.claude/agents/` 서브에이전트로 읽는다.
4. 스킬 내용을 고칠 때는 이 파일이 아니라 원본을 고친다.
