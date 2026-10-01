// Strata (ka-hgis) Claude Code 개발 설정 설치 / 업데이트.
//
//   node scripts\setup-claude-dev.mjs
//
// What it does, in order (stops before changing anything if a check fails):
//   1. Checks: claude CLI, Git Bash, free space on C:, and that A:\qgis can take the
//      project files (on main, nothing in the way).
//   2. Backs up ~/.claude/settings.json and the plugin lists to
//      ~/.claude/_reset_backup/<date-time>-dev-setup/ (plus A:\qgis .gitignore edits).
//   3. Installs or updates, at user scope, and turns on:
//        superpowers@superpowers-marketplace       (obra/superpowers, 6.4.2 or newer)
//        security-guidance@claude-plugins-official (2.0.8 or newer)
//   4. Builds security-guidance's Python venv now (about 300 MB on C:), so the
//      first session does not have to.
//   5. Runs superpowers' SessionStart hook once, the same way Claude Code does.
//   6. Brings CLAUDE.md, .claude/settings.json, .claude/claude-security-guidance.md
//      and .gitignore into A:\qgis: `git checkout -- .gitignore` then
//      `git merge --ff-only <branch>`, then `git push origin main` (never forced).
//      New desktop sessions branch from GitHub's main, so the push is what makes
//      CLAUDE.md reach them.
// Safe to run again later: it updates the plugins and skips what is already done.
// Options: --branch=<name>  --skip-repo  --no-push  --repo=<path> (testing only)

import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const BRANCH_DEFAULT = 'claude/current-dev-setup-0b45e7'; // branch that holds the project files
const argv0 = process.argv.slice(2);
const REPO = (argv0.find((a) => a.startsWith('--repo=')) || '').slice(7) || 'A:/qgis';
const PROJECT_FILES = ['CLAUDE.md', '.claude/settings.json', '.claude/claude-security-guidance.md'];
const MARKETPLACES = ['anthropics/claude-plugins-official', 'obra/superpowers-marketplace'];
const PLUGINS = [
  { id: 'superpowers@superpowers-marketplace', min: '6.4.2', name: 'superpowers' },
  { id: 'security-guidance@claude-plugins-official', min: '2.0.8', name: 'security-guidance' },
];

const args = process.argv.slice(2);
const BRANCH = (args.find((a) => a.startsWith('--branch=')) || '').slice(9) || BRANCH_DEFAULT;
const SKIP_REPO = args.includes('--skip-repo');
const NO_PUSH = args.includes('--no-push');
const CONFIG_DIR = process.env.CLAUDE_CONFIG_DIR || path.join(os.homedir(), '.claude');

const say = (s = '') => console.log(s);
function fail(msg) {
  console.error(`\n[중단] ${msg}`);
  process.exit(1);
}
function run(cmd, argv, opts = {}) {
  const r = spawnSync(cmd, argv, { encoding: 'utf8', windowsHide: true, ...opts });
  return { code: r.status ?? -1, out: r.stdout || '', err: r.stderr || '', error: r.error };
}
const git = (...a) => run('git', ['-C', REPO, ...a]);
const slash = (p) => p.replace(/\\/g, '/');
function newer(v, min) {
  const a = String(v).split('.').map((n) => parseInt(n, 10) || 0);
  const b = min.split('.').map((n) => parseInt(n, 10) || 0);
  for (let i = 0; i < Math.max(a.length, b.length); i++) {
    if ((a[i] || 0) !== (b[i] || 0)) return (a[i] || 0) > (b[i] || 0);
  }
  return true;
}

// ---------- 1. checks (nothing is changed yet) ----------
say('Strata 개발 설정: 확인 중...');

let CLAUDE = 'claude';
let CLAUDE_SHELL = true; // npm's claude.cmd needs a shell on Windows
if (run('claude', ['--version'], { shell: true }).code !== 0) {
  const base = path.join(process.env.APPDATA || '', 'Claude', 'claude-code');
  const dirs = fs.existsSync(base) ? fs.readdirSync(base).filter((d) => fs.existsSync(path.join(base, d, 'claude.exe'))) : [];
  dirs.sort((x, y) => (newer(x, y) ? 1 : -1));
  if (!dirs.length) fail('claude 명령을 찾지 못했습니다. 잠시 뒤 다시 실행해 주세요.');
  CLAUDE = path.join(base, dirs[dirs.length - 1], 'claude.exe');
  CLAUDE_SHELL = false;
}
const claude = (...a) => {
  const r = spawnSync(CLAUDE, a, { stdio: 'inherit', shell: CLAUDE_SHELL, windowsHide: true });
  return r.status ?? -1;
};

const BASH = [process.env.CLAUDE_CODE_GIT_BASH_PATH, 'C:\\Program Files\\Git\\bin\\bash.exe', 'C:\\Program Files (x86)\\Git\\bin\\bash.exe']
  .find((p) => p && fs.existsSync(p));
if (!BASH) fail('Git Bash(C:\\Program Files\\Git\\bin\\bash.exe)를 찾지 못했습니다. Git for Windows가 필요합니다.');

try {
  const st = fs.statfsSync('C:\\');
  const freeGB = (st.bavail * st.bsize) / 1024 ** 3;
  if (freeGB < 1) fail(`C: 드라이브 남은 공간이 ${freeGB.toFixed(1)} GB입니다. 1 GB 이상 비운 뒤 다시 실행해 주세요.`);
  say(`  C: 남은 공간 ${freeGB.toFixed(1)} GB`);
} catch {
  say('  C: 남은 공간을 확인하지 못했습니다(계속 진행).');
}

let repoPlan = 'skip';
if (!SKIP_REPO) {
  if (git('rev-parse', '--abbrev-ref', 'HEAD').out.trim() !== 'main') fail(`${REPO} 가 main 브랜치가 아닙니다.`);
  if (git('rev-parse', '--verify', '--quiet', `refs/heads/${BRANCH}^{commit}`).code !== 0) {
    if (PROJECT_FILES.every((f) => fs.existsSync(path.join(REPO, f)))) {
      say(`  브랜치 ${BRANCH} 는 없지만 ${REPO} 에 프로젝트 설정이 이미 있습니다.`);
    } else {
      fail(`브랜치 ${BRANCH} 가 없습니다. --branch=<이름> 으로 알려 주세요.`);
    }
  } else if (git('merge-base', '--is-ancestor', BRANCH, 'HEAD').code === 0) {
    say(`  ${REPO} 에는 ${BRANCH} 가 이미 들어가 있습니다.`);
  } else if (git('merge-base', '--is-ancestor', 'HEAD', BRANCH).code !== 0) {
    fail(`${REPO} main 이 ${BRANCH} 와 갈라졌습니다(앞으로만 감기 불가). Claude에게 "설정 브랜치를 main 위로 옮겨줘"라고 부탁해 주세요.`);
  } else {
    const changed = git('diff', '--name-only', 'HEAD', BRANCH).out.split(/\r?\n/).filter(Boolean);
    const dirty = git('status', '--porcelain', '--untracked-files=all', '--', ...changed).out
      .split(/\r?\n/).filter(Boolean).filter((l) => l.slice(3).trim() !== '.gitignore');
    for (const f of PROJECT_FILES) {
      if (fs.existsSync(path.join(REPO, f)) && !dirty.some((l) => l.endsWith(f))) dirty.push(`(이미 있음) ${f}`);
    }
    if (dirty.length) fail(`${REPO} 에 이 설정과 겹치는 파일이 있습니다:\n  ${dirty.join('\n  ')}\nClaude에게 이 목록을 보여 주세요.`);
    repoPlan = 'merge';
  }
}

// ---------- 2. backup ----------
const now = new Date();
const two = (n) => String(n).padStart(2, '0');
const stamp = `${now.getFullYear()}-${two(now.getMonth() + 1)}-${two(now.getDate())}-${two(now.getHours())}${two(now.getMinutes())}`;
const backupDir = path.join(CONFIG_DIR, '_reset_backup', `${stamp}-dev-setup`);
fs.mkdirSync(backupDir, { recursive: true });
for (const f of ['settings.json', 'plugins/installed_plugins.json', 'plugins/known_marketplaces.json']) {
  const src = path.join(CONFIG_DIR, f);
  if (fs.existsSync(src)) fs.copyFileSync(src, path.join(backupDir, path.basename(f)));
}
if (repoPlan === 'merge') {
  const d = spawnSync('git', ['-C', REPO, 'diff', '--', '.gitignore']);
  if (d.stdout && d.stdout.length) fs.writeFileSync(path.join(backupDir, 'A-qgis-gitignore-edit.diff'), d.stdout);
}
say(`  백업: ${backupDir}`);

// ---------- 3. plugins ----------
say('\n플러그인 설치·업데이트 (사용자 범위)...');
for (const m of MARKETPLACES) claude('plugin', 'marketplace', 'add', m); // already added = fine
if (claude('plugin', 'marketplace', 'update') !== 0) say('  (마켓플레이스 새로고침이 일부 실패했습니다. 설치는 계속합니다.)');
for (const p of PLUGINS) {
  if (claude('plugin', 'install', p.id) !== 0) fail(`${p.id} 설치에 실패했습니다. 인터넷 연결을 확인하고 다시 실행해 주세요.`);
  claude('plugin', 'update', p.id); // install never upgrades; update does
}

const installed = JSON.parse(fs.readFileSync(path.join(CONFIG_DIR, 'plugins', 'installed_plugins.json'), 'utf8')).plugins || {};
const settingsFile = path.join(CONFIG_DIR, 'settings.json');
const settings = JSON.parse(fs.readFileSync(settingsFile, 'utf8'));
// The CLI may migrate unrelated settings (seen: "model": "opus" -> "opus[1m]" in a fresh
// config). Only plugin keys are this script's business; put anything else back.
const before = fs.existsSync(path.join(backupDir, 'settings.json'))
  ? JSON.parse(fs.readFileSync(path.join(backupDir, 'settings.json'), 'utf8')) : null;
if (before) {
  const keep = new Set(['enabledPlugins', 'extraKnownMarketplaces']);
  const drift = [...new Set([...Object.keys(before), ...Object.keys(settings)])]
    .filter((k) => !keep.has(k) && JSON.stringify(before[k]) !== JSON.stringify(settings[k]));
  if (drift.length) {
    for (const k of drift) {
      if (k in before) settings[k] = before[k];
      else delete settings[k];
    }
    fs.writeFileSync(settingsFile, JSON.stringify(settings, null, 2) + '\n');
    say(`  플러그인과 무관하게 바뀐 설정을 되돌림: ${drift.join(', ')}`);
  }
}
const roots = {};
for (const p of PLUGINS) {
  const e = (installed[p.id] || []).find((x) => x.scope === 'user');
  if (!e) fail(`${p.id} 가 설치 목록에 없습니다.`);
  if (!newer(e.version, p.min)) fail(`${p.id} 버전이 ${e.version} 입니다(${p.min} 이상 필요). 다시 실행해 주세요.`);
  if (settings.enabledPlugins?.[p.id] !== true) fail(`${p.id} 가 켜져 있지 않습니다.`);
  roots[p.name] = slash(e.installPath);
  say(`  켜짐: ${p.id} ${e.version}`);
}

// ---------- 4. security-guidance venv ----------
say('\nsecurity-guidance 준비 (처음 한 번 1분 안팎)...');
const sgRoot = roots['security-guidance'];
const boot = run(BASH, [`${sgRoot}/hooks/sg-python.sh`, `${sgRoot}/hooks/ensure_agent_sdk.py`], { timeout: 400000 });
let outcome = -1;
for (const line of boot.out.split(/\r?\n/)) {
  try {
    const j = JSON.parse(line);
    if (j.metrics && typeof j.metrics.sdk_bootstrap === 'number') outcome = j.metrics.sdk_bootstrap;
  } catch { /* not JSON */ }
}
const stateDir = process.env.SECURITY_WARNINGS_STATE_DIR || path.join(CONFIG_DIR, 'security');
const venvPy = path.join(stateDir, 'agent-sdk-venv', 'Scripts', 'python.exe');
const sdkOk = [0, 7, 8].includes(outcome)
  || (fs.existsSync(venvPy) && run(venvPy, ['-c', 'import claude_agent_sdk']).code === 0);
if (sdkOk) {
  say(`  준비됨 (결과 코드 ${outcome}, ${fs.existsSync(venvPy) ? venvPy : '시스템 Python'})`);
} else {
  say(`  [주의] 커밋 정밀 검토용 Python 환경을 만들지 못했습니다(결과 코드 ${outcome}). 편집 경고는 그대로 동작합니다.`);
  if (boot.err.trim()) say(boot.err.trim().split(/\r?\n/).slice(-5).map((l) => '    ' + l).join('\n'));
}

// ---------- 5. superpowers SessionStart hook ----------
const spRoot = roots.superpowers;
const hook = run(BASH, [`${spRoot}/hooks/run-hook.cmd`, 'session-start'], {
  env: { ...process.env, CLAUDE_PLUGIN_ROOT: spRoot },
  timeout: 60000,
});
let injected = '';
try { injected = JSON.parse(hook.out).hookSpecificOutput?.additionalContext || ''; } catch { /* checked below */ }
if (hook.code !== 0 || !injected.includes('You have superpowers')) {
  fail(`superpowers 시작 훅이 동작하지 않았습니다(exit ${hook.code}). ${hook.err.trim().slice(0, 300)}`);
}
say(`\nsuperpowers 시작 훅 확인: exit 0, 안내문 ${injected.length}자 주입`);

// ---------- 6. project files into A:\qgis ----------
if (repoPlan === 'merge') {
  say(`\n${REPO} 에 프로젝트 설정 넣는 중 (${BRANCH})...`);
  if (git('diff', '--quiet', '--', '.gitignore').code !== 0) {
    const c = git('checkout', '--', '.gitignore');
    if (c.code !== 0) fail(`.gitignore 되돌리기 실패: ${c.err.trim()}`);
  }
  const m = git('merge', '--ff-only', BRANCH);
  if (m.code !== 0) fail(`git merge --ff-only 실패: ${m.err.trim()}`);
  say(`  ${REPO} main = ${git('rev-parse', '--short', 'HEAD').out.trim()}`);
}
if (!SKIP_REPO && !NO_PUSH) {
  // New desktop sessions start from GitHub's main, so CLAUDE.md must be there too.
  const ahead = git('rev-list', '--count', 'origin/main..main').out.trim();
  if (ahead !== '0') {
    say(`\nGitHub에 올리는 중 (main, 커밋 ${ahead}개)...`);
    const p = git('push', 'origin', 'main');
    if (p.code === 0) say('  GitHub main 에 올렸습니다.');
    else say(`  [주의] GitHub에 올리지 못했습니다. 새 세션이 규칙 파일을 받으려면 필요합니다. Claude에게 "main 푸시해줘"라고 부탁해 주세요.\n    ${p.err.trim().split(/\r?\n/).slice(-2).join(' ')}`);
  }
}
if (!SKIP_REPO) {
  const missing = PROJECT_FILES.filter((f) => !fs.existsSync(path.join(REPO, f)));
  if (missing.length) fail(`${REPO} 에 다음 파일이 없습니다: ${missing.join(', ')}`);
  say(`  확인: ${PROJECT_FILES.map((f) => `${REPO}/${f}`).join(', ')}`);
}

if (!process.env.ANTHROPIC_API_KEY && !process.env.ANTHROPIC_AUTH_TOKEN) {
  say('\n참고: security-guidance의 편집 경고는 켜졌습니다. 턴 끝·커밋 AI 검토는 API 키가 훅에 전달될 때만 돌고, 이 PC의 로그인 방식에서는 건너뜁니다(기록: ~/.claude/security/log.txt).');
}
say('\n완료. 지금 열려 있는 세션에는 적용되지 않습니다. Claude 앱에서 새 세션을 열어 평소처럼 부탁하세요.');
say('화면 확인: 새 세션에서 기능을 부탁하면 답 앞부분에 superpowers:brainstorming (증상이면 superpowers:systematic-debugging) 스킬을 쓰는 줄이 보입니다.');
