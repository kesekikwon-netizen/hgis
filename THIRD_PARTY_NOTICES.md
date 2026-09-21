# 제3자 고지 (ka-hgis)

이 앱은 OSGeo4W `qgis-dev`의 `qgis_core` / `qgis_gui`에 링크한다(Architecture B).  
앱 자체는 GNU GPLv2 이상(`LICENSE`). 아래는 **배포 시 같이 두는** 의존 고지다. 전문은 공식 URL과 SDK `licenses/`를 본다.

측정 시점 2026-09-20. 이 PC 핀: `VERSION_QGIS_PIN.txt` — QGIS 4.3.0-Master, Qt 6.11.1 (OSGeo4W).  
오늘 공식 페이지: [QGIS GPL v2+·Qt 링크 예외](https://qgis.org/license/), [Qt 6.11 라이선스](https://doc.qt.io/qt-6/licensing.html), [Qt WebEngine](https://doc.qt.io/qt-6/qtwebengine-licensing.html), [GDAL MIT](https://gdal.org/en/stable/license.html), [PROJ MIT · contributors (2026)](https://proj.org/en/stable/about.html), [SQLite public domain](https://www.sqlite.org/copyright.html). API 키·계정은 이 파일에 없다.

## 앱이 링크하는 핵심

| 구성 | 라이선스 | 공식 근거 |
| --- | --- | --- |
| QGIS | GNU GPL v2 이상. Qt 링크 예외 | https://www.qgis.org/license/ |
| Qt 6 | LGPLv3 / GPLv2 / GPLv3 (모듈별). 상용 옵션 별도 | https://doc.qt.io/qt-6/licensing.html |
| Qt WebEngine / Chromium | Qt 쪽 LGPLv3 또는 GPLv2/3. Chromium은 구성 요소별(가장 제한적 LGPLv2.1) | https://doc.qt.io/qt-6/qtwebengine-licensing.html |
| GDAL/OGR | 대체로 MIT. 일부 파일 BSD 등. 빌드 의존에 따라 더 제한될 수 있음 | https://gdal.org/en/stable/license.html |
| PROJ | MIT (X/MIT). 인용: PROJ contributors (2026) | https://proj.org/en/stable/about.html |
| GEOS | LGPLv2.1 | 스플래시·정보 창 고지. https://libgeos.org/ |
| SQLite | Public domain | https://www.sqlite.org/copyright.html |

Qt 제3자 구성 목록: https://doc.qt.io/QT-6/licenses-used-in-qt.html  
OSGeo4W에서 복사한 QGIS `LICENSE`/`AUTHORS`는 포터블 `licenses/QGIS-*`.

## 글꼴

맑은 고딕(Malgun Gothic)은 **Windows에 설치된 글꼴을 이름만 지정**한다. 앱 폴더에 글꼴 파일을 넣지 않는다. 재배포 조건은 Microsoft 글꼴 라이선스를 따른다.

## 지도·자료 (런타임 다운로드)

VWorld, 국토정보플랫폼, 국가유산 공간정보, 흙토람, KIGAM 등은 **제공처 이용조건**이 별도이다. API 키는 앱에 넣지 않는다.

## 포터블 폴더에서 위치

- 이 파일: `THIRD_PARTY_NOTICES.md` (폴더 루트)
- 앱 GPL: `LICENSE`
- QGIS 원본 고지: `licenses/QGIS-LICENSE` 등 (`scripts/make-portable.ps1`이 복사)
