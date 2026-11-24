// Run with node tests/panel-audio.cjs path/to/Mariotronics.html
const assert = require('node:assert/strict');
const fs = require('node:fs');
async function check() {
  const html = fs.readFileSync(process.argv[2], 'utf8');
  const bytes = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');
  const { instance } = await WebAssembly.instantiate(bytes);
  const w = instance.exports;
  w.init();
  let frames = 0;
  function render(blocks) {
    let sum = 0;
    for (let i = 0; i < blocks; i++) {
      w.step(Math.floor(frames * 1000 / 44100));
      const samples = new Int32Array(w.memory.buffer, w.audio(), 512);
      for (const sample of samples) sum += sample * sample;
      frames += 256;
    }
    return Math.sqrt(sum / (blocks * 512));
  }
  const idle = render(20);
  w.keys(1 << 7);
  const attack = render(40);
  const held = render(80);
  w.keys(0);
  render(600);
  const released = render(30);
  console.log({idle, attack, held, released});
  assert.equal(idle, 0);
  assert(attack > 200 && held > 500);
  assert(released < held / 30);
}
check().catch(error => { console.error(error); process.exitCode = 1; });
