import test from 'node:test';
import assert from 'node:assert/strict';
import {identifyLayout, validateRelease, validateCache, flashPlan, PARTS} from '../../web/companion/layout.mjs';
const table = entries => {
  const bytes = new Uint8Array(4096).fill(255), view = new DataView(bytes.buffer);
  entries.forEach(([name, kind, subtype, offset, size], index) => {
    const p = index * 32; view.setUint16(p, 0x50AA, true); bytes[p + 2] = kind; bytes[p + 3] = subtype;
    view.setUint32(p + 4, offset, true); view.setUint32(p + 8, size, true);
    bytes.fill(0, p + 12, p + 32); bytes.set(new TextEncoder().encode(name), p + 12);
  }); return bytes;
};
const original = table([['nvs',1,2,0x9000,0x5000],['app0',0,0,0x10000,0x640000],
  ['themeart',1,64,0x650000,0x9A0000],['coredump',1,3,0xFF0000,0x10000]]);
const companion = table([['nvs',1,2,0x9000,0x5000],['otadata',1,0,0xE000,0x2000],
  ['ota_0',0,16,0x10000,0x640000],['themeart',1,64,0x650000,0x480000],
  ['ota_1',0,17,0xAD0000,0x400000],['ps_nvs',1,2,0xED0000,0x80000],
  ['sounds',1,131,0xF50000,0xA0000],['coredump',1,3,0xFF0000,0x10000]]);
const files = Object.fromEntries(Object.entries(PARTS).map(([name, [, size]]) => [name, new Uint8Array(size)]));
test('identifies exact standalone, companion, blank, and foreign layouts', () => {
  assert.equal(identifyLayout(original), 'orb'); assert.equal(identifyLayout(companion), 'companion');
  assert.equal(identifyLayout(new Uint8Array(4096).fill(255)), 'blank');
  const wrong = companion.slice(); wrong[8] ^= 1; assert.equal(identifyLayout(wrong), 'unknown');
});
test('routine updates touch only the two firmware slots', () => {
  const plan = flashPlan('companion', files);
  assert.deepEqual(plan.map(p => p.address), [0x10000,0xAD0000]);
  assert.deepEqual(plan.map(p => p.data.length), [0x640000,0x400000]);
});
test('migration writes never overlap Orb NVS or theme cache, even with maximum images', () => {
  const plan = flashPlan('orb', files);
  for (const part of plan) for (const [start,end] of [[0x9000,0xE000],[0x650000,0xAD0000]]) {
    assert.ok(part.address + part.data.length <= start || part.address >= end);
  }
  assert.equal(plan.at(-1).address, 0x8000);
  assert.throws(() => flashPlan('unknown',files));
});
test('release metadata cannot change destinations, add files, or exceed capacity', () => {
  const valid = {schema:1,layout:'orb-printsphere-v1',chip:'ESP32-S3',flashSize:0x1000000,
    parts:Object.entries(PARTS).map(([path,[offset,size]])=>({path,offset,size,sha256:'a'.repeat(64)}))};
  validateRelease(valid);
  for (const edit of [m=>m.parts[0].offset=0x9000,m=>m.parts[0].path='../settings.bin',
    m=>m.parts[2].size++,m=>m.parts.push(m.parts[0]),m=>m.parts[0].sha256='bad']) {
    const bad = structuredClone(valid); edit(bad); assert.throws(()=>validateRelease(bad));
  }
});
test('migration rejects overfull caches and entries crossing the retained boundary', () => {
  const bytes = new Uint8Array(8192), v = new DataView(bytes.buffer);
  v.setUint32(0,0x4F524254,true); v.setUint32(4,6,true); validateCache(bytes);
  v.setUint32(12,0x480000,true); assert.throws(()=>validateCache(bytes)); v.setUint32(12,0,true);
  v.setUint32(8,1,true); v.setUint32(16+44,0x47F000,true); v.setUint32(16+48,8192,true);
  assert.throws(()=>validateCache(bytes));
});
