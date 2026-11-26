const fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
  const html=fs.readFileSync(process.argv[2],'utf8');
  const w=(await WebAssembly.instantiate(Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64'))).instance.exports;
  w.init();let ms=0;
  function tick(n=1){while(n--){w.step(ms+=6);w.audio();}}
  function press(id,n){w.button(id,1);tick(n);w.button(id,0);tick();}
  function messages(){let p;const result=[];while(p=w.take_note_out())result.push(p&255);return result;}
  w.enc(1,2);tick();press(6,1);const edit=w.page();
  w.keys(3);tick();assert.deepEqual(messages(),[144,144]);
  press(8,202);assert.equal(w.page(),edit);w.keys(0);tick();assert.deepEqual(messages(),[]);
  w.keys(4);tick();w.keys(0);tick();assert.deepEqual(messages(),[144,128]);
  press(8,202);assert.deepEqual(messages(),[128,128]);
  press(8,202);w.keys(1);tick();w.keys(3);tick();w.keys(2);tick();w.keys(0);tick();
  assert.deepEqual(messages(),[144,144]);press(8,202);assert.deepEqual(messages(),[128,128]);
  press(8,1);assert.equal(w.page(),0);
  console.log('Keyboard HOLD: long press, current/next chord, melody, note-offs and short HOME verified.');
})().catch(e=>{console.error(e);process.exitCode=1;});
