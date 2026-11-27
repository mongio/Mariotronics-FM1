const fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
  const html=fs.readFileSync(process.argv[2],'utf8');
  const w=(await WebAssembly.instantiate(Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64'))).instance.exports;
  w.init();let ms=0;
  function tick(n=1){while(n--){w.step(ms+=6);w.audio();}}
  function press(id,n){w.button(id,1);tick(n);w.button(id,0);tick();}
  function messages(){let p;const result=[];while(p=w.take_note_out())result.push(p&255);return result;}
  const HOME=8, LONG=170, SHORT_OF_LONG=150;   // 6 ms ticks: 1.02 s and 0.90 s
  w.enc(1,2);tick();press(6,1);const edit=w.page();

  // 0.9 s is not a long press: nothing is latched, the page changes to HOME on release.
  w.keys(1);tick();assert.deepEqual(messages(),[144]);
  press(HOME,SHORT_OF_LONG);assert.equal(w.page(),0);
  w.keys(0);tick();assert.deepEqual(messages(),[128]);
  press(6,1);assert.equal(w.page(),edit);

  // Armed with a chord down: the chord and every later note keep sounding.
  w.keys(3);tick();assert.deepEqual(messages(),[144,144]);
  press(HOME,LONG);assert.equal(w.page(),edit);
  w.keys(0);tick();assert.deepEqual(messages(),[]);
  w.keys(4);tick();w.keys(0);tick();assert.deepEqual(messages(),[144]);
  w.keys(8);tick();w.keys(0);tick();assert.deepEqual(messages(),[144]);
  w.keys(1);tick();w.keys(0);tick();assert.deepEqual(messages(),[128,144]); // already held, RESTART: it starts again
  press(HOME,LONG);assert.deepEqual(messages(),[128,128,128,128]);

  // Armed with no key down: a melody accumulates, nothing is released until HOME again.
  press(HOME,LONG);
  w.keys(1);tick();w.keys(3);tick();w.keys(2);tick();w.keys(0);tick();
  assert.deepEqual(messages(),[144,144]);
  // An octave change gives the same key a new pitch; both stay held and both are released.
  w.button(1,1);tick();w.button(1,0);tick();
  w.keys(1);tick();w.keys(0);tick();assert.deepEqual(messages(),[144]);
  press(HOME,LONG);assert.deepEqual(messages(),[128,128,128]);

  // A key still down when HOLD ends keeps its note until it is released.
  press(HOME,LONG);w.keys(1);tick();w.keys(2);tick();assert.deepEqual(messages(),[144,144]);
  press(HOME,LONG);assert.deepEqual(messages(),[128]);
  w.keys(0);tick();assert.deepEqual(messages(),[128]);

  // KEYS Hold Key RELEASE (as GRANA): a held note pressed again ends; a new one joins.
  for(let i=0;i<7 && w.page()!==15;i++){w.button(7,1);tick();w.button(7,0);tick();}   // GLO up to KEYS
  assert.equal(w.page(),15);w.set_param(1,1);tick();
  press(HOME,LONG);
  w.keys(1);tick();w.keys(0);tick();w.keys(2);tick();w.keys(0);tick();assert.deepEqual(messages(),[144,144]);
  w.keys(1);tick();assert.deepEqual(messages(),[128]);w.keys(0);tick();assert.deepEqual(messages(),[]);
  press(HOME,LONG);assert.deepEqual(messages(),[128]);
  // KEYS Velocity and Hold time.
  w.set_param(2,64);w.set_param(3,5);tick();
  press(HOME,90);w.keys(4);tick();w.keys(0);tick();assert.deepEqual(messages(),[144]);press(HOME,90);assert.deepEqual(messages(),[128]);
  w.set_param(1,0);w.set_param(2,110);w.set_param(3,10);tick();
  press(HOME,1);assert.equal(w.page(),0);
  console.log('Keyboard HOLD: 1 s arm, chord and later notes latched, RESTART and RELEASE (GRANA logic), octave change, key held at release, velocity, hold time, short HOME verified.');
})().catch(e=>{console.error(e);process.exitCode=1;});
