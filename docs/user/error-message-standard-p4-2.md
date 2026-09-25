# P4-2 오류 메시지 표준 — 전후 비교

작성 2026-09-25. 헬퍼 `KaUserError`(`formatBody` + 선택 해결 단추).

공통 본문:

```
무엇이 안 됐나
…
왜 그런가
…
어떻게 하나
…
```

| # | 자리 | 이전 | 이후 |
| --- | --- | --- | --- |
| 1 | 앱 시작 임시 폴더 | 한 덩어리 문단 | what/why/how + 닫기 |
| 2 | 포터블 proj.db | 한 덩어리 문단 | what/why/how + 닫기 |
| 3 | 조사 열기(내장 작업공간) | 한 덩어리 문단 | what/why/how + 닫기 |
| 4 | 조사 열기(동반 프로젝트) | 한 덩어리 문단 | what/why/how + 닫기 |
| 5 | 다른 이름으로 저장(복사 실패) | 경로+오류만 | what/why/how + 닫기 |
| 6 | 다른 이름으로 저장(생성·작업공간) | 오류 문자열만 | what/why/how + 닫기 |
| 7 | 지적도 로그인 실패 | 본문+「아이디·비밀번호 다시 입력」 | 동일 단추, 본문 표준 |
| 8 | 지적도 준비 실패 | 한 줄/서버 문구 | what/why/how + 닫기 |
| 9 | 지적도 좌표계 | 한 줄 | what/why/how + 닫기 |
| 10 | VWorld API 키 필요 | information 안내만 | warn + 「VWorld API 키 입력」 |
| 11 | 드래그 파일 열기 실패 | 경로만 | what/why/how + 닫기 |
| 12 | CAD(DWG) 열기 실패 | 안내 문단 | what/why/how + 닫기 |
| 13 | CSV 기준점 읽기 실패 | 오류만 | what/why/how + 닫기 |
| 14 | 맞추기 파일 열기 | 한 줄 | what/why/how + 닫기 |
| 15 | 주변유적 폴더 만들기 | 한 줄 | what/why/how + 닫기 |
| 16 | 주변유적 사전 조건 | why만 | what/why/how + 닫기 |
| 17 | 수치지형도 폴더 | 한 줄 | what/why/how + 닫기 |
| 18 | 속성 저장 실패 | 커밋 오류만 | what/why/how + 닫기 |
| 19 | 편집 모드 열기 실패 | 레이어+상세 | what/why/how + 닫기 |
| 20 | 그리기 전 조사 없음 | 한 줄 | what/why/how + 「새 조사」 안내 |

시험: `user_error` (`formatBody_containsWhatWhyHow`, `warn_withAction_returnsActionChosen`, `warn_withoutAction_returnsDismissed`).
