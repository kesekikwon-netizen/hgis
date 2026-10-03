#!/usr/bin/env node
// Writes <build>/compile_commands.json for clangd from CMake's file-API reply.
// The Visual Studio generator ignores CMAKE_EXPORT_COMPILE_COMMANDS, so the C++ code tool (clangd-lsp)
// had no compiler database in a fresh worktree. CMakeLists.txt and build-now.ps1 leave a codemodel query
// in the build folder; every cmake run then writes a reply, and this script (run by the build) turns the
// Release configuration into one cl.exe command per source. Never fails the build: problems are printed.
//   node scripts/compile-commands.mjs <build-dir> [<path of the build's cl.exe>]
// User decision 2026-10-03, docs/intent/2026-10-03-dev-setup-gaps.md.
import fs from 'node:fs';
import path from 'node:path';

const CONFIG = 'Release';

// With the build's cl.exe (…/VC/Tools/MSVC/<ver>/bin/Hostx64/x64/cl.exe) clangd is told which MSVC
// toolset to read headers from; otherwise it takes the newest one on the PC (here VS 18, wrong STL).
function compilerArgs(compiler) {
  if (!compiler) return { program: 'cl.exe', pin: [] };
  const toolset = compiler.replace(/\\/g, '/').replace(/\/bin\/[^/]+\/[^/]+\/cl\.exe$/i, '');
  return { program: compiler, pin: toolset !== compiler.replace(/\\/g, '/') ? ['/vctoolsdir', toolset] : [] };
}

function main(buildDir, compiler) {
  const cl = compilerArgs(compiler);
  const api = path.join(buildDir, '.cmake', 'api', 'v1');
  const query = path.join(api, 'query', 'codemodel-v2');
  if (!fs.existsSync(query)) {
    fs.mkdirSync(path.dirname(query), { recursive: true });
    fs.writeFileSync(query, '');
  }
  const reply = path.join(api, 'reply');
  const index = fs.existsSync(reply) && fs.readdirSync(reply).filter((f) => /^index-.*\.json$/.test(f)).sort().pop();
  if (!index) {
    console.log('compile_commands.json: CMake has not answered the codemodel query yet; it will after the next cmake run.');
    return;
  }
  const out = path.join(buildDir, 'compile_commands.json');
  const read = (name) => JSON.parse(fs.readFileSync(path.join(reply, name), 'utf8'));
  const codemodel = read(read(index).objects.find((o) => o.kind === 'codemodel').jsonFile);
  const { source, build } = codemodel.paths;
  const config = codemodel.configurations.find((c) => c.name === CONFIG);
  const seen = new Set();
  const list = [];
  for (const ref of config ? config.targets : []) {
    const target = read(ref.jsonFile);
    const groups = target.compileGroups || [];
    for (const src of target.sources || []) {
      const group = groups[src.compileGroupIndex];
      if (!group || !['C', 'CXX'].includes(group.language)) continue;
      const file = path.isAbsolute(src.path) ? src.path : `${source}/${src.path}`;
      if (seen.has(file)) continue;
      seen.add(file);
      const flags = (group.compileCommandFragments || []).flatMap((f) => f.fragment.match(/(?:[^\s"]+|"[^"]*")+/g) || []);
      list.push({
        directory: build,
        file,
        arguments: [
          cl.program,
          ...flags,
          ...(group.includes || []).map((i) => `/I${i.path}`),
          ...(group.defines || []).map((d) => `/D${d.define}`),
          ...cl.pin,
          '/c',
          file,
        ],
      });
    }
  }
  // clangd reloads the database whenever the file changes: write only when the content differs.
  const text = JSON.stringify(list, null, 1);
  if (fs.existsSync(out) && fs.readFileSync(out, 'utf8') === text) return;
  fs.writeFileSync(out, text);
  console.log(`compile_commands.json: ${list.length} sources (${CONFIG})`);
}

try {
  main(path.resolve(process.argv[2] || 'build'), process.argv[3]);
} catch (e) {
  console.log(`compile_commands.json not written: ${e.message}`);
}
