const fs = require('node:fs');
const assert = require('node:assert/strict');
const source = fs.readFileSync(process.argv[2], 'utf8');
(async () => {
  const w = (await WebAssembly.instantiate(Buffer.from(source.match(/wasm='([^']+)'/)[1], 'base64'))).instance.exports;
  w.init();
  let sample = 0;
  function tick() {
    w.step(Math.floor(sample*1000/44100)); sample+=256;
    return new Int32Array(w.memory.buffer,w.audio(),512);
  }
  function press(id) { w.button(id,1); tick(); w.button(id,0); tick(); }
  function run(n) { while(n--) tick(); }
  press(13); w.midi(144,57,110); run(710); w.midi(128,57,0); press(13);
  w.enc(1,4); tick(); press(6); assert.equal(w.state()>>12&7,4);
  assert.equal(w.voice(),26); assert.equal(w.page(),23); assert.equal(w.param(0),0);
  run(560); assert.equal(w.rf_count(),3); assert(w.rf_meter()>0);
  w.set_param(0,1); w.set_param(1,0); run(8);
  press(10); assert(w.rf_frozen()); run(180); press(10); assert.equal(w.rf_frozen(),0);
  assert.notEqual((w.state()>>4)&3,0); // The pre-existing tape is still in motion.
  w.set_param(2,8); w.set_param(3,7);
  w.enc(1,-1);tick();assert.equal(w.voice(),16);w.enc(1,1);tick();assert.equal(w.voice(),26);
  press(9); press(9);
  const bank=new Uint8Array(w.memory.buffer,w.store(),w.store_len()).slice();
  const fresh=(await WebAssembly.instantiate(Buffer.from(source.match(/wasm='([^']+)'/)[1],'base64'))).instance.exports;
  new Uint8Array(fresh.memory.buffer,fresh.store(),fresh.store_len()).set(bank); fresh.init();
  assert.equal(fresh.voice(),26);fresh.button(6,1);fresh.step(10);fresh.button(6,0);fresh.step(20);
  fresh.button(6,1);fresh.step(30);fresh.button(6,0);fresh.step(40);
  for(let i=0;i<3 && fresh.page()!==23;i++){fresh.button(6,1);fresh.step(50+i*20);fresh.button(6,0);fresh.step(60+i*20);}   // EDIT: TIMBRE <-> RING MOD
  assert.equal(fresh.page(),23);assert.deepEqual([0,1,2,3].map(k=>fresh.param(k)),[1,0,8,7]);
  console.log('AIR scan, source lock, freeze, tape transport and preset recall passed.');
})().catch(e=>{console.error(e);process.exitCode=1;});
