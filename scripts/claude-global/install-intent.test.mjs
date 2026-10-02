// node --test scripts/claude-global/install-intent.test.mjs
// Every test writes only to its own temporary folder; the real ~/.claude is never touched.
import test from 'node:test';
import assert from 'node:assert/strict';
import { applyBlock, removeBlock, START, END } from './install-intent.mjs';

const BOM = String.fromCharCode(0xfeff);
const ORIGINAL = '# 전역 지침\n\n- 하나\n- 둘\n';
const BODY = '- 새 줄';

test('applyBlock adds the block once at the end', () => {
  const once = applyBlock(ORIGINAL, BODY);
  assert.equal(once, `${ORIGINAL}\n${START}\n${BODY}\n${END}\n`);
  assert.equal(applyBlock(once, BODY), once);
});

test('applyBlock replaces an older block in place', () => {
  const old = applyBlock(ORIGINAL, '- 옛 줄') + '- 뒤에 쓴 줄\n';
  const updated = applyBlock(old, BODY);
  assert.ok(updated.includes(BODY) && !updated.includes('옛 줄'));
  assert.ok(updated.startsWith(ORIGINAL) && updated.endsWith(`${END}\n- 뒤에 쓴 줄\n`));
});

test('removeBlock restores the original text', () => {
  assert.equal(removeBlock(applyBlock(ORIGINAL, BODY)), ORIGINAL);
  assert.equal(removeBlock(ORIGINAL), ORIGINAL);
});

test('applyBlock keeps CRLF and BOM', () => {
  const crlf = `${BOM}# 전역 지침\r\n- 하나\r\n`;
  const out = applyBlock(crlf, BODY);
  assert.ok(out.startsWith(crlf));
  assert.equal(out.replace(/\r\n/g, '').includes('\n'), false);
  assert.equal(removeBlock(out), crlf);
});

test('applyBlock on empty text writes only the block', () => {
  assert.equal(applyBlock('', BODY), `${START}\n${BODY}\n${END}\n`);
  assert.equal(removeBlock(applyBlock('', BODY)), '');
});

test('broken markers are refused', () => {
  assert.throws(() => applyBlock(`${ORIGINAL}${START}\n- 반쪽\n`, BODY), /표시가 깨졌습니다/);
  assert.throws(() => removeBlock(`${END}\n${START}\n`), /표시가 깨졌습니다/);
  assert.throws(() => applyBlock(`${START}\n${END}\n${START}\n${END}\n`, BODY), /표시가 깨졌습니다/);
});
