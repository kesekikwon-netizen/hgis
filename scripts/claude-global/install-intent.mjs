// 요청 기록(capture-intent) 전역 설치 / 제거.
//
// The global CLAUDE.md gets one rule line between two markers, so it can be added, updated and
// removed again without touching the person's own lines. The original line ending (CRLF/LF) and a
// leading BOM are kept.

export const START = '<!-- capture-intent:start -->';
export const END = '<!-- capture-intent:end -->';

const BOM = String.fromCharCode(0xfeff);
const BROKEN = '전역 지침의 capture-intent 표시가 깨졌습니다';

const occurrences = (text, needle) => text.split(needle).length - 1;
const eolOf = (text) => (text.includes('\r\n') ? '\r\n' : '\n');

// null when there are no markers; throws when they are half there, reversed or repeated.
function findMarkers(text) {
  const starts = occurrences(text, START);
  const ends = occurrences(text, END);
  if (starts === 0 && ends === 0) return null;
  if (starts !== 1 || ends !== 1) throw new Error(BROKEN);
  const start = text.indexOf(START);
  const end = text.indexOf(END);
  if (end < start) throw new Error(BROKEN);
  return { start, end: end + END.length };
}

export function applyBlock(text, body) {
  const eol = eolOf(text);
  const lines = body.replace(/\r\n/g, '\n').replace(/\n+$/, '').split('\n').join(eol);
  const block = `${START}${eol}${lines}${eol}${END}`;
  const markers = findMarkers(text);
  if (markers) return text.slice(0, markers.start) + block + text.slice(markers.end);
  const bom = text.startsWith(BOM) ? BOM : '';
  if (text.length === bom.length) return `${bom}${block}${eol}`;
  const base = text.endsWith('\n') ? text : text + eol;
  return `${base}${eol}${block}${eol}`;
}

export function removeBlock(text) {
  const markers = findMarkers(text);
  if (!markers) return text;
  const eol = eolOf(text);
  let from = markers.start;
  let to = markers.end;
  if (text.startsWith(eol, to)) to += eol.length;
  if (text.slice(0, from).endsWith(eol + eol)) from -= eol.length;
  return text.slice(0, from) + text.slice(to);
}
