#!/usr/bin/env node
// Hard guards for ka-hgis rules that must not depend on the model remembering them.
// pre:  PreToolUse, deny or ask before the tool runs.
// post: PostToolUse, a note when a hotspot file grows.
// settings.json permission rules match only the Bash tool; this hook also covers PowerShell.
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

const mode = process.argv[2] || "pre";
let input = {};
try { input = JSON.parse(fs.readFileSync(0, "utf8") || "{}"); } catch { process.exit(0); }
const tool = input.tool_name || "";
const ti = input.tool_input || {};
const projectDir = process.env.CLAUDE_PROJECT_DIR || input.cwd || process.cwd();

// Test fixtures that scripts/secret-scan.py also allows. Any other GUID literal looks like a real key.
const FAKE_GUIDS = new Set([
  "11111111-2222-3333-4444-555555555555", "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE",
  "00000000-1111-2222-3333-444455556666", "11112222-3333-4444-5555-666677778888",
]);
const GUID_RE = /\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\b/gi;
const TEXT_FILE = /\.(cpp|h|hpp|ini|json|js|mjs|ps1|py|qss|md|txt|xml|qgs|cmake)$/i;
const HOTSPOTS = ["src/app/MainWindow.cpp", "src/core/LayerOps.cpp"];

// Repo-relative path with "/" separators. Worktrees sit under .claude/worktrees/<name>/,
// so directory checks match a segment anywhere in the path.
const rel = (p) => (p ? path.relative(projectDir, path.resolve(projectDir, p)).split(path.sep).join("/") : "");
const under = (r, dir) => new RegExp(`(^|/)${dir}/`).test(r);

function newText() {
  if (typeof ti.content === "string") return ti.content;
  if (typeof ti.new_string === "string") return ti.new_string;
  if (Array.isArray(ti.edits)) return ti.edits.map((e) => e.new_string || "").join("\n");
  if (typeof ti.new_source === "string") return ti.new_source;
  return "";
}

function decide(decision, reason) {
  process.stdout.write(JSON.stringify({
    hookSpecificOutput: { hookEventName: "PreToolUse", permissionDecision: decision, permissionDecisionReason: reason },
  }) + "\n");
  process.exit(0);
}

function checkEdit() {
  const r = rel(ti.file_path || ti.notebook_path);
  // Only repository files are guarded. Scratch files elsewhere may legitimately contain GUID-shaped ids.
  if (!r || r.startsWith("..") || path.isAbsolute(r)) return;
  const lower = r.toLowerCase();
  const text = newText();
  if (/appdata\/local\/ka-hgis\//.test(lower) ||
      /(^|\/)config\/(secrets\.ini|[^/]*-local\.ini|[^/]*-account\.ini)$/.test(lower))
    decide("deny", "[guard] 사용자 계정·설정 파일은 고치지 않는다. 필요하면 사용자에게 요청한다.");
  if (under(r, "src") && /\bremoveAllMapLayers\s*\(/.test(text))
    decide("deny", "[guard] AGENTS.md 불변식: removeAllMapLayers()를 부르지 않는다. 도메인 레이어만 내리고 배경·참조 레이어는 남긴다.");
  if (!TEXT_FILE.test(r)) return;
  const guids = (text.match(GUID_RE) || []).filter((g) => !FAKE_GUIDS.has(g.toUpperCase()));
  if (guids.length === 0) return;
  if (under(r, "tests"))
    decide("ask", `[guard] 시험 파일에 새 GUID(${guids[0]})가 들어간다. 실제 VWorld 키가 아닌 가짜 값인지 확인하고, 가짜면 scripts/secret-scan.py 허용 목록에도 넣는다.`);
  decide("deny", `[guard] 키 모양 GUID(${guids[0].slice(0, 8)}…)를 저장소 파일에 쓰지 않는다. VWorld 키는 VworldSettings·로컬 설정·config/secrets.ini(git 제외)에만 둔다.`);
}

// A segment that only reads (grep, cat, sed, Select-String, git show …) may name a guarded script
// or process without running it. Ask only when some segment actually invokes it.
const READ_ONLY = /^\s*(grep|rg|egrep|fgrep|cat|sed|head|tail|awk|less|more|type|findstr|wc|diff|sort|uniq|cut|tr|echo|printf|ls|dir|find|stat|file|select-string|sls|get-content|gc|get-item|gi|get-childitem|test-path|resolve-path|git\s+(diff|show|log|grep|status|blame|ls-files))\b/i;
function runsIn(cmd, re) {
  return cmd.split(/&&|\|\||;|\|/).some((seg) => re.test(seg) && !READ_ONLY.test(seg));
}

function checkShell() {
  const c = String(ti.command || "");
  const lc = c.toLowerCase();
  if (/\bgit\b[^\n;|&]*\bpush\b[^\n;|&]*(--force\b|--force-with-lease\b|\s-f\b|\s\+\S)/i.test(c))
    decide("deny", "[guard] 강제 push 금지(AGENTS.md Git 규칙).");
  if (/auto-git-push|install-auto-push-task|run-auto-push-now/.test(lc) ||
      (/register-scheduledtask/.test(lc) && /git/.test(lc)))
    decide("deny", "[guard] 숨은 자동 push·예약 push는 도입하지 않는다(AGENTS.md).");
  if (/(remove-item|\brm\b|\bdel\b|rmdir)[^\n;|]*appdata[\\/]+local[\\/]+ka-hgis/i.test(c))
    decide("deny", "[guard] 사용자 앱 데이터(%LOCALAPPDATA%\\ka-hgis)는 지우지 않는다.");
  if (runsIn(lc, /(publish-desktop|make-portable|sign-release)\.ps1/))
    decide("ask", "[guard] 포터블 생성·배포·서명은 사용자가 명시적으로 요청할 때만 한다(AGENTS.md).");
  if (runsIn(lc, /(stop-process|taskkill|\bkill\b)[^\n;|]*ka-hgis/) ||
      (runsIn(lc, /get-process[^\n|]*ka-hgis/) && /stop-process|\.kill\(|closemainwindow/.test(lc)))
    decide("ask", "[guard] 사용자가 실행 중인 앱은 요청 없이 닫거나 재시작하지 않는다(AGENTS.md).");
  // The Bash "ask" permission rules do not apply to the PowerShell tool; mirror them here.
  if (tool === "PowerShell" &&
      /\bgit\b(\s+-c\s+\S+)*\s+(commit|push|merge|rebase|reset|checkout|restore|stash|clean)\b/i.test(c))
    decide("ask", "[guard] 커밋·push·되돌리기 계열 git 명령은 사용자가 요청할 때만 한다.");
}

function hotspotNote() {
  const r = rel(ti.file_path);
  const hit = HOTSPOTS.find((h) => r === h || r.endsWith("/" + h));
  if (!hit) return;
  const abs = path.resolve(projectDir, ti.file_path);
  const repoRoot = abs.slice(0, abs.length - hit.length).replace(/[\\/]+$/, "");
  let before = 0;
  let now = 0;
  try {
    before = execFileSync("git", ["-C", repoRoot, "show", `HEAD:${hit}`],
      { encoding: "utf8", maxBuffer: 64 << 20, stdio: ["ignore", "pipe", "ignore"] }).split("\n").length;
    now = fs.readFileSync(abs, "utf8").split("\n").length;
  } catch { return; }
  if (now <= before) return;
  process.stdout.write(JSON.stringify({
    hookSpecificOutput: {
      hookEventName: "PostToolUse",
      additionalContext: `[guard] 핫스팟 ${hit}가 HEAD보다 ${now - before}줄 늘었다(${before}→${now}). ` +
        "AGENTS.md: 핫스팟을 키우지 말고 새 개념은 작은 파일로 뺀다. 한두 줄 호출 추가가 아니면 옮긴다.",
    },
  }) + "\n");
}

if (mode === "post") hotspotNote();
else if (/^(Edit|Write|MultiEdit|NotebookEdit)$/.test(tool)) checkEdit();
else if (/^(Bash|PowerShell)$/.test(tool)) checkShell();
process.exit(0);
