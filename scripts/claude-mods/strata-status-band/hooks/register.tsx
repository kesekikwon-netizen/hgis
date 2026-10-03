import { atom, read, update } from 'claude-code'
import type { Register } from 'claude-code'

import type { Status } from '../types'
import { bandLine, buildResult, isStrataDir, stageName, testResult } from './status'

const EMPTY: Status = { stage: null, build: null, tests: null }
const status = atom({ plugin: 'strata-status-band', key: 'status' } as const, EMPTY)

export const register: Register = on => {
  on('tool.call', { tool: 'Skill' }, async ($, e, next) => {
    const ran = await next(e)
    const stage = stageName(e.skill)
    if (stage && ran.deny === undefined && !ran.isError) {
      await update($, status, s => ({ ...s, stage }))
    }
    return ran
  })

  for (const tool of ['Bash', 'PowerShell'] as const) {
    on('tool.call', { tool }, async ($, e, next) => {
      const ran = await next(e)
      if (ran.deny !== undefined) return ran
      // An errored call's result is its error text or nothing, never the output record.
      const output = ran.isError ? undefined : (ran.result as { backgroundTaskId?: string; persistedOutputPath?: string } | undefined)
      // A background run has no output yet; its result is not known here.
      if (output?.backgroundTaskId) return ran
      // Long output is saved to a file and the text keeps only its head, without the ctest summary.
      const text = output?.persistedOutputPath ? await $.fs.read(output.persistedOutputPath) : (ran.text ?? '')
      const build = buildResult(e.command, text, ran.isError === true)
      const tests = testResult(e.command, text)
      if (build || tests) {
        await update($, status, s => ({ ...s, build: build ?? s.build, tests: tests ?? s.tests }))
      }
      return ran
    })
  }

  on('ui.render', { component: 'AbovePrompt' }, async ($, e, next) => {
    if (e.props.hasSurvey || !isStrataDir(await $.session.cwd())) return next(e)
    const { Text } = $.ui.resolve(e)
    return (
      <Text dimColor>
        {bandLine(await read($, status))}
      </Text>
    )
  })
}
