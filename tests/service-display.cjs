const fs=require('node:fs'),assert=require('node:assert/strict');
const html=fs.readFileSync(process.argv[2],'utf8');
const code=Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64');
async function boot(saved){
  const w=(await WebAssembly.instantiate(code)).instance.exports;
  const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());
  if(saved)store.set(saved);
  w.init();let ms=0;
  function tick(n=1){for(let i=0;i<n;i++){w.step(ms+=6);w.audio();}}
  function press(k){w.button(k,1);tick();w.button(k,0);tick();}
  function image(){w.idle_preview();return Buffer.from(new Uint8Array(w.memory.buffer,w.fb(),240*146*2));}
  function sx(op,index,payload=[]){
    const frame=[125,71,82,77,op,index,...payload];
    new Uint8Array(w.memory.buffer,w.sx_in(),frame.length).set(frame);w.sx_feed(frame.length);
    return Array.from(new Uint8Array(w.memory.buffer,w.sx_out(),w.sx_out_len()));
  }
  function dump(){
    const count=sx(1,0)[8];
    return Array.from({length:count},(_,i)=>sx(2,i).slice(8,-1));
  }
  return {w,store,tick,press,image,sx,dump};
}
(async()=>{
  const a=await boot();
  a.w.set_param(1,91);a.press(9);a.press(9);
  const bank=a.dump(),b=await boot(),before=Buffer.from(b.store);
  b.sx(3,0);b.sx(4,0,bank[0]);assert.equal(b.sx(5,0)[7],1);
  assert.deepEqual(Buffer.from(b.store),before);
  b.sx(3,0);bank.forEach((v,i)=>assert.equal(b.sx(4,i,v)[7],0));
  assert.equal(b.sx(5,0)[7],0);
  const reboot=await boot(b.store);assert.equal(reboot.w.param(1),91);
  assert.deepEqual(reboot.dump(),bank);
  const e=await boot();e.press(6);
  const quiet=e.image();e.w.midi(144,60,110);e.tick(80);
  assert(!quiet.equals(e.image()),'EDIT follows the live oscillator');
  e.press(7);e.tick(80);const loud=e.image();e.w.master(0);e.tick(140);
  assert(!loud.equals(e.image()),'GLO follows master output level');
  e.press(10);assert.equal(e.w.page(),13);e.press(10);assert.equal(e.w.page(),20);
  e.w.set_param(3,11);e.press(8);e.press(3);e.press(3);e.press(3);
  assert.equal(e.w.page(),2);assert.equal(e.w.param(3),9);e.w.set_param(3,3);e.press(10);e.press(10);assert.equal(e.w.param(3),11);
  console.log('EDIT scope, post-master stereo display, independent SEL/ARP controls, incomplete restore and all 64 slots after reboot verified.');
})().catch(err=>{console.error(err);process.exitCode=1});
