// Status line: model | branch | C++ gate state. Reads Claude Code's status JSON on stdin.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

let s = {};
try { s = JSON.parse(fs.readFileSync(0, "utf8") || "{}"); } catch {}
const root = s.workspace?.project_dir ?? s.workspace?.current_dir ?? process.cwd();
const model = s.model?.display_name ?? "?";

let branch = "";
try {
  const head = fs.readFileSync(path.join(root, ".git", "HEAD"), "utf8").trim();
  branch = head.startsWith("ref:") ? head.replace(/^ref:\s*refs\/heads\//, "") : head.slice(0, 7);
} catch {}

const state = path.join(path.dirname(fileURLToPath(import.meta.url)), "state");
let gate = "";
try {
  const pending = fs.readFileSync(path.join(state, "pending.txt"), "utf8").split(/\r?\n/).filter(Boolean);
  if (pending.length) {
    const done = ["graft", "clangd", "archify", "ctest"].map((n) =>
      fs.existsSync(path.join(state, n)) ? n : `!${n}`);
    gate = ` | C++ ${pending.length}개 수정 · ${done.join(" ")}`;
  }
} catch {}

process.stdout.write(`${model} | ${branch || "no-git"}${gate}`);
