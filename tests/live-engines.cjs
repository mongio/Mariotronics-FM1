// Compare aligned tape playbacks across live engine switches.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const html = fs.readFileSync(process.argv[2], 'utf8');
const bytes = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');

async function session(mode) {
  const w = (await WebAssembly.instantiate(bytes)).instance.exports;
  w.init();
  let frames = 0;
  function tick() {
    w.step(Math.floor(frames * 1000 / 44100));
    frames += 256;
    return new Int32Array(w.memory.buffer, w.audio(), 512);
  }
  function run(blocks) { for (let i=0;i<blocks;i++) tick(); }
  function rec() { w.button(13,1); tick(); w.button(13,0); tick(); }
  function page(p) { w.enc(0,p-w.page()); tick(); }
  function engine(d) { w.enc(1,d); tick(); }
  const initial = new Uint8Array(w.memory.buffer,w.store(),w.store_len()).slice();
  w.set_param(1,91);
  page(7); w.set_param(0,0);
  page(8); w.set_param(0,15);
  page(0); rec(); w.keys(1<<9); run(75); w.keys(0); run(675);
  rec(); assert.equal((w.state()>>4)&3,3); // HOLD
  engine(mode==='reference'?0:1);
  assert.equal((w.state()>>4)&3,3);
  if(mode!=='reference') w.set_param(2,56);   /* Send is the engine's own; feedback (post-loop) is shared since 1.20 */
  page(8); if(mode!=='reference') w.set_param(0,46);
  if(mode==='dub') rec(); else run(2);
  if(mode!=='reference') w.keys(1<<16);
  run(90); w.keys(0); run(400);
  if(mode==='dub') rec(); else run(2);
  engine(-1); assert.equal(w.param(0),15);
  page(0); assert.equal(w.param(1),91); assert.equal(w.param(2),80);
  engine(mode==='reference'?0:1);
  if(mode!=='reference') {assert.equal(w.param(2),56); assert.equal(w.param(1),91);}
  engine(-1); run(750);
  assert.deepEqual(new Uint8Array(w.memory.buffer,w.store(),w.store_len()),initial);
  const result=Buffer.alloc(690*512*2);
  for(let b=0;b<690;b++) {
    const samples=tick();
    for(let i=0;i<512;i++) result.writeInt16LE(samples[i],2*(512*b+i));
  }
  return result;
}
(async()=>{
  const before=await session('reference');
  assert(before.some(v=>v));
  function peakDifference(a,b) {
    let peak=0;
    for(let i=0;i<a.length;i+=2)
      peak=Math.max(peak,Math.abs(a.readInt16LE(i)-b.readInt16LE(i)));
    return peak;
  }
  // The live voice's fixed-point filters can retain one LSB after release.
  assert(peakDifference(before,await session('hold'))<=1, 'HOLD must preserve audio and tape alignment');
  assert(peakDifference(before,await session('dub'))>100, 'ENO overdub must write audible material');
  console.log('FRIPP → ENO → FRIPP: aligned HOLD replay within one LSB, ENO overdub audible, live edits restored, preset storage untouched.');
})().catch(e=>{console.error(e);process.exitCode=1;});
