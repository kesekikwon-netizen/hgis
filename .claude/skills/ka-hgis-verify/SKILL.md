---
name: ka-hgis-verify
description: 사용자가 화면으로 확인하던 일을 측정 가능한 확인으로 바꾼다. 주변유적 받기, 리본, 지도, 조판처럼 ctest 통과만으로 끝나지 않는 데스크톱 UI 변경에 사용한다. 사용자가 스크린샷을 보이거나 확인, 검증, 너무 오래 걸린다, 되지 않는다고 말하면 연다.
---

# ka-hgis-verify (Claude Code 연결)

이 스킬의 정본은 `.agents/skills/ka-hgis-verify/SKILL.md`다. Cursor와 Claude Code가 같은 원본을 쓰도록 여기서는 내용을 복사하지 않는다.

1. `.agents/skills/ka-hgis-verify/SKILL.md`를 Read로 끝까지 읽고 그 지시를 따른다.
2. 원본이 가리키는 `references/`, `scripts/` 등은 `.agents/skills/ka-hgis-verify/` 기준 상대 경로다.
3. 원본의 "Cursor", "Task subagent" 표현은 CLAUDE.md의 `<harness_override>`대로 Claude Code와 `.claude/agents/` 서브에이전트로 읽는다.
4. 스킬 내용을 고칠 때는 이 파일이 아니라 원본을 고친다.
