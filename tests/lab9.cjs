// 1.20: ENO voice menu, modulation PRE/POST, RADIGUE MATRIX (audio, save, reboot), factory bank across engines.
const fs=require('node:fs'),assert=require('node:assert/strict');
const bytes=Buffer.from(fs.readFileSync(process.argv[2],'utf8').match(/wasm='([^']+)'/)[1],'base64');
async function boot(saved){
  const w=(await WebAssembly.instantiate(bytes)).instance.exports;
  const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());if(saved)store.set(saved);
  w.init();let ms=0;
  const tick=(n=1)=>{let h=0;while(n--){w.step(ms+=6);for(const x of new Int32Array(w.memory.buffer,w.audio(),512))h=Math.imul(h,31)+x|0;}return h;};
  const press=k=>{w.button(k,1);tick();w.button(k,0);tick();};
  const go=(b,p)=>{for(let i=0;i<14&&w.page()!==p;i++)press(b);assert.equal(w.page(),p);};
  const select=i=>{w.midi(192,i,0);tick();};
  const save=()=>{w.button(9,1);tick();tick(170);w.button(9,0);tick();w.button(9,1);tick();w.button(9,0);tick();};
  return {w,store,tick,press,go,select,save};
}
(async()=>{
  let a=await boot();a.tick(200);
  a.go(6,1);a.w.set_param(2,1);a.tick(3);
  const order=[];for(let i=0;i<12;i++){order.push(a.w.param(0));a.w.enc(2,1);a.tick();}
  assert.deepEqual(order,[0,4,2,3,5,6,7,8,9,10,11,11],'ENO K1 walks Piano, Rhodes ... the dark pads');
  for(const v of [9,10,11]){a.w.set_param(0,v);a.w.midi(144,48,100);let peak=0;
    for(let i=0;i<120;i++){a.w.step(i);for(const x of new Int32Array(a.w.memory.buffer,a.w.audio(),512))peak=Math.max(peak,Math.abs(x));}
    a.w.midi(128,48,0);a.tick(200);assert(peak>800 && peak<32000,'dark pad '+v+' plays ('+peak+')');}
  a.go(2,30);a.w.set_param(0,6);assert.equal(a.w.param(0),6);a.w.set_param(0,9);assert.equal(a.w.param(0),8,'eight modulation types');
  // RADIGUE: the matrix changes the drone and is kept by SAVE across a reboot
  let r=await boot();r.tick(200);r.select(5);r.tick(3);r.go(6,1);r.w.set_param(2,3);r.tick(3);
  const play=()=>{r.w.midi(144,50,100);r.tick(40);const h=r.tick(150);r.w.midi(128,50,0);r.tick(300);return h;};
  const dry=play();
  r.go(5,32);r.w.set_param(0,1);r.w.set_param(1,0);r.w.set_param(2,1);r.w.set_param(3,7);
  r.press(5);assert.equal(r.w.page(),33);r.w.set_param(0,7);
  const wet=play();assert.notEqual(dry,wet,'MATRIX LFO A to Beat moves the drone');
  r.go(5,32);r.w.set_param(0,0);assert.equal(r.w.param(2),0);r.w.set_param(0,1);assert.equal(r.w.param(2),1);assert.equal(r.w.param(3),7);
  r.save();
  let b=await boot(r.store.slice());b.tick(50);b.select(5);b.tick(5);
  b.go(5,29);b.press(5);assert.equal(b.w.page(),32);b.w.set_param(0,1);
  assert.equal(b.w.param(1),0);assert.equal(b.w.param(2),1);assert.equal(b.w.param(3),7,'matrix slot recalled after reboot');
  b.press(5);assert.equal(b.w.param(0),7,'LFO A rate recalled');
  // Factory: P41 is RADIGUE with a matrix, P53 STOCKHAUSEN, P17 ENO, P01 FRIPP
  let f=await boot();f.tick(50);
  for(const [slot,eng] of [[0,0],[16,1],[32,2],[40,3],[52,4],[60,0]]){f.select(slot);f.tick(3);f.go(6,1);assert.equal(f.w.param(2),eng,'P'+(slot+1)+' engine');}
  f.select(40);f.tick(3);f.go(5,29);f.press(5);f.w.set_param(0,0);assert.equal(f.w.param(2),1,'P41 routes LFO A to Beat');
  // Beta 7: engine change keeps the plate and tape; OFF-TAPE is global (kept across presets); filter POST types
  let g=await boot();g.tick(50);
  g.go(2,7);g.w.set_param(0,77);g.w.set_param(1,66);
  g.go(8,0);g.w.set_param(1,44);
  g.go(6,1);g.w.set_param(2,3);g.tick(3);g.w.set_param(2,1);g.tick(3);
  g.go(2,7);assert.equal(g.w.param(0),77,'plate mix kept across engines');assert.equal(g.w.param(1),66);
  g.go(8,0);assert.equal(g.w.param(1),44,'tape feedback kept across engines');
  g.go(7,14);g.w.set_param(0,63);g.select(20);g.tick(5);g.go(7,14);assert.equal(g.w.param(0),63,'OFF-TAPE is a global setting');
  g.go(2,18);g.w.set_param(0,4);assert.equal(g.w.param(0),4,'LP POST');
  console.log('ENO menu and dark pads, eight modulation types, RADIGUE matrix audio/save/reboot, factory engines verified.');
})().catch(e=>{console.error(e);process.exitCode=1});
