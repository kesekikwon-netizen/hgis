import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const event = process.argv[2];
const state = path.join(path.dirname(fileURLToPath(import.meta.url)), "state");
const pendingPath = path.join(state, "pending.txt");
const raw = fs.readFileSync(0, "utf8").trim() || "{}";

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

function setMarker(name) {
  ensureState();
  fs.writeFileSync(path.join(state, name), "", "utf8");
}

function pendingLines() {
  if (!fs.existsSync(pendingPath)) return [];
  return fs.readFileSync(pendingPath, "utf8").split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
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
  if (/clangd-definition\.py/.test(raw)) setMarker("clangd");
  if (/archify\.ps1/.test(raw)) setMarker("archify");
  if (/(^|[^A-Za-z])ctest([^A-Za-z]|$)/.test(raw)) setMarker("ctest");
  emit({});
} else if (event === "afterMCPExecution") {
  if (/graft_/.test(raw)) setMarker("graft");
  emit({});
} else if (event === "stop") {
  const status = raw.match(/"status"\s*:\s*"([^"]+)"/)?.[1] ?? "completed";
  const loop = Number(raw.match(/"loop_count"\s*:\s*(\d+)/)?.[1] ?? 0);
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
    const missing = ["graft", "clangd", "archify", "ctest"].filter(
      (name) => !fs.existsSync(path.join(state, name)),
    );
    if (missing.length === 0) {
      clearState();
      emit({});
    } else {
      emit({
        followup_message:
          "C++ under src/ or tests changed, but this session has no execution record. Missing: " +
          missing.join(", ") +
          ". Run them now on that change and put the input plus the result in the reply. Graft is hgis_graft. clangd is scripts/clangd-definition.py. Archify is scripts/archify.ps1 validate. Tests are the matching ctest. Naming a tool is not evidence.",
      });
    }
  }
} else {
  emit({});
}
