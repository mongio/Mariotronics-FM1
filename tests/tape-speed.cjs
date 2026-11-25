const fs = require('node:fs');
const assert = require('node:assert/strict');
const html = fs.readFileSync(process.argv[2], 'utf8');
const wasm = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');

// Estimate the period of a sustained organ note from the recorded return.
function pitch(samples, expected) {
  let best = 0, highest = -1;
  for (let lag = Math.floor(44100 / (expected * 1.12)); lag < 44100 / (expected * .88); lag++) {
    let a = 0, b = 0, cross = 0;
    for (let i = lag; i < samples.length; i++) {
      const x = samples[i], y = samples[i-lag];
      a += x*x; b += y*y; cross += x*y;
    }
    const correlation = cross / Math.sqrt(a*b);
    if (correlation > highest) { highest = correlation; best = lag; }
  }
  assert(highest > .85);
  return 44100 / best;
}
(async () => {
  const w = (await WebAssembly.instantiate(wasm)).instance.exports;
  w.init();
  let frame = 0;
  function tick() {
    w.step(Math.floor(frame * 1000 / 44100)); frame += 256;
    return new Int32Array(w.memory.buffer, w.audio(), 512);
  }
  function run(n) { while (n-- > 0) tick(); }
  function press(id) { w.button(id,1); tick(); w.button(id,0); tick(); }
  w.enc(1,2); tick(); w.enc(0,7); tick(); w.set_param(0,0); press(8);
  w.set_param(1,100); w.set_param(2,90); w.set_param(3,100);
  press(13); w.midi(144,60,110); run(700); press(13); w.midi(128,60,0); run(140);
  press(7); press(7); assert.equal(w.page(),22); assert.equal(w.param(0),2);
  function checkRate(v, hz) {
    w.set_param(0,v); run(14);
    const samples = [];
    for (let j=0;j<16;j++) samples.push(...tick().filter((_,i)=>i%2===0));
    const measured = pitch(samples,hz);
    assert(Math.abs(measured-hz)<hz*.018, measured+' Hz, expected '+hz);
    return measured.toFixed(1);
  }
  const measured = [0,1,2,3].map(i=>checkRate(i,65.406*2**i));
  w.set_param(1,1); assert.equal(w.param(0),96);
  measured.push(checkRate(80,369.99));
  press(11); press(7); press(7); measured.push(checkRate(80,369.99));
  console.log('Recorded tape Hz at all four detents, fine speed and reverse:',measured.join(', '));
})().catch(e=>{console.error(e);process.exitCode=1});
