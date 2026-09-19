# ka-hgis 문서

현행 목록은 이 한 장이다. 옛 스프린트 목표(`PO_GOAL_*`)와 Grok 지형도 핸드오프는 [archive/](archive/README.md)에 있다.

## 지금 보는 곳

| 문서 | 용도 |
| --- | --- |
| [AGENTS.md](../AGENTS.md) | Cursor 작업 규칙·제품 불변식 |
| [.codex/NOW.md](../.codex/NOW.md) | 최근 세션 상태 |
| [HANDOFF.md](../HANDOFF.md) · [HANDOFF.md](HANDOFF.md) | 제품 진실 (둘을 같이 고친다) |
| [developer-tools.md](developer-tools.md) | 여섯 도구 루프 |
| [testing-map.md](testing-map.md) | 축 A–J 자동 시험 |
| [clangd-navigation.md](clangd-navigation.md) | 선언·정의 조회 |
| [archify-setup.md](archify-setup.md) | 구조도 설치·실행 |

## 설계

| 문서 | 용도 |
| --- | --- |
| [adr/0001-standalone-cpp-qgis-libs.md](adr/0001-standalone-cpp-qgis-libs.md) | Architecture B |
| [architecture/data-flow.md](architecture/data-flow.md) | 임계 경로 (현행은 AGENTS·소스 우선) |
| [domain/data-model.md](domain/data-model.md) | GPKG 레이어·필드 |
| [vendor/qgis-manual-3.44/README.md](vendor/qgis-manual-3.44/README.md) | 로컬 QGIS 매뉴얼 |

## 빌드·배포

| 문서 | 용도 |
| --- | --- |
| [build-windows.md](build-windows.md) | Windows 빌드 |
| [deploy-windows.md](deploy-windows.md) | 배포 |
| [../THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) | Qt·QGIS·GDAL·PROJ·글꼴 고지 |
| [portable-desktop.md](portable-desktop.md) | 포터블 |
| [other-pc-setup.md](other-pc-setup.md) | 다른 PC |
| [git-token-setup.md](git-token-setup.md) | git 토큰 |

## 사용자

| 문서 | 용도 |
| --- | --- |
| [user/gui-scenario-checklist.md](user/gui-scenario-checklist.md) | 화면 QA 서식 |
| [user/layer-information.md](user/layer-information.md) | 레이어 목록 |
| [user/job-cards/](user/job-cards/) | 작업 카드 01–07 |
| [user/](user/) | 지적·지번·조판·참조지도 등 |

## 그 밖

- [ux/](ux/) 와이어프레임·초심자 IA
- [research/](research/) 조사 메모
- [recovery/](recovery/) 장애 복구 기록
- [superpowers/](superpowers/) 옛 스펙·플랜 (현행 계획이 아님)
- [COMMIT_STATUS.md](COMMIT_STATUS.md) 훅이 갱신. 손으로 고치지 않는다.
- [ERROR_REGRESSION.md](ERROR_REGRESSION.md) 회귀 메모
- [archive/](archive/README.md) 폐기된 목표·세션 상세
