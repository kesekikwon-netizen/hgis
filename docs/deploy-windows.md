# 배포 (Windows)

## 권장 모델 (실행만)

1. 개발 PC에서 Release 빌드 후 `.\scripts\make-portable.ps1`
2. `dist\ka-hgis-portable\` **폴더 전체**를 USB/공유로 복사
3. 대상 Windows 10/11 64비트에서 `start.bat` 또는 `ka-hgis.exe`
4. OSGeo4W / Visual Studio 설치 불필요

다른 PC에서 **소스 개발**하려면: [`other-pc-setup.md`](./other-pc-setup.md)

## 런처

- 개발: `scripts/run-ka-hgis.ps1`
- 포터블: `dist/ka-hgis-portable/start.bat`

## 주의

- 포터블은 QGIS/Qt/GDAL DLL을 폴더에 넣습니다. git에는 올리지 않습니다.
- VWorld API 키·계정은 대상 PC 에서 앱으로 넣는다. 기본 포터블 폴더에는 넣지 않는다.
  **개인용 포터블 예외:** 사용자가 요청한 개인용 포터블(`make-portable.ps1 -IncludeLocalCredentials`)만 이 PC 의 VWorld 키와 계정 비밀번호를 폴더 `config` 에 **암호화 없이** 담는다(키는 그대로, 비밀번호는 앱과 같은 가림 형식 `password_portable=` 이며 암호화가 아니다). 다른 PC 에서 로그인 없이 쓰려는 사용자 결정이다. 기본값은 미포함이고, 담을 때 스크립트가 경고를 띄우며 `PORTABLE-MANIFEST.json` 의 `credentialsIncluded` 가 `true` 가 된다. 그 폴더는 본인 USB·PC 에만 두고 남에게 주거나 올리지 않는다.
- GPLv2+ (QGIS 라이브러리 링크). 폴더에 `LICENSE`(고지), `COPYING`(GPL v2 전문), `THIRD_PARTY_NOTICES.md`, `licenses/BUNDLED-COMPONENTS.txt`(번들 DLL 과 출처 패키지), `source/ka-hgis-source.zip`(이 EXE 의 대응 소스)이 들어간다.
- `PORTABLE-MANIFEST.json` 에 EXE SHA256 과 `verify-release` 통과 여부(`verified`/`unverified`)를 적는다. 받은 PC 에서 폴더 안의 `verify-portable-pack.ps1` 을 실행하면 EXE 가 그 해시와 같은지 대조한다. 포터블 생성은 막지 않는다.
