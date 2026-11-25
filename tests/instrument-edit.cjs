const fs = require('node:fs');
const assert = require('node:assert/strict');
async function boot(file, bank) {
  const src = fs.readFileSync(file, 'utf8');
  const wasm = Buffer.from(src.match(/wasm='([^']+)'/)[1], 'base64');
  const w = (await WebAssembly.instantiate(wasm)).instance.exports;
  const store = new Uint8Array(w.memory.buffer, w.store(), w.store_len());
  if (bank) store.set(bank);
  w.init();
  let ms = 0;
  function tick() { w.step(ms += 6); w.audio(); }
  function press(k) { w.button(k, 1); tick(); w.button(k, 0); tick(); }
  function params() { return [0,1,2,3].map(k => w.param(k)); }
  function values(list) { list.forEach((v, k) => w.set_param(k, v)); }
  function engine(delta) { w.enc(1, delta); tick(); }
  return { w, store, tick, press, params, values, engine };
}
(async () => {
  const file = process.argv[2];
  for (const alg of [1,2,3]) {
    const a = await boot(file);
    a.engine(alg);
    a.press(6); a.values([2]);
    a.press(6); a.values([7,1]);
    a.press(6); a.values([2,6]);
    if (alg === 1) { a.press(6); a.values([1,13,73,23]); }
    a.press(10); a.values([4,7,67,28]);
    a.press(9); a.press(9);
    const b = await boot(file, a.store);
    assert.equal(b.w.state() >> 12 & 3, alg);
    b.press(10); assert.deepEqual(b.params(), [4,7,67,28]);
    b.press(6); assert.equal(b.w.param(0), 2);
    b.press(6); assert.deepEqual(b.params().slice(0,2), [7,1]);
    b.press(6); assert.deepEqual(b.params().slice(0,2), [2,6]);
    if (alg === 1) { b.press(6); assert.deepEqual(b.params(), [1,13,73,23]); }
  }
  if (process.argv[3]) {
    const old = await boot(process.argv[3]);
    old.engine(1); old.press(10); old.values([1,14,71,63]); old.press(9); old.press(9);
    const b = await boot(file, old.store);
    b.press(10); assert.equal(b.w.param(0),0);
    for(let i=0;i<4;i++) b.press(6);
    assert.equal(b.w.page(),24); assert.deepEqual(b.params(),[1,14,71,63]);
  }
  console.log('Voice controls and patterns survive reboot; legacy ENO Note FX migration checked when supplied.');
})().catch(e => { console.error(e); process.exitCode = 1; });
