import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

// Records whether this session really ran the six-tool loop for C++ edits (AGENTS.md).
// A marker is set only from evidence in the command's captured output, never from the tool
// name alone (F033): ctest needs a passing summary (and a JUnit file with 0 failures when
// --output-junit was used), clangd-definition.py needs resolved locations, Archify needs a
// clean validate/deliver/visual-check run whose named diagram files exist. Cursor passes
// {command, output, duration, sandbox} to afterShellExecution; there is no exit code.
// https://cursor.com/docs/hooks

const event = process.argv[2];
const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const state = path.join(path.dirname(fileURLToPath(import.meta.url)), "state");
const pendingPath = path.join(state, "pending.txt");
const raw = fs.readFileSync(0, "utf8").trim() || "{}";

let payload = {};
try {
  payload = JSON.parse(raw);
} catch {
  payload = {};
}
const command = typeof payload.command === "string" ? payload.command : raw;
const output = typeof payload.output === "string" ? payload.output : "";

function emit(obj) {
  process.stdout.write(JSON.stringify(obj) + "\n");
}

function ensureState() {
  fs.mkdirSync(state, { recursive: true });
}

function clearState() {
  fs.rmSync(state, { recursive: true, force: true });
}

function addPending(filePath) {
  const normalized = filePath.replace(/\\\\/g, "\\");
  if (!/(^|[\\/])(src|tests)[\\/].*\.(cpp|cxx|cc|h|hpp)$/i.test(normalized)) return;
  ensureState();
  const existing = fs.existsSync(pendingPath)
    ? fs.readFileSync(pendingPath, "utf8").split(/\r?\n/).filter(Boolean)
    : [];
  if (!existing.includes(normalized)) {
    fs.appendFileSync(pendingPath, normalized + "\n", "utf8");
  }
}

// The latest run counts: ok=true writes the evidence marker and clears an earlier failure;
// ok=false records why and withdraws an earlier pass (a green ctest followed by a red one is
// not a passing record).
function record(name, ok, detail) {
  ensureState();
  const passed = path.join(state, name);
  const failed = path.join(state, name + ".failed");
  if (ok) {
    fs.writeFileSync(passed, detail || "", "utf8");
    fs.rmSync(failed, { force: true });
  } else {
    fs.rmSync(passed, { force: true });
    fs.writeFileSync(failed, detail || "", "utf8");
  }
}

function pendingLines() {
  if (!fs.existsSync(pendingPath)) return [];
  return fs.readFileSync(pendingPath, "utf8").split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
}

function resolveFrom(base, file) {
  const clean = file.replace(/^["']|["']$/g, "");
  return path.isAbsolute(clean) ? clean : path.resolve(base, clean);
}

function ctestEvidence() {
  const summary = output.match(/(\d+)% tests passed,\s*(\d+) tests? failed out of (\d+)/i);
  if (!summary) {
    return { ok: false, detail: "ctest printed no pass/fail summary (not run, or output not captured)" };
  }
  const failedCount = Number(summary[2]);
  const total = Number(summary[3]);
  if (total === 0) return { ok: false, detail: "ctest ran 0 tests" };
  if (failedCount > 0) return { ok: false, detail: `ctest: ${failedCount} of ${total} tests failed` };
  const junit = command.match(/--output-junit\s+("[^"]+"|\S+)/i);
  if (junit) {
    const testDir = command.match(/--test-dir\s+("[^"]+"|\S+)/i);
    const base = testDir ? resolveFrom(repoRoot, testDir[1]) : repoRoot;
    const file = resolveFrom(base, junit[1]);
    if (!fs.existsSync(file)) return { ok: false, detail: `ctest JUnit file missing: ${file}` };
    const xml = fs.readFileSync(file, "utf8");
    const suite = xml.match(/<testsuite\b[^>]*>/i)?.[0] ?? "";
    const count = (attr) => Number(suite.match(new RegExp(`\\b${attr}="(\\d+)"`, "i"))?.[1] ?? 0);
    if (count("tests") === 0 || count("failures") > 0 || count("errors") > 0) {
      return { ok: false, detail: `ctest JUnit ${file}: tests=${count("tests")} failures=${count("failures")} errors=${count("errors")}` };
    }
  }
  return { ok: true, detail: `ctest ${total} tests passed` };
}

function clangdEvidence() {
  if (/clangd-definition:\s/.test(output) || /Traceback \(most recent call last\)/.test(output)) {
    return { ok: false, detail: "clangd-definition.py reported an error" };
  }
  if (!/"locations"\s*:\s*\[\s*\{[\s\S]*?"file"\s*:/.test(output)) {
    return { ok: false, detail: "clangd-definition.py printed no resolved location" };
  }
  return { ok: true, detail: "clangd-definition.py resolved a location" };
}

function archifyEvidence() {
  if (!/\b(validate|deliver|visual-check)\b/i.test(command)) {
    return { ok: false, detail: "archify.ps1 ran without validate/deliver/visual-check" };
  }
  if (/(^|\n)\s*(FAIL|ERROR)\b|\bError:/.test(output)) {
    return { ok: false, detail: "archify.ps1 reported a failure" };
  }
  const named = [...command.matchAll(/("[^"]+\.(?:html|json)"|\S+\.(?:html|json))/gi)].map((m) => m[1]);
  const absent = named.map((file) => resolveFrom(repoRoot, file)).filter((file) => !fs.existsSync(file));
  if (absent.length > 0) return { ok: false, detail: `archify diagram file missing: ${absent.join(", ")}` };
  return { ok: true, detail: "archify.ps1 check passed" };
}

if (event === "sessionStart") {
  clearState();
  emit({});
} else if (event === "afterFileEdit") {
  for (const match of raw.matchAll(/(?:file_path|path|uri|filePath)"\s*:\s*"([^"]+)"/gi)) {
    addPending(match[1]);
  }
  emit({});
} else if (event === "afterShellExecution") {
  if (/clangd-definition\.py/.test(command)) {
    const r = clangdEvidence();
    record("clangd", r.ok, r.detail);
  }
  if (/archify\.ps1/.test(command)) {
    const r = archifyEvidence();
    record("archify", r.ok, r.detail);
  }
  if (/(^|[^A-Za-z])ctest([^A-Za-z]|$)/.test(command)) {
    const r = ctestEvidence();
    record("ctest", r.ok, r.detail);
  }
  // hgis_graft MCP가 이 세션에 없어도, 프로젝트 서버 스크립트 조회는 Graft 기록이다.
  if (/graft-mcp\.mjs|graft_find_|graft_file_api|graft_check_freshness/.test(command) && output.trim()) {
    record("graft", true, "graft server script answered");
  }
  emit({});
} else if (event === "afterMCPExecution") {
  if (/graft_/.test(raw) && !/"error"\s*:/i.test(raw)) record("graft", true, "hgis_graft MCP call");
  emit({});
} else if (event === "stop") {
  const status = typeof payload.status === "string" ? payload.status : "completed";
  const loop = Number(payload.loop_count ?? 0);
  const lines = pendingLines();
  if (status !== "completed" || lines.length === 0 || loop >= 2) {
    if (lines.length === 0) clearState();
    emit({});
  } else if (process.platform !== "win32") {
    clearState();
    emit({
      followup_message:
        "This cloud VM is Ubuntu and has no OSGeo4W or Visual Studio. Do not configure, build, ctest, or smoke ka-hgis here, and do not report those as passed. Windows CTest and smoke remain the verification gate.",
    });
  } else {
    const names = ["graft", "clangd", "archify", "ctest"];
    const missing = names.filter((name) => !fs.existsSync(path.join(state, name)));
    if (missing.length === 0) {
      clearState();
      emit({});
    } else {
      const reasons = missing
        .map((name) => {
          const failed = path.join(state, name + ".failed");
          return fs.existsSync(failed) ? `${name} (${fs.readFileSync(failed, "utf8").trim()})` : `${name} (not run)`;
        })
        .join("; ");
      emit({
        followup_message:
          "C++ under src/ or tests changed, but this session has no passing execution record for: " +
          reasons +
          ". Run them now on that change and put the input plus the result in the reply. Graft is hgis_graft. clangd is scripts/clangd-definition.py. Archify is scripts/archify.ps1 validate. Tests are the matching ctest, and it has to pass (a failed or empty run does not count). Naming a tool is not evidence.",
      });
    }
  }
} else {
  emit({});
}
