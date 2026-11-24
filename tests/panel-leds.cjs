// Exercise the published editor's panel and MIDI inputs against the shared firmware.
const assert = require('node:assert/strict');
const fs = require('node:fs');
async function check() {
  const html = fs.readFileSync(process.argv[2], 'utf8');
  const wasm = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');
  const w = (await WebAssembly.instantiate(wasm)).instance.exports;
  w.init();
  let ms = 10;
  function step(delta = 10) { ms += delta; w.step(ms); w.audio(); }
  function press(id) { w.button(id,1); step(); w.button(id,0); step(); }
  const leds = new Uint8Array(w.memory.buffer,w.leds(),41);
  step();
  assert.equal(leds[8],3);
  for (const [button,page] of [[4,8],[3,2],[5,10],[6,1],[2,9],[7,11],[7,12],[7,15]]) {
    press(button); assert.equal(w.state() & 15,page); assert.equal(leds[button],3);
    assert.equal(leds[8],0);
  }
  for (const level of [0,1,2]) {
    w.set_param(0,level); step();
    assert(leds.slice(14).every(v=>v===level));
    for (let k=0;k<27;k++) {
      w.keys(1<<k); step(); assert.equal(leds[14+k],3);
      w.keys(0); step(); assert.equal(leds[14+k],level);
    }
  }
  w.keys(1); w.midi(144,53,100); step(); w.keys(0); step(); assert.equal(leds[14],3);
  w.midi(128,53,0); step(); assert.equal(leds[14],2);
  press(13); step(); assert.equal(leds[12],3); assert.equal(leds[13],3);
  step(250); assert.equal(leds[13],0); step(250); assert.equal(leds[13],3);
  press(13); step(); assert.equal(w.state()>>4&3,2);
  press(13); step(); assert.equal(leds[13],0); assert.equal(leds[12],3);
  press(12); step(); assert.equal(leds[12],0);
}
check().catch(e=>{console.error(e);process.exitCode=1});
