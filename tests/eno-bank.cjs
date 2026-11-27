// Piano sample bank v2: twelve replaceable slots, earlier sustain bank, foreign banks, read-back, factory reset.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const html = fs.readFileSync(process.argv[2], 'utf8');
const wasm = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');
const tail = Buffer.from(html.match(/enoTail='([^']+)'/)[1], 'base64');
const BLOCK = 35, TAIL = 665, HEADER = 128;
async function boot(previous) {
  const w = (await WebAssembly.instantiate(wasm)).instance.exports;
  const bank = new Uint8Array(w.memory.buffer, w.ubank_buf(), w.ubank_size());
  bank.fill(255);
  if (previous) bank.set(previous);
  w.init();
  function send(op, lo = 0, data = []) {
    const frame = [125, 71, 82, 77, op, lo, ...data];
    new Uint8Array(w.memory.buffer, w.sx_in(), frame.length).set(frame);
    w.sx_feed(frame.length);
    return Array.from(new Uint8Array(w.memory.buffer, w.sx_out(), w.sx_out_len()));
  }
  function write(img, force = 0) {
    let r = send(10, force, [img.length & 127, (img.length >> 7) & 127, (img.length >> 14) & 127])[7];
    if (r) return r;
    for (let off = 0; off < img.length; off += 128) {
      const i = off / 128, digits = [i >> 7];
      for (const b of img.subarray(off, off + 128)) digits.push(b >> 4, b & 15);
      r = send(11, i & 127, digits)[7];
      if (r) return 10 + r;
    }
    return send(12)[7];
  }
  return { w, bank, send, write };
}
// Builds a v2 image: modes per slot (0 factory, 1 sustain, 2 the given recording).
function image(modes, own = {}) {
  const parts = [];
  let at = HEADER;
  const head = new Uint8Array(HEADER);
  head.set(Buffer.from('MTEP2-SAMPLES'), 0);
  modes.forEach((m, z) => {
    const data = m === 1 ? tail.subarray(16 + BLOCK * z * TAIL, 16 + BLOCK * (z + 1) * TAIL) : m === 2 ? own[z] : null;
    if (!data) return;
    const r = 32 + 8 * z, blocks = data.length / BLOCK;
    head[r] = at & 255; head[r + 1] = (at >> 8) & 255; head[r + 2] = (at >> 16) & 255;
    head[r + 4] = blocks & 255; head[r + 5] = blocks >> 8; head[r + 6] = m;
    parts.push(data); at += data.length;
  });
  const img = new Uint8Array(at);
  img.set(head); let o = HEADER; for (const p of parts) { img.set(p, o); o += p.length; }
  let hash = 2166136261; for (let i = HEADER; i < at; i++) hash = Math.imul(hash ^ img[i], 16777619) >>> 0;
  const put = (k, v) => { img[k] = v & 255; img[k + 1] = (v >> 8) & 255; img[k + 2] = (v >> 16) & 255; img[k + 3] = v >>> 24; };
  put(16, at); put(20, hash);
  return img;
}
// A one-second recording in raw IMA blocks: silence-coded blocks with a varying predictor.
function recording(blocks, level) {
  const d = new Uint8Array(blocks * BLOCK);
  for (let b = 0; b < blocks; b++) { const v = Math.round(level * Math.sin(b / 3)); d[b * BLOCK] = v & 255; d[b * BLOCK + 1] = (v >> 8) & 255; d[b * BLOCK + 2] = 20; d.fill(0x11, b * BLOCK + 3, b * BLOCK + BLOCK); }
  return d;
}
const status = t => { const r = t.send(13); return { version: r[8], modes: r.slice(9, 21) }; };
function render(t) {
  let n = 0, h = 0; const tick = () => { t.w.step(Math.floor(n * 1000 / 44100)); n += 256; return new Int32Array(t.w.memory.buffer, t.w.audio(), 512); };
  t.w.enc(1, 1); tick(); t.w.midi(144, 36, 110); for (let i = 0; i < 520; i++) for (const x of tick()) h = Math.imul(h, 31) + x | 0;
  return h;
}
(async () => {
  // Another firmware's samples: kept unless the player agrees to replace them.
  const foreign = new Uint8Array(4096).fill(255); foreign[901] = 24; foreign[0] = 7;
  const alien = await boot(foreign);
  assert.equal(status(alien).version, 3);
  assert.equal(alien.write(image(Array(12).fill(1))), 2); assert.equal(alien.bank[901], 24);
  assert.equal(alien.send(14)[7], 2); assert.equal(alien.bank[901], 24);
  assert.equal(alien.write(image(Array(12).fill(1)), 1), 0); assert.equal(status(alien).version, 2);

  // Empty memory: all sustain, verified, survives a restart, falls back when damaged.
  let t = await boot(); assert.equal(status(t).version, 0); assert.equal(t.w.eno_extended(), 0);
  const factory = render(await boot());
  const all = image(Array(12).fill(1));
  assert.equal(t.write(all), 0); assert.equal(t.w.eno_extended(), 1);
  assert.deepEqual(status(t), { version: 2, modes: Array(12).fill(1) });
  const sustain = render(await boot(t.bank));
  const back = await boot(t.bank); assert.equal(status(back).version, 2);
  const broken = t.bank.slice(); broken[613] ^= 1; const damaged = await boot(broken);
  assert.equal(status(damaged).version, 3); assert.equal(damaged.w.eno_extended(), 0);
  assert.equal(render(damaged), factory);

  // The earlier sustain bank (Lab 5-7) is still recognised and sounds the same as v2 sustain.
  const old = new Uint8Array(tail.length); old.set(tail);
  const v1 = await boot(old); assert.deepEqual(status(v1), { version: 1, modes: Array(12).fill(1) });
  assert.equal(render(await boot(old)), sustain);

  // One slot replaced by the player's recording; read back; slot record reported.
  const own = recording(250, 9000), modes = Array(12).fill(0); modes[0] = 2; modes[1] = 2; modes[5] = 1;
  t = await boot();
  assert.equal(t.write(image(modes, { 0: own, 1: own })), 0);
  assert.deepEqual(status(t).modes, modes);
  const info = t.send(16, 0), off = info[8] | info[9] << 7 | info[10] << 14, blocks = info[11] | info[12] << 7;
  assert.equal(blocks, 250); assert.equal(off, HEADER);
  const piece = t.send(15, 2, [0]); const got = []; for (let i = 0; i < 64; i++) got.push(piece[8 + 2 * i] * 16 + piece[9 + 2 * i]);
  assert.deepEqual(got, Array.from(t.bank.subarray(128, 192)));
  const replaced = render(await boot(t.bank));
  assert.notEqual(replaced, factory); assert.notEqual(replaced, sustain);

  // Factory reset erases the bank.
  assert.equal(t.send(14)[7], 0); assert.equal(status(t).version, 0); assert(t.bank.every(b => b === 255));
  console.log('Piano samples: foreign bank kept or replaced on request, sustain, restart, damage fallback, earlier bank, own recording, read-back, factory reset.');
})().catch(e => { console.error(e); process.exitCode = 1; });
