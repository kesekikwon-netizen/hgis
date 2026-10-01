---
name: portable-release
description: Make the Strata (ka-hgis) portable folder when the user asks for 「포터블」, 「키 전부 포함 포터블」, 「개인용 포터블」, a USB copy or "다른 PC에서 쓰게". Runs scripts/make-portable.ps1 through a UTF-8-BOM copy, names the Desktop folder, verifies with verify-portable-pack.ps1 and a no-OSGeo4W smoke, and says what the folder holds. Only on request, never as part of a normal build.
---

# Portable release (only when the user asks)

Report in Korean. Paths, commands and identifiers stay in English. Run every command
from the Bash tool in the worktree root; the PowerShell tool turns native stderr into a
terminating error (git CRLF warnings, qWarning) and the script would stop half-way.

## 0. Say this before starting

- A portable folder is about 1.0 GB (measured 2026-10-01: 1030 MB) and the user also
  packs a `.rar` of it, so C: needs about 2.5 GB free. Check first:
  `powershell.exe -NoProfile -Command '[math]::Round((Get-PSDrive C).Free/1GB,1)'`
  and tell the user the number (11 GB on 2026-10-01 17:30; five older portable folders
  and three `.rar` files already sit on the Desktop and are the user's to delete).
- 「키 전부 포함」/「개인용」 means `-IncludeLocalCredentials`: the VWorld key goes into
  `config\` as plain text and the NGII, heritage-intranet and VWorld-cadastral account
  passwords travel in the folder too (obfuscated or plain, see step 6; never encrypted).
  Say that the folder is for the user's own USB/PC only. Without those words, build
  without the switch.

## 1. Preconditions

- Release exe at `build\Release\ka-hgis.exe`, built from the commit the user expects.
  `KA_HGIS_GIT_HASH` is taken at CMake configure time (`git rev-parse --short=12 HEAD`,
  CMakeLists.txt lines 9-19), so after new commits reconfigure, then build and smoke:
  ```
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $ErrorActionPreference="Continue"; cmake --preset vs; cmake --build build --config Release --parallel --target ka-hgis; exit $LASTEXITCODE'
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; $p = Start-Process build/Release/ka-hgis.exe -ArgumentList "--smoke-quit" -WorkingDirectory build/Release -Wait -PassThru; exit $p.ExitCode'
  ```
  On C1060 / MSB4018 / MSB4166 / 0x800705AF rerun the build with `-- /m:1 /p:CL_MPCount=2`.
- OSGeo4W at `A:\OSGeo4W` (the script also accepts `OSGEO4W_ROOT`, `C:\OSGeo4W`,
  `D:\OSGeo4W`) with `apps\qgis-dev` and `bin\curl-ca-bundle.crt`; it stops otherwise.
- The output folder must not exist: the script throws "Output already exists" and never
  overwrites a delivery. Name: `C:\Users\kwonyoungin1\Desktop\Strata-포터블-개인용-<YYYY-MM-DD>`
  with the key switch, `Strata-포터블-<YYYY-MM-DD>` without it; add `-v2`, `-v3` when the
  date is taken. Never delete an older folder to make room.

## 2. Make the BOM copy (why: the script has Korean text and no BOM)

`scripts/make-portable.ps1` starts with `# Build` (no BOM) and contains the Korean
README text; Windows PowerShell 5.1 reads a BOM-less file as cp949 and writes mojibake
README.txt / 사용법.txt (the 2026-09-30 portables show it). Prepend a BOM to a copy
that sits in `scripts/`, so `$PSScriptRoot` and `$MyInvocation` still resolve:
```
printf '\xEF\xBB\xBF' > scripts/make-portable.bom.ps1 && cat scripts/make-portable.ps1 >> scripts/make-portable.bom.ps1
```
Delete the copy in step 5. It is untracked; `git status --short scripts` must be empty
afterwards.

## 3. Run it (Korean path on the command line is fine from Bash)

```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/make-portable.bom.ps1 -OutDir "C:\Users\kwonyoungin1\Desktop\Strata-포터블-개인용-$(date +%F)" -IncludeLocalCredentials
```
Measured 2026-10-01: a Korean `-OutDir` passed from Bash to `-File` arrives intact
(char codes 54252,53552,48660 / 44060,51064,50857). Korean in the script's console
output looks garbled in Bash; that is output encoding only. Confirm the folder with
`ls "C:/Users/kwonyoungin1/Desktop/"`. The script prints key/account lines as "copied"
or "not found" and never prints values; do not print them either.

Parameters of `scripts/make-portable.ps1`:

| Parameter | Meaning |
| --- | --- |
| `-OutDir <path>` | target folder; default `dist\ka-hgis-portable`; must not exist |
| `-IncludeLocalCredentials` | copy the VWorld key and the three account INIs into `<out>\config` |
| `-RefreshManifestOnly` | merge-line script only: rewrite `PORTABLE-MANIFEST.json` of an existing folder (used by publish-desktop.ps1); needs `<out>\ka-hgis.exe` |

What it copies: `ka-hgis.exe` (+ `.pdb` if present), `data\`, `docs\user\`, the QGIS
prefix (without python/grass/include/lib/doc/server/bin), Qt6 plugins and the WebEngine
runtime (`copy-webengine-runtime.ps1`), GDAL data, `share\proj` (+ `proj.db`/`proj.ini`
next to the exe), every runtime DLL into the root (unused Oracle/ODBC/qgis_app/python/
Designer DLLs removed), the MSVC runtime from System32, `curl-ca-bundle.crt`, `LICENSE`,
`THIRD_PARTY_NOTICES.md`, QGIS notices in `licenses\`, `run.ps1`, `run.bat`, `start.bat`,
`README.txt`, `사용법.txt`.

## 4. Verify

```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/verify-portable-pack.ps1 -OutDir "<out>"
```
Expect `portable pack verification passed`, exit 0. Warnings `portable has no COPYING /
licenses/BUNDLED-COMPONENTS.txt / source/ka-hgis-source.zip` or `No PORTABLE-MANIFEST.json`
mean the folder was made by the main-line script (step 6), not a broken folder.

Smoke without OSGeo4W, with the log in a scratch folder, through a BOM helper so the
Korean path never sits inside a `-Command` string:
```
S="$(cygpath -w "$CLAUDE_SCRATCHPAD_DIR" 2>/dev/null || echo "$TEMP")"
printf '\xEF\xBB\xBFparam([string]$OutDir, [string]$LogDir)\nforeach ($n in "OSGEO4W_ROOT","QGIS_PREFIX_PATH","QT_PLUGIN_PATH","QGIS_PLUGIN_PATH","GDAL_DATA","PROJ_DATA","PROJ_LIB") { Remove-Item "Env:$n" -ErrorAction SilentlyContinue }\n$env:PATH = (($env:PATH -split ";") | Where-Object { $_ -notmatch "OSGeo4W" }) -join ";"\nNew-Item -ItemType Directory -Force -Path $LogDir | Out-Null\n$env:KA_HGIS_LOG_DIR = $LogDir\n$p = Start-Process (Join-Path $OutDir "ka-hgis.exe") -ArgumentList "--smoke-quit" -WorkingDirectory $OutDir -Wait -PassThru\nexit $p.ExitCode\n' > scripts/smoke-portable.bom.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/smoke-portable.bom.ps1 -OutDir "<out>" -LogDir "$S\portable-smoke"; echo "exit=$?"
grep -rl "$(git rev-parse --short=12 HEAD)" "$S/portable-smoke" && echo "log shows this commit"
rm scripts/smoke-portable.bom.ps1
```
Exit 0 and a session log naming the current commit (`KaSessionLog` prints version, git
hash and QGIS version) are the pass condition. A log naming an older hash means step 1
was skipped: rebuild and repackage into a new folder.

## 5. Clean up and report

- `rm scripts/make-portable.bom.ps1`; `git status --short scripts` must print nothing.
- Never commit, print or copy anything under `<out>\config`; never add the folder to git
  (`*.exe`, `*.dll`, `**/config/*-account.ini`, `**/secrets.ini` are ignored, but say it
  anyway).
- Report in Korean: the folder path, size, that `config\` holds the key and accounts
  (file names only), that it is for the user's own USB/PC, the verify and smoke results
  with exit codes, free space left on C:, and one on-screen check: double-click
  `<out>\ka-hgis.exe`; 더보기 → 정보 shows the commit hash, and 더보기 → API 키 is already
  filled on a personal portable.

## 6. Two script versions exist (check `git log -1 -- scripts/make-portable.ps1`)

| | main 9f3992c (a6eb712 line) | merge line claude/strata-merge-20261001 (de0414f), restored in 384f5e9 |
| --- | --- | --- |
| passwords in `config\*-account.ini` | DPAPI → plain `password=` | DPAPI → `password_portable=` (base64 of UTF-8 XOR `ka-hgis-account-v1`; obfuscated, not encrypted) |
| extra files | — | `PORTABLE-MANIFEST.json` (EXE SHA256, `releaseStatus` verified/unverified, git commit, `credentialsIncluded`), `COPYING`, `licenses\BUNDLED-COMPONENTS.txt`, `source\ka-hgis-source.zip`, a copy of `verify-portable-pack.ps1` |
| `-RefreshManifestOnly` | no | yes |
| `-IncludeLocalCredentials` warning | none | `Write-Warning` naming the plain key / obfuscated passwords |
| verify-portable-pack | file list only | also compares the EXE hash with the manifest and warns when `credentialsIncluded` |

`releaseStatus` is `unverified` unless `scripts/verify-release.ps1` (full ctest + smoke,
long) ran for this exact EXE; that is normal for a personal portable, say so, and run
verify-release only when the user asks. The Desktop folder `Strata-포터블-개인용-2026-10-01`
was made with the main-line script (no manifest; verify prints the "made before F178"
warning).
