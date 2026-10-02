# 동봉 글꼴

| 파일 | 출처 | 버전 | 라이선스 |
|---|---|---|---|
| IBMPlexSansKR-Regular/Medium/SemiBold/Bold.ttf (hinted) | https://github.com/IBM/plex/releases/tag/%40ibm/plex-sans-kr%401.1.0 (ibm-plex-sans-kr.zip, sha256 9837800c…24562e) | 1.1.0 | SIL OFL 1.1 (LICENSE-IBM-Plex-Sans-KR.txt) |
| IBMPlexMono-Regular/Medium/Bold.ttf | https://github.com/IBM/plex/releases/tag/%40ibm/plex-mono%402.5.0 (ibm-plex-mono.zip, sha256 6d23f012…bc481c) | 2.5.0 | SIL OFL 1.1 (LICENSE-IBM-Plex-Mono.txt) |

내려받은 날: 2026-09-29. **기본은 꺼져 있다**(2026-09-30): 현장 PC 에서 IBM Plex Sans KR 이 11~13 px 에서 맑은 고딕보다 흐리고 가늘게 보여 기본 글꼴은 맑은 고딕으로 두었다.
켜려면 `KA_HGIS_BUNDLED_FONTS=1` 또는 KaTheme::DisplayOptions::bundledFonts. 켜면 KaTheme::apply 가 이 폴더의 .ttf 를 QFontDatabase::addApplicationFont 로 올린다.
글꼴 이름: "IBM Plex Sans KR", "IBM Plex Mono". 파일이 없으면 맑은 고딕으로 돌아간다.
