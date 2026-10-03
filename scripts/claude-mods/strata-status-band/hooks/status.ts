import type { Build, Status, Tests } from '../types'

// Skill name (after any "plugin:" prefix) → the stage name CLAUDE.md uses.
const STAGES: Record<string, string> = {
  'systematic-debugging': '원인 찾기',
  'test-driven-development': '시험 먼저',
  'verification-before-completion': '끝나기 전 확인',
  'requesting-code-review': '코드 검토',
  'receiving-code-review': '검토 반영',
  brainstorming: '설계 논의',
  'writing-plans': '계획 쓰기',
  'executing-plans': '계획대로 만들기',
  'finishing-a-development-branch': '마무리',
  'capture-intent': '요청 기록',
  'run-strata': '앱 화면 확인',
}

const CTEST_SUMMARY = /\d+% tests passed, (\d+) tests failed out of (\d+)/

export const stageName = (skill: string): string | undefined => STAGES[skill.slice(skill.lastIndexOf(':') + 1)]

export const testResult = (command: string, text: string): Tests | undefined => {
  if (!command.includes('ctest')) return undefined
  const m = CTEST_SUMMARY.exec(text)
  if (!m) return undefined
  const failed = Number(m[1])
  const total = Number(m[2])
  return { passed: total - failed, total }
}

export const buildResult = (command: string, text: string, isError: boolean): Build | undefined => {
  if (!command.includes('cmake --build') && !command.includes('build-now.ps1')) return undefined
  // A ctest summary means the build before it finished; a failing test is not a failed build.
  if (!isError || testResult(command, text)) return 'ok'
  return 'fail'
}

export const bandLine = ({ stage, build, tests }: Status): string => {
  const buildText = build === null ? '빌드 —' : build === 'ok' ? '빌드 ✓' : '빌드 ✗'
  let testText = '시험 —'
  if (tests !== null) {
    const failed = tests.total - tests.passed
    testText = failed === 0 ? `시험 ${tests.passed}/${tests.total} ✓` : `시험 ${tests.passed}/${tests.total} ✗ (${failed}개 실패)`
  }
  return `${stage ?? '단계 —'} · ${buildText} · ${testText}`
}

export const isStrataDir = (cwd: string): boolean => {
  const dir = cwd.replace(/\\/g, '/').toLowerCase()
  return dir === 'a:/qgis' || dir.startsWith('a:/qgis/')
}
