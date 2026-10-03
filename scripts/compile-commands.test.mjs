// node --test scripts/compile-commands.test.mjs
// Each test writes a small CMake file-API reply into a temp build folder and runs the converter on it.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const SCRIPT = path.join(path.dirname(fileURLToPath(import.meta.url)), 'compile-commands.mjs');
const slash = (p) => p.replace(/\\/g, '/');

function project(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'compile-commands-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const src = slash(path.join(root, 'src'));
  const build = slash(path.join(root, 'build'));
  fs.mkdirSync(path.join(build, '.cmake', 'api', 'v1', 'reply'), { recursive: true });
  return { src, build, reply: path.join(build, '.cmake', 'api', 'v1', 'reply') };
}

function writeReply({ src, build, reply }) {
  const json = (name, value) => fs.writeFileSync(path.join(reply, name), JSON.stringify(value));
  json('target-core-Release-1.json', {
    name: 'core',
    sources: [
      { path: 'src/core/A.cpp', compileGroupIndex: 0 },
      { path: `${build}/core_autogen/mocs_compilation_Release.cpp`, compileGroupIndex: 0 },
      { path: 'src/core/A.h' },
    ],
    compileGroups: [{
      language: 'CXX',
      compileCommandFragments: [{ fragment: '/DWIN32 /EHsc /O2 -std:c++20 -MD' }, { fragment: '/utf-8' }],
      includes: [{ path: `${src}/src/core` }, { path: 'C:/Program Files/Qt/include', isSystem: true }],
      defines: [{ define: 'QT_CORE_LIB' }, { define: 'NAME="Strata"' }],
      sourceIndexes: [0, 1],
    }],
  });
  json('target-tests-Release-2.json', {
    name: 'tests',
    sources: [{ path: 'src/core/A.cpp', compileGroupIndex: 0 }, { path: 'tests/test_a.cpp', compileGroupIndex: 0 }],
    compileGroups: [{ language: 'CXX', compileCommandFragments: [{ fragment: '/O2' }], includes: [], sourceIndexes: [0, 1] }],
  });
  json('target-ALL_BUILD-Release-3.json', { name: 'ALL_BUILD', sources: [] });
  json('target-core-Debug-4.json', { name: 'core', sources: [{ path: 'src/core/Debug.cpp', compileGroupIndex: 0 }], compileGroups: [{ language: 'CXX', sourceIndexes: [0] }] });
  json('codemodel-v2-1.json', {
    paths: { source: src, build },
    configurations: [
      { name: 'Debug', targets: [{ name: 'core', jsonFile: 'target-core-Debug-4.json' }] },
      { name: 'Release', targets: [
        { name: 'core', jsonFile: 'target-core-Release-1.json' },
        { name: 'tests', jsonFile: 'target-tests-Release-2.json' },
        { name: 'ALL_BUILD', jsonFile: 'target-ALL_BUILD-Release-3.json' },
      ] },
    ],
  });
  json('index-2026-10-03T00-00-00-0000.json', { objects: [{ kind: 'codemodel', jsonFile: 'codemodel-v2-1.json' }] });
}

const run = (build) => spawnSync(process.execPath, [SCRIPT, build], { encoding: 'utf8' });
const entries = (build) => JSON.parse(fs.readFileSync(path.join(build, 'compile_commands.json'), 'utf8'));

test('every Release C++ source gets one cl command with its flags, includes and defines', (t) => {
  const p = project(t);
  writeReply(p);
  const r = run(p.build);
  assert.equal(r.status, 0, r.stderr);
  const list = entries(p.build);
  assert.deepEqual(list.map((e) => slash(e.file)), [
    `${p.src}/src/core/A.cpp`,
    `${p.build}/core_autogen/mocs_compilation_Release.cpp`,
    `${p.src}/tests/test_a.cpp`,
  ]);
  const a = list[0];
  assert.equal(slash(a.directory), p.build);
  assert.deepEqual(a.arguments, [
    'cl.exe', '/DWIN32', '/EHsc', '/O2', '-std:c++20', '-MD', '/utf-8',
    `/I${p.src}/src/core`, '/IC:/Program Files/Qt/include',
    '/DQT_CORE_LIB', '/DNAME="Strata"',
    '/c', `${p.src}/src/core/A.cpp`,
  ]);
});

// clangd picked the newest MSVC on the PC (VS 18) instead of the one the build uses and reported
// 13 false errors in LayerOps.cpp; pinning the build's toolset brought it to 0.
test('the build compiler pins the MSVC toolset clangd reads headers from', (t) => {
  const p = project(t);
  writeReply(p);
  const cl = 'C:/VS/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe';
  const r = spawnSync(process.execPath, [SCRIPT, p.build, cl], { encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr);
  const args = entries(p.build)[0].arguments;
  assert.equal(args[0], cl);
  assert.deepEqual(args.slice(-4), ['/vctoolsdir', 'C:/VS/2022/BuildTools/VC/Tools/MSVC/14.44.35207', '/c', `${p.src}/src/core/A.cpp`]);
});

// clangd reloads the database whenever the file changes, so an unchanged result is not rewritten.
test('an unchanged database is not rewritten, a changed one is', (t) => {
  const p = project(t);
  writeReply(p);
  run(p.build);
  const out = path.join(p.build, 'compile_commands.json');
  const old = Date.now() / 1000 - 600;
  fs.utimesSync(out, old, old);
  run(p.build);
  assert.equal(Math.round(fs.statSync(out).mtimeMs / 1000), Math.round(old));
  fs.writeFileSync(out, '[]');
  fs.utimesSync(out, Date.now() / 1000 + 600, Date.now() / 1000 + 600);
  run(p.build);
  assert.equal(entries(p.build).length, 3);
});

test('without a reply it asks CMake for one next time and never fails the build', (t) => {
  const p = project(t);
  const r = run(p.build);
  assert.equal(r.status, 0, r.stderr);
  assert.ok(fs.existsSync(path.join(p.build, '.cmake', 'api', 'v1', 'query', 'codemodel-v2')));
  assert.equal(fs.existsSync(path.join(p.build, 'compile_commands.json')), false);
});
