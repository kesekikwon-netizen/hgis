import { describe, expect, test } from 'claude-code/testing'

import { bandLine, buildResult, isStrataDir, stageName, testResult } from '../hooks/status'

const BAND = {
  component: 'AbovePrompt',
  props: {
    hasSurvey: false,
    isWorking: false,
    maxRows: 10,
    bodyColumns: 100,
    scroll: { offset: 0, bodyRows: 10 },
    view: {},
  },
} as const

const BUILD = `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; cmake --build build --config Release --parallel --target ka-hgis test_save_open; exit $LASTEXITCODE'`
const CTEST = `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command '. ./scripts/dev-env.ps1; ctest --test-dir build -C Release -j4 --output-on-failure -R "^(save_open)$"; exit $LASTEXITCODE'`

describe('stageName', () => {
  test('names the Strata working stages in Korean', () => {
    expect(stageName('superpowers:systematic-debugging')).toBe('원인 찾기')
    expect(stageName('superpowers:test-driven-development')).toBe('시험 먼저')
    expect(stageName('superpowers:verification-before-completion')).toBe('끝나기 전 확인')
    expect(stageName('superpowers:requesting-code-review')).toBe('코드 검토')
    expect(stageName('capture-intent')).toBe('요청 기록')
    expect(stageName('run-strata')).toBe('앱 화면 확인')
  })

  test('ignores skills that are not a working stage', () => {
    expect(stageName('plugin-authoring')).toBeUndefined()
    expect(stageName('anthropic-skills:pdf')).toBeUndefined()
  })
})

describe('buildResult', () => {
  test('reads a finished build', () => {
    expect(buildResult(BUILD, 'ka-hgis.vcxproj -> A:\\qgis\\build\\Release\\ka-hgis.exe', false)).toBe('ok')
    expect(buildResult(BUILD, 'error C2065: undeclared identifier', true)).toBe('fail')
  })

  test('counts a build as passed when ctest ran after it in the same command', () => {
    const text = '92% tests passed, 1 tests failed out of 12'
    expect(buildResult(`${BUILD}; ${CTEST}`, text, true)).toBe('ok')
  })

  test('reads the first build script', () => {
    expect(buildResult('powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1', '', false)).toBe('ok')
  })

  test('ignores commands that are not a build', () => {
    expect(buildResult('git status', '', false)).toBeUndefined()
  })
})

describe('testResult', () => {
  test('reads the ctest summary', () => {
    expect(testResult(CTEST, '100% tests passed, 0 tests failed out of 12\n\nTotal Test time')).toEqual({ passed: 12, total: 12 })
    expect(testResult(CTEST, '92% tests passed, 1 tests failed out of 12')).toEqual({ passed: 11, total: 12 })
  })

  test('ignores output without a summary', () => {
    expect(testResult(CTEST, 'No tests were found!!!')).toBeUndefined()
    expect(testResult('git log', '100% tests passed, 0 tests failed out of 3')).toBeUndefined()
  })
})

describe('bandLine', () => {
  test('shows dashes before anything ran', () => {
    expect(bandLine({ stage: null, build: null, tests: null })).toBe('단계 — · 빌드 — · 시험 —')
  })

  test('shows the stage, build and tests', () => {
    expect(bandLine({ stage: '시험 먼저', build: 'ok', tests: { passed: 12, total: 12 } })).toBe('시험 먼저 · 빌드 ✓ · 시험 12/12 ✓')
    expect(bandLine({ stage: '원인 찾기', build: 'fail', tests: { passed: 11, total: 12 } })).toBe('원인 찾기 · 빌드 ✗ · 시험 11/12 ✗ (1개 실패)')
  })
})

describe('isStrataDir', () => {
  test('matches the Strata checkout and folders inside it', () => {
    expect(isStrataDir('A:\\qgis')).toBe(true)
    expect(isStrataDir('a:/qgis/.claude/worktrees/x')).toBe(true)
  })

  test('rejects other folders', () => {
    expect(isStrataDir('C:\\Users\\me\\project')).toBe(false)
    expect(isStrataDir('A:\\qgis-old')).toBe(false)
  })
})

test('the band follows skill, build and test calls', async ($, on) => {
  on('session.cwd', () => ({ value: 'A:/qgis' }))
  on('tool.call', ($, e) => {
    if (e.tool === 'Skill') return { result: { success: true, commandName: 'test-driven-development' } }
    const text = String(e.command).includes('ctest') ? '100% tests passed, 0 tests failed out of 3' : 'ka-hgis.exe'
    return { result: { stdout: '', stderr: '', interrupted: false }, text }
  })

  await $.tool.call({ tool: 'Skill', skill: 'superpowers:test-driven-development' })
  await $.tool.call({ tool: 'Bash', command: BUILD })
  await $.tool.call({ tool: 'Bash', command: CTEST })

  for (const surface of ['terminal', 'desktop'] as const) {
    const ui = await $.ui.mount({ plugin: 'strata-status-band', surface, ...BAND })
    expect((await ui.find({ type: 'Text' }))?.text).toBe('시험 먼저 · 빌드 ✓ · 시험 3/3 ✓')
    await ui.unmount()
  }
})

test('a build sent to the background leaves the build result alone', async ($, on) => {
  on('session.cwd', () => ({ value: 'A:/qgis' }))
  on('tool.call', () => ({ result: { stdout: '', stderr: '', interrupted: false, backgroundTaskId: 'b1' }, text: 'Command running in background' }))

  await $.tool.call({ tool: 'Bash', command: BUILD })

  const ui = await $.ui.mount({ plugin: 'strata-status-band', surface: 'terminal', ...BAND })
  expect((await ui.find({ type: 'Text' }))?.text).toBe('단계 — · 빌드 — · 시험 —')
  await ui.unmount()
})

test('a failed build with no stored output shows the build as failed', async ($, on) => {
  on('session.cwd', () => ({ value: 'A:/qgis' }))
  on('tool.call', () => ({ result: undefined, text: 'Exit code 1', isError: true }))

  await $.tool.call({ tool: 'Bash', command: BUILD })

  const ui = await $.ui.mount({ plugin: 'strata-status-band', surface: 'terminal', ...BAND })
  expect((await ui.find({ type: 'Text' }))?.text).toBe('단계 — · 빌드 ✗ · 시험 —')
  await ui.unmount()
})

test('a long test run saved to a file is read from that file', async ($, on) => {
  on('session.cwd', () => ({ value: 'A:/qgis' }))
  on('fs.read', ($, e) => ({ value: e.path.replace(/\\/g, '/') === 'C:/out/ctest.txt' ? 'lots of output\n100% tests passed, 0 tests failed out of 7\n' : '' }))
  on('tool.call', () => ({
    result: { stdout: 'lots of', stderr: '', interrupted: false, persistedOutputPath: 'C:/out/ctest.txt', persistedOutputSize: 99999 },
    text: 'lots of',
  }))

  await $.tool.call({ tool: 'PowerShell', command: CTEST })

  const ui = await $.ui.mount({ plugin: 'strata-status-band', surface: 'terminal', ...BAND })
  expect((await ui.find({ type: 'Text' }))?.text).toBe('단계 — · 빌드 — · 시험 7/7 ✓')
  await ui.unmount()
})
