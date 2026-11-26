const fs=require('node:fs'),assert=require('node:assert/strict');
const html=fs.readFileSync(process.argv[2],'utf8');
const bytes=Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64');
async function run(engine,target,depth,backup){
  const w=(await WebAssembly.instantiate(bytes)).instance.exports;
  if(backup)new Uint8Array(w.memory.buffer,w.store(),backup.length).set(backup);
  w.init();let ms=0;
  function tick(){w.step(ms+=6);return new Int32Array(w.memory.buffer,w.audio(),512);}
  function press(k){w.button(k,1);tick();w.button(k,0);tick();}
  if(!backup){w.enc(1,engine);tick();}
  press(5);assert.equal(w.page(),10);
  const vib=w.state()&512;press(3);assert.notEqual(w.state()&512,vib);press(3);
  press(5);assert.equal(w.page(),26);
  if(!backup){w.set_param(0,1);w.set_param(1,0);w.set_param(2,1);w.set_param(3,0);}
  for(let i=0;i<620;i++)tick();assert(w.rf_count()>0);
  press(5);assert.equal(w.page(),27);
  if(backup){assert.equal(w.param(0),target);assert.equal(w.param(1),depth);}
  else{w.set_param(0,target);w.set_param(1,depth);}
  for(let i=0;i<16;i++)tick();
  let hash=0;for(let i=0;i<130;i++){if(!(i%32))w.midi(144,59+i/32,100);
    const a=tick();for(const x of a)hash=Math.imul(hash,31)+x|0;}
  press(9);press(9);
  return {hash,store:new Uint8Array(w.memory.buffer,w.store(),w.store_len()).slice()};
}
(async()=>{
 for(let e=0;e<5;e++){const base=await run(e,0,0);
   for(let d=0;d<4;d++){const mod=await run(e,d,-94);assert.notEqual(base.hash,mod.hash,'engine '+e+' target '+d);
     if(d===1)await run(e,d,-94,mod.store);}}
 console.log('All four RF targets change rendered audio on all five engines. Signed routes recalled after reboot.');
})().catch(e=>{console.error(e);process.exitCode=1;});
