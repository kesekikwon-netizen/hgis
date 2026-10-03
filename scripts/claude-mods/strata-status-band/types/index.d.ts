export type Build = 'ok' | 'fail'
export type Tests = { passed: number; total: number }
export type Status = { stage: string | null; build: Build | null; tests: Tests | null }

declare module 'claude-code' {
  interface PluginState {
    'strata-status-band': { status: Status }
  }
}
