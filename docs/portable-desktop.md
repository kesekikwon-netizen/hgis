# 바탕화면 개인용 포터블

Windows 10/11 64비트용 전체 폴더 배포다. Qt6, QGIS, GDAL, PROJ 좌표계 자료, Qt WebEngine과 MSVC 런타임을 포함한다. EXE 하나만 복사하지 말고 ZIP을 완전히 푼 폴더 또는 폴더 전체를 옮긴다. 쓰기 가능한 위치에서 `ka-hgis.exe`를 실행한다.

## 생성

현재 Release를 빌드한 뒤 새 출력 경로로 실행한다. 이전 배포·조사 자료는 덮어쓰지 않는다.

```powershell
.\scripts\make-portable.ps1 -OutDir "$([Environment]::GetFolderPath('Desktop'))\HGIS-포터블-새버전" -IncludeLocalCredentials
.\scripts\verify-portable-pack.ps1 -OutDir "$([Environment]::GetFolderPath('Desktop'))\HGIS-포터블-새버전"
```

`-IncludeLocalCredentials`는 사용자가 요청한 개인용 배포에서만 사용한다. 기본값은 계정 미포함이다. **개인용 포터블 예외:** 사용자가 요청한 개인용 포터블(`make-portable.ps1 -IncludeLocalCredentials`)만 이 PC 의 VWorld 키와 계정 비밀번호를 폴더 `config` 에 **암호화 없이** 담는다(키는 그대로, 비밀번호는 앱과 같은 가림 형식 `password_portable=` 이며 암호화가 아니다). 다른 PC 에서 로그인 없이 쓰려는 사용자 결정이다. 기본값은 미포함이고, 담을 때 스크립트가 경고를 띄우며 `PORTABLE-MANIFEST.json` 의 `credentialsIncluded` 가 `true` 가 된다. 그 폴더는 본인 USB·PC 에만 두고 남에게 주거나 올리지 않는다. [other-pc-setup.md](other-pc-setup.md)의 「git·번들로 옮기지 않는다」와 [deploy-windows.md](deploy-windows.md)의 「폴더에 넣지 않음」은 기본 포터블 이야기이고, 이 개인용 예외만 다르다. 현재 앱의 `LocalAppData/ka-hgis/ka-hgis` 설정을 우선하며 옛 org-only VWorld 키가 현재 키를 덮지 않는다. 개인용 폴더에는 VWorld 키, 수치지형도 계정, 국가유산 인트라넷 계정이 들어간다. 키·계정 갱신은 앱의 더보기 메뉴에서 한다. 인터넷 서비스는 연결과 유효한 인증이 필요하다.

## 무엇이 어떤 판인지 (검증됨/미검증)

포터블 생성은 사용자가 요청할 때만 하며 검증 실패로 막지 않는다. 대신 `PORTABLE-MANIFEST.json` 에 `executableSha256`, `releaseStatus`(`verified` = 그 EXE 가 `scripts/verify-release.ps1` 의 빌드·전체 CTest·시작 검사를 통과했고 소스가 그 뒤로 바뀌지 않음, 아니면 `unverified`), `releaseStatusNote`, `gitDescribe`, `qgisPin` 을 적는다. `publish-desktop.ps1` 로 EXE 만 바꿔도 이 파일을 다시 쓴다. 받은 PC 에서는 폴더 안의 `verify-portable-pack.ps1` 로 EXE 해시를 대조하고 판 상태를 본다.

## 다른 PC의 Windows 실행 차단과 서명

2026-09-15 개발 PC의 Release EXE는 Authenticode 검사 결과 `NotSigned`다. 현재 사용자 인증서 저장소에도 코드 서명 인증서가 없다. 이 상태에서는 포터블 파일 구성이 정상이어도 Windows가 게시자를 확인할 수 없어 실행을 경고하거나 차단할 수 있다. 아래 검사는 파일을 변경하지 않고 서명 상태와 전달본 대조용 SHA256을 출력한다. 검사 명령이 끝났다는 사실이 배포 승인이나 SmartScreen 통과를 의미하지 않는다.

```powershell
.\scripts\sign-release.ps1 -CheckOnly
.\scripts\sign-release.ps1 -CheckOnly -ExePath 'D:\HGIS-포터블\ka-hgis.exe'
```

- **Microsoft Defender SmartScreen / 인식할 수 없는 앱**: 파일·게시자 평판 검사다. 사용자가 출처와 전달받은 해시를 확인했고 화면에 선택지가 있을 때만 **추가 정보 → 실행**으로 해당 앱 실행을 선택할 수 있다. 새 서명 파일은 평판이 쌓이기 전 경고가 남을 수 있고, EV 인증서도 즉시 경고 제거를 보장하지 않는다. [Microsoft SmartScreen 설명](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)
- **Smart App Control / 조직 정책 차단**: 위 실행 선택지가 없을 수 있다. 차단된 파일과 Windows 보안 기록을 확인해야 한다. Smart App Control은 DLL·보조 EXE 등 실행 코드에도 적용되므로 주 EXE 하나의 서명으로 전체 번들 통과를 보장하지 않는다. [Microsoft 서명 요구사항](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control), [Smart App Control FAQ](https://support.microsoft.com/en-US/Windows/Security/threat-malware-protection/smart-app-control-frequently-asked-questions)

지속적인 배포에는 공인 CA가 발급한 **RSA 코드 서명 인증서와 서명 권한**이 필요하다. 인증서 발급·본인/조직 확인·비용 승인은 소유자가 진행해야 한다. 자체 서명 인증서는 다른 PC의 공적 신뢰를 만들지 못한다. 배포 스크립트는 Defender 설정, 차단 표시 또는 수신 PC의 신뢰 저장소를 변경하지 않는다.

인증서 공급자 도구로 인증서를 Windows `CurrentUser\My`(또는 `LocalMachine\My`)에 연결한 뒤 Windows SDK의 `signtool.exe`와 공급자의 RFC 3161 타임스탬프 주소로 최종 빌드를 서명한다. 아래 변수는 실제 설치 경로·공개 인증서 지문·공급자 URL로 먼저 지정한다. PFX나 비밀번호는 저장소 또는 명령줄에 넣지 않는다.

```powershell
.\scripts\sign-release.ps1 -Sign -CertificateThumbprint $publisherThumbprint -SignToolPath $windowsSdkSignTool -TimestampUrl $providerTimestampUrl
# 시스템 인증서 저장소를 사용하면 -CertificateStore LocalMachine 추가
```

스크립트는 지정된 인증서를 확인하고 SHA256 서명과 타임스탬프를 적용한 후 Authenticode 정책으로 검증한다. 실패하면 배포를 중단한다. **최종 Release 빌드 → 서명 → 요청한 포터블 생성 → 전달 EXE 서명/해시 확인** 순서를 지킨다. 다시 링크한 EXE와 이미 전달한 미서명 포터블은 별도로 서명이 필요하다. 타임스탬프나 후속 검증 실패 시 파일에 부분 서명이 남을 수 있으므로 성공 검증 전 전달하지 않는다. [Microsoft SignTool 문서](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)

실제 신뢰된 인증서를 사용한 서명과 다른 PC에서의 차단 해소는 아직 검증하지 않았다. 수신 PC에서 라이브러리나 조직 정책이 차단 원인인 경우 해당 파일/정책을 별도로 확인해야 한다.

## 화면과 한글

- Windows의 논리 화면 크기/배율을 사용한다. 메인 창은 표시·모니터 이동 때 작업영역에 맞춘다. 주변유적 진행창도 자세히 확장할 때 화면에 맞춘다.
- 단면도는 고정 1100픽셀 최소폭을 제거하고 속성 폼 줄바꿈·세로 스크롤로 작은 화면에서 생성/PDF 버튼에 접근한다. 화면보다 많은 항목은 스크롤해서 사용한다.
- 주변유적을 새로 받거나 적재할 때 원본 SHP 인코딩을 해석한 뒤 UTF-8 GeoPackage 작업 사본을 쓴다. UTF-8 SHP로 재작성할 때 발생하는 한글 DBF 필드명·값 잘림을 피한다. 내려받은 원본 SHP/CPG는 그대로 보관한다.
- 이미 열린/저장된 SHP를 시작할 때 자동 변환하지 않는다. 해당 자료를 다시 적재하면 새 경로가 적용된다.

## 검증 범위

파일 구성 검사만으로 다른 PC에서의 모든 기능을 검증했다고 간주하지 않는다. SDK 경로를 제거한 실행 환경의 시작 검사, 번들 좌표계와 WebEngine 검사, 작은 논리 화면의 버튼 접근 검사를 별도로 수행한다. 실제 다른 PC와 모든 모니터·그래픽 드라이버를 검증했다는 의미는 아니다. 개별 전달 결과는 `build/qa/desktop-portable-20260915/`에 기록한다.
