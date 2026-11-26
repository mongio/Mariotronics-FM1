const fs = require('node:fs');
const assert = require('node:assert/strict');
const html = fs.readFileSync(process.argv[2], 'utf8');
const wasm = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');
const samples = Buffer.from(html.match(/enoTail='([^']+)'/)[1], 'base64');
async function boot(previous) {
  const w = (await WebAssembly.instantiate(wasm)).instance.exports;
  const bank = new Uint8Array(w.memory.buffer,w.ubank_buf(),w.ubank_size());
  bank.fill(255);
  if (previous) bank.set(previous);
  w.init();
  function send(op,lo=0,data=[]) {
    const frame=[125,71,82,77,op,lo,...data];
    new Uint8Array(w.memory.buffer,w.sx_in(),frame.length).set(frame);
    w.sx_feed(frame.length);
    return new Uint8Array(w.memory.buffer,w.sx_out(),w.sx_out_len())[7];
  }
  function write(off) {
    const i=off/128, digits=[i>>7];
    for (const b of samples.subarray(off,off+128)) digits.push(b>>4,b&15);
    return send(11,i&127,digits);
  }
  return {w,bank,send,write};
}
(async()=>{
  const alien=await boot();alien.bank[901]=24;
  assert.equal(alien.send(10),2);assert.equal(alien.write(0),1);assert.equal(alien.bank[901],24);
  let t=await boot();assert.equal(t.w.eno_extended(),0);
  assert.equal(t.send(10),0);assert.equal(t.write(128),1);
  assert.equal(t.write(0),0);assert.equal(t.write(128),0);
  assert.equal(t.send(12),1);
  t=await boot(t.bank);assert.equal(t.w.eno_extended(),0);assert.equal(t.send(10),0);
  for(let off=0;off<samples.length;off+=128)assert.equal(t.write(off),0);
  assert.equal(t.send(12),0);assert.equal(t.w.eno_extended(),1);assert.equal(t.send(10),3);
  assert.deepEqual(Buffer.from(t.bank.subarray(0,samples.length)),samples);
  assert(t.bank.subarray(samples.length).every(b=>b===255));
  const restarted=await boot(t.bank);assert.equal(restarted.w.eno_extended(),1);
  restarted.bank[613]^=1;const corrupt=await boot(restarted.bank);assert.equal(corrupt.w.eno_extended(),0);
  console.log('ENO bank: collision, ordered transfer, restart, verification and fallback passed.');
})().catch(e=>{console.error(e);process.exitCode=1;});
