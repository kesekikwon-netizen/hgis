---
name: capture-intent
description: Use FIRST, before any design or code, when the person asks in any project for a new feature or a change in how something works (e.g. 「…기능 넣어 줘」, 「…하게 바꿔 줘」, 「…도 되게 해 줘」) - writes their request in their own words to docs/intent/ as a short Korean 요청 기록, shows a summary with open questions, and waits for 「진행」 before building. Also use, read-only, when earlier work continues in a new conversation (「어제 하던 거 계속」) or the person says something went back to how it was (「예전으로 돌아갔다」) - read the matching docs/intent/ record first. Writes no record for bug reports, symptoms (「안 돼요」), questions, or one-line text or colour edits.
---

# 요청 기록 (capture-intent)

새 기능이나 동작 변경 요청을 설계·코드보다 먼저, 사용자 말 그대로 한 장에 남긴다. superpowers:brainstorming의 「의도 파악·이해한 내용 적어 보이기」 단계는 이 기록으로 한다.

## 언제 쓰나
- 쓴다: 새 기능, 화면이나 동작을 바꾸는 요청, 개발 설정을 바꾸는 요청.
- 쓰지 않는다: 버그·증상 신고, 질문, 글자·색·라벨 한두 개 바꾸기, 이미 확정된 기록의 작업을 이어 하라는 요청.
- 진행 중인 기능에 덧붙는 요청은 새 파일을 만들지 않는다. 그 기록의 「요청 원문」에 날짜와 함께 덧붙이고 나머지를 고친다.

## 이미 있는 기록 읽기
새 대화에서 하던 일을 이어 하라는 요청이나 「예전으로 돌아갔다」 같은 신고를 받으면, 다른 일보다 먼저 `docs/intent/`에서 관련 기록을 찾아 읽는다. 그 기록의 「바라는 결과」, 「지켜야 할 것」, 미정 질문에 받은 답을 기준으로 일한다. 새 기록은 만들지 않는다. 기록과 사용자의 새 말이 다르면 새 말을 따르고, 그 말을 기록의 「요청 원문」에 날짜와 함께 덧붙인다.

## 순서
1. 지금 동작, 파일 위치 같은 사실은 코드와 문서에서 직접 확인한다. 사용자에게 묻지 않는다.
2. 같은 폴더의 `template.md` 양식으로 프로젝트 루트에 `docs/intent/YYYY-MM-DD-<영문-소문자-하이픈>.md`를 만든다(폴더가 없으면 만든다). 한국어로 40줄 안팎으로 쓴다. 사용자 말은 「요청 원문」에 고치지 않고 옮긴다. 키·비밀번호·개인정보는 [가림]으로 바꾼다. 짐작한 내용에는 (추측)을 붙인다.
3. 한 메시지로 보인다: 기록 파일 링크, 이해한 내용 3~5줄(문제, 바라는 결과, 하지 않는 것, 확인 방법), 미정 질문(4개 이하, 질문마다 추천 답). 끝에 「맞으면 '진행', 고칠 곳이 있으면 그 부분만 말씀해 주세요」라고 쓰고 그 턴을 끝낸다. 코드는 아직 고치지 않는다.
4. 답을 받으면 고친 내용과 답을 기록에 반영하고 상태를 「확정」으로 바꾼다. 「추천대로」라는 답은 남은 미정 질문 모두에 추천 답을 적용한다. 그 프로젝트의 커밋 규칙이 허락하면 기록만 따로 커밋한다(git 저장소가 아니면 파일만 둔다). 그리고 같은 턴에 설계·구현으로 넘어간다.
5. 설계·계획 문서를 만들면 그 문서 첫머리에 기록 경로를 적고, 기록의 「관련 문서」에도 그 문서를 적는다.
6. 끝났다고 말하기 전에 「확인 방법」을 실제로 확인한다. 「결과」에 확인한 것과 커밋을 적고 상태를 「완료」로 바꾼다. 마지막 보고의 「화면에서 확인할 것」은 「확인 방법」에서 고른다. 사용자가 그만두라고 하면 상태를 「취소」로 바꾸고 이유를 한 줄 적는다.

## 작업 폴더가 없을 때
코드나 문서가 있는 작업 폴더라면 git 저장소가 아니어도 기록 파일을 만든다. 사용자 홈 폴더에서 시작한 대화처럼 작업할 폴더가 없으면, 파일은 만들지 않고 3단계 확인만 채팅으로 한다.
