// Claude Code port of .cursor/hooks/dev-loop.mjs.
// Records which of the four required tools (Graft, clangd, Archify, CTest) ran after
// C++ under src/ or tests/ changed, and blocks Stop until they have.
// Events: sessionStart | postEdit | postShell | postMcp | stop
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const event = process.argv[2];
const state = path.join(path.dirname(fileURLToPath(import.meta.url)), "state");
const pendingPath = path.join(state, "pending.txt");
const blockCountPath = path.join(state, "stop-blocks");
const MAX_BLOCKS = 2; // same loop_limit as the Cursor hook

let input = {};
try {
  const raw = fs.readFileSync(0, "utf8").trim();
  input = raw ? JSON.parse(raw) : {};
} catch {
  input = {};
}

const ensureState = () => fs.mkdirSync(state, { recursive: true });
const clearState = () => fs.rmSync(state, { recursive: true, force: true });
const setMarker = (name) => { ensureState(); fs.writeFileSync(path.join(state, name), ""); };
const has = (name) => fs.existsSync(path.join(state, name));

function pendingLines() {
  if (!fs.existsSync(pendingPath)) return [];
  return fs.readFileSync(pendingPath, "utf8").split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
}

function addPending(filePath) {
  if (!filePath) return;
  const normalized = String(filePath).replace(/\\\\/g, "\\");
  if (!/(^|[\\/])(src|tests)[\\/].*\.(cpp|cxx|cc|h|hpp)$/i.test(normalized)) return;
  ensureState();
  const existing = pendingLines();
  if (!existing.includes(normalized)) fs.appendFileSync(pendingPath, normalized + "\n", "utf8");
}

const toolInput = input.tool_input ?? {};

switch (event) {
  case "sessionStart": {
    clearState();
    // stdout of SessionStart is added to Claude's context.
    process.stdout.write(
      "ka-hgis: 비자명한 작업 전 .codex/NOW.md 맨 위와 docs/HANDOFF.md를 확인한다. " +
      "C++(src/, tests/) 수정 시 Graft·clangd·Archify·CTest 실행 기록이 있어야 Stop 훅을 통과한다.\n",
    );
    break;
  }
  case "postEdit": {
    addPending(toolInput.file_path ?? toolInput.notebook_path ?? toolInput.path);
    break;
  }
  case "postShell": {
    const cmd = String(toolInput.command ?? "");
    if (/clangd-definition\.py/.test(cmd)) setMarker("clangd");
    if (/archify\.ps1/.test(cmd)) setMarker("archify");
    if (/(^|[^A-Za-z_])ctest([^A-Za-z_]|$)/.test(cmd)) setMarker("ctest");
    if (/graft-mcp\.mjs|graft_find_|graft_file_api|graft_check_freshness/.test(cmd)) setMarker("graft");
    break;
  }
  case "postMcp": {
    if (/graft_/.test(String(input.tool_name ?? ""))) setMarker("graft");
    break;
  }
  case "stop": {
    const lines = pendingLines();
    if (lines.length === 0) { clearState(); break; }
    const blocks = has("stop-blocks") ? Number(fs.readFileSync(blockCountPath, "utf8")) || 0 : 0;
    if (blocks >= MAX_BLOCKS) { clearState(); break; } // give up quietly; never loop forever

    let reason;
    if (process.platform !== "win32") {
      reason =
        "이 환경은 Windows가 아니다(OSGeo4W·Visual Studio 없음). ka-hgis를 configure·build·ctest·smoke하지 말고, " +
        "그것들이 통과했다고 보고하지 않는다. Windows PC의 CTest와 smoke가 검증 게이트로 남았다고 답에 적는다.";
      clearState();
    } else {
      const missing = ["graft", "clangd", "archify", "ctest"].filter((n) => !has(n));
      if (missing.length === 0) { clearState(); break; }
      reason =
        "src/ 또는 tests/의 C++가 바뀌었는데 이 세션에 실행 기록이 없다. 빠진 것: " + missing.join(", ") + ". " +
        "바뀐 파일: " + lines.slice(0, 8).join(", ") + ". " +
        "지금 그 변경에 대해 실행하고, 입력과 결과를 답에 적어라. Graft=mcp__hgis_graft__* 도구, " +
        "clangd=python scripts/clangd-definition.py <file> <line> <col>, Archify=scripts/archify.ps1 validate, " +
        "테스트=맞는 ctest -R. 도구 이름만 적는 것은 증거가 아니다. 도구를 쓸 수 없으면 '검증 미완료'와 이유를 보고한다.";
      ensureState();
      fs.writeFileSync(blockCountPath, String(blocks + 1));
    }
    process.stdout.write(JSON.stringify({ decision: "block", reason }) + "\n");
    break;
  }
  default:
    break;
}
process.exit(0);
