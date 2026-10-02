// node --test scripts/jev/jev-ask.test.mjs
// 개발 도구에서 Jev(TypeSafe System One) 판정을 부르는 도우미. 실제 서버는 부르지 않고 가짜 fetch 로 시험한다.
import { test } from 'node:test';
import assert from 'node:assert/strict';

import { ask, loadKey, MODEL } from './jev-ask.mjs';

const KEY = 'test-key-not-real';
const QUESTIONS = { real: { type: 'noul', instructions: 'Is this finding a real defect?' } };

const reply = (status, body) => ({ ok: status >= 200 && status < 300, status, json: async () => body });
const fakeFetch = (replies) => {
  const calls = [];
  const fetchImpl = async (url, init) => {
    calls.push({ url, init, body: JSON.parse(init.body) });
    return replies[Math.min(calls.length - 1, replies.length - 1)];
  };
  return { calls, fetchImpl };
};

test('보낸 요청은 고정한 모델·키·질문을 담고, 답과 비용을 돌려준다', async () => {
  const { calls, fetchImpl } = fakeFetch([
    reply(200, { model: MODEL, answers: { real: { type: 'noul', noul: 0.91 } }, usage: { input_tokens: 2000, output_tokens: 10 } }),
  ]);
  const r = await ask({ state: { finding: 'x' }, questions: QUESTIONS }, { key: KEY, fetchImpl, backoffMs: 0 });
  assert.equal(r.ok, true);
  assert.equal(r.answers.real.noul, 0.91);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].url, 'https://api.typesafe.ai/v1/systemone');
  assert.equal(calls[0].init.headers.authorization, ['Bearer', KEY].join(' '));
  assert.deepEqual(calls[0].body, { model: MODEL, state: { finding: 'x' }, questions: QUESTIONS });
  assert.ok(Math.abs(r.costUsd - 2000 * 0.042e-6) < 1e-12);
});

test('키가 없으면 부르지 않고 no-key 로 알린다', async () => {
  const { calls, fetchImpl } = fakeFetch([reply(200, {})]);
  const r = await ask({ state: 'x', questions: QUESTIONS }, { key: '', fetchImpl });
  assert.equal(r.ok, false);
  assert.equal(r.reason, 'no-key');
  assert.equal(calls.length, 0);
});

test('서버가 바쁘면(529) 다시 부르고, 키가 틀리면(401) 다시 부르지 않으며 결과에 키를 쓰지 않는다', async () => {
  const busy = fakeFetch([reply(529, {}), reply(200, { model: MODEL, answers: {}, usage: { input_tokens: 1 } })]);
  assert.equal((await ask({ state: 'x', questions: QUESTIONS }, { key: KEY, fetchImpl: busy.fetchImpl, backoffMs: 0 })).ok, true);
  assert.equal(busy.calls.length, 2);

  const denied = fakeFetch([reply(401, { detail: { error_type: 'authentication_error', message: 'Cannot authenticate' } })]);
  const r = await ask({ state: 'x', questions: QUESTIONS }, { key: KEY, fetchImpl: denied.fetchImpl, backoffMs: 0 });
  assert.equal(r.ok, false);
  assert.equal(r.reason, 'http-401');
  assert.equal(denied.calls.length, 1);
  assert.ok(!JSON.stringify(r).includes(KEY));
});

test('키는 이 프로세스 환경 변수를 먼저, 없으면 사용자 환경 변수(앱을 다시 켜기 전)를 읽는다', () => {
  assert.equal(loadKey({ TYPESAFE_API_KEY: ' abc ' }, () => 'zzz'), 'abc');
  assert.equal(loadKey({}, (name) => (name === 'TYPESAFE_API_KEY' ? 'from-user-env' : '')), 'from-user-env');
  assert.equal(loadKey({}, () => ''), '');
});
