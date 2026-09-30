---
paths:
  - "src/**/*.{cpp,cxx,cc,h,hpp}"
  - "tests/**/*.{cpp,cxx,cc,h,hpp}"
---

# C++/Qt/QGIS 소스 규칙

이 규칙은 C++ 소스를 읽거나 고칠 때만 적용된다.

- Architecture B: `qgis_core`/`qgis_gui`를 링크해서 쓴다. PROJ·GDAL·QGIS 렌더러·CRS 변환을 다시 구현하지 않는다. 낯선 `Qgs*` API는 `docs/vendor/qgis-manual-3.44/`와 설치된 SDK 헤더로 먼저 확인한다.
- 편집 흐름은 `startEditing` → `QgsFeature(layer->fields())` + `setGeometry` + `addFeature` → `commitChanges`/`rollBack`.
- 도메인 레이어는 `LayerOps::ensureDomainLayer`로만 추가하고, 논리 식별자는 `ka_hgis/layer_key`에 둔다. 한국어 제목은 UI 라벨일 뿐이다.
- `loadSurveyLayers`에서 `removeAllMapLayers()`를 부르지 않는다. 빈 도메인 레이어를 자동으로 범례에 넣지 않는다.
- Qt 소유권: 부모가 있는 `QObject`는 부모가 지운다. 부모 없는 객체만 `std::unique_ptr`나 명시적 `deleteLater`로 관리한다. 시그널/슬롯은 함수 포인터 형식 `connect(sender, &Sender::sig, receiver, &Receiver::slot)`을 쓴다.
- GUI 스레드에서 오래 걸리는 I/O를 새로 만들지 않는다. 불가피하면 기존 진행 표시 패턴을 따른다.
- 예외: QGIS/Qt 경계에서는 반환값·`bool ok`·`QString* error` 패턴이 기존 코드의 주류다. 새 예외 계층을 도입하지 않는다.
- 경고: `/W4` 경고 0을 유지한다. 새 경고를 억제 pragma로 숨기지 않는다.
- 테스트: Qt Test(`QTEST_MAIN`, `QCOMPARE`, `QVERIFY`) 패턴을 따르고, 바뀐 동작마다 `tests/`에 회귀 시험을 둔다.
