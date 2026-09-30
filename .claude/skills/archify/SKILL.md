---
name: archify
description: 소스로 확인한 구조·워크플로·시퀀스·데이터 흐름·상태 전이를 검증된 독립 HTML 다이어그램으로 만든다. ka-hgis C++ 변경의 필수 4단계 중 Archify 단계, 아키텍처 설명, 여러 모듈 리팩터링 리뷰, Mermaid 변환·정리 요청에 사용한다. 실행은 scripts/archify.ps1로 한다.
---

# archify (Claude Code 연결)

이 스킬의 정본은 `.agents/skills/archify/SKILL.md`다. Cursor와 Claude Code가 같은 원본을 쓰도록 여기서는 내용을 복사하지 않는다.

1. `.agents/skills/archify/SKILL.md`를 Read로 끝까지 읽고 그 지시를 따른다.
2. 원본이 가리키는 `references/`, `scripts/` 등은 `.agents/skills/archify/` 기준 상대 경로다.
3. 원본의 "Cursor", "Task subagent" 표현은 CLAUDE.md의 `<harness_override>`대로 Claude Code와 `.claude/agents/` 서브에이전트로 읽는다.
4. 스킬 내용을 고칠 때는 이 파일이 아니라 원본을 고친다.
