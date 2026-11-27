// 1.19: four knobs per page, engine options, LFO 2, ENSEMBLE, slow arpeggio, persistence of the new controls.
const fs=require('node:fs'),assert=require('node:assert/strict');
const bytes=Buffer.from(fs.readFileSync(process.argv[2],'utf8').match(/wasm='([^']+)'/)[1],'base64');
const P={HOME:0,SOUND:1,SCALE:4,BT:12,ARP:13,OFF_TAPE:14,KEYS:15,HEAD2:19,ERASE:21,SPEED:22,TIMBRE:23,MOTION:25,LFO2:29,MODFX:30,RING:31};
async function boot(saved){
  const w=(await WebAssembly.instantiate(bytes)).instance.exports;
  const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());if(saved)store.set(saved);
  w.init();let n=0;
  const tick=()=>{w.step(Math.floor(n*1000/44100));n+=256;return new Int32Array(w.memory.buffer,w.audio(),512);};
  const press=id=>{w.button(id,1);tick();w.button(id,0);tick();};
  const go=(button,target)=>{for(let i=0;i<9 && w.page()!==target;i++)press(button);assert.equal(w.page(),target,'reach page '+target);};
  const save=()=>{w.button(9,1);tick();for(let i=0;i<180;i++)tick();w.button(9,0);tick();w.button(9,1);tick();w.button(9,0);tick();};
  const listen=(notes,blocks=300)=>{let h=0,diff=0;for(const x of notes)w.midi(144,x,100);
    for(let i=0;i<blocks;i++){const a=tick();for(let j=0;j<a.length;j+=2){h=Math.imul(h,31)+a[j]|0;h=Math.imul(h,31)+a[j+1]|0;diff+=Math.abs(a[j]-a[j+1]);}}
    for(const x of notes)w.midi(128,x,0);for(let i=0;i<80;i++)tick();return {h,diff};};
  return {w,store,tick,press,go,save,listen};
}
(async()=>{
  // Every page offers four controls that move within their ranges.
  let e=await boot();
  for(const [page,button] of [[P.SCALE,3],[P.OFF_TAPE,2],[P.MODFX,2],[P.LFO2,5],[P.KEYS,7],[P.SPEED,7],[P.ERASE,7],[P.BT,7]]){
    e.go(button,page);
    for(let k=0;k<4;k++){const before=e.w.param(k);e.w.set_param(k,before+1);e.tick();e.w.set_param(k,before);e.tick();assert.equal(e.w.param(k),before,'page '+page+' knob '+k);}
  }
  // Each new sound control changes what is heard; with everything at its default nothing changes.
  async function effect(engine,voice,page,button,k,v,notes=[57,64]){
    const a=await boot(),b=await boot();
    for(const x of [a,b]){x.w.enc(1,engine);x.tick();if(voice!=null){x.go(6,1);x.w.set_param(0,voice);}x.tick();}
    b.go(button,page);b.w.set_param(k,v);b.tick();
    return [a.listen(notes),b.listen(notes)];
  }
  const cases=[
    ['RILEY Percussion',2,0,P.TIMBRE,6,2,8],['RILEY Leslie',2,1,P.TIMBRE,6,3,7],
    ['RADIGUE Sub',3,0,P.TIMBRE,6,2,8],['RADIGUE Drift',3,1,P.TIMBRE,6,3,8],
    ['ENO Glass Octave',1,2,P.TIMBRE,6,2,8],['ENO Rhodes Sympathetic',1,4,P.TIMBRE,6,3,8],
    ['STOCKHAUSEN Ring',4,null,P.RING,6,0,8],['STOCKHAUSEN Bow',4,null,P.RING,6,2,1],
    ['ENO Motion square',1,3,P.MOTION,6,2,2],['Transpose',0,null,P.SCALE,3,2,5],['Tune',1,0,P.SCALE,3,3,-30],
    ['ENSEMBLE chorus',2,0,P.MODFX,2,0,2],['ENSEMBLE phaser',1,3,P.MODFX,2,0,3],['ENSEMBLE flanger',3,0,P.MODFX,2,0,4],
  ];
  for(const [name,engine,voice,page,button,k,v] of cases){
    const [a,b]=await effect(engine,voice,page,button,k,v);
    assert.notEqual(a.h,b.h,name+' changes the sound');
  }
  // ENSEMBLE is stereo; with it off, left and right are equal.
  { const [off,on]=await effect(1,3,P.MODFX,2,0,1); assert(on.diff>off.diff*1.3,'ensemble widens the image '+off.diff+' '+on.diff); }
  // OFF-TAPE Pan moves the first head's signal.
  { const run=async pan=>{const x=await boot();x.w.enc(1,1);x.tick();x.go(2,P.PLATE??7);x.go(2,P.OFF_TAPE);x.w.set_param(0,100);x.w.set_param(2,pan);x.tick();
      x.press(13);return x.listen([60,64],500).diff;};
    const centre=await run(50),left=await run(0);assert(left>centre*1.3,'off-tape pan '+centre+' '+left); }
  // LFO 2 on pitch adds to the vibrato.
  { const a=await boot(),b=await boot();
    for(const x of [a,b]){x.w.enc(1,2);x.tick();}
    b.go(5,P.LFO2);b.w.set_param(0,30);b.w.set_param(2,3);b.w.set_param(3,80);b.tick();
    assert.notEqual(a.listen([60]).h,b.listen([60]).h,'LFO 2 to pitch'); }
  // Slow arpeggio: the slowest rate is one step every 20 seconds.
  { const steps=async rate=>{const x=await boot();x.w.enc(1,1);x.tick();x.go(10,P.ARP);x.w.set_param(0,1);x.w.set_param(1,rate);x.tick();
      assert.equal(x.w.param(1),rate);x.w.midi(144,60,100);x.w.midi(144,64,100);x.tick();let last=x.w.arp()&255,count=0;
      for(let i=0;i<1400;i++){x.tick();const s=x.w.arp()&255;if(s!==last){count++;last=s;}}return count;};   /* 8.1 s */
    assert.equal(await steps(-9),0,'20 s per step');assert(await steps(-4)>=3,'0.5 Hz');assert(await steps(3)>=20,'3 Hz as before'); }

  // New controls are stored with the program and global ones in the settings.
  e=await boot();e.w.midi(192,6,0);e.tick();e.w.enc(1,2);e.tick();
  e.go(6,1);e.w.set_param(0,1);e.go(6,P.TIMBRE);e.w.set_param(2,5);e.w.set_param(3,6);
  e.go(2,P.MODFX);e.w.set_param(0,1);e.w.set_param(1,44);e.w.set_param(2,77);e.w.set_param(3,61);
  e.go(5,P.LFO2);e.w.set_param(0,20);e.w.set_param(1,5);e.w.set_param(2,6);e.w.set_param(3,-40);
  e.go(3,P.SCALE);e.w.set_param(2,-3);e.w.set_param(3,12);
  e.go(10,P.ARP);e.w.set_param(1,-4);
  e.go(7,P.KEYS);e.w.set_param(1,1);e.w.set_param(2,90);e.w.set_param(3,15);
  e.go(7,P.SPEED);e.w.set_param(2,20);e.w.set_param(3,-12);
  e.go(7,P.BT);e.w.set_param(3,1);
  for(let i=0;i<200;i++)e.tick();
  e.save();for(let i=0;i<200;i++)e.tick();
  const x=await boot(e.store.slice());x.w.midi(192,6,0);x.tick();
  const read=(button,page)=>{x.go(button,page);return [0,1,2,3].map(k=>x.w.param(k));};
  assert.deepEqual(read(6,P.TIMBRE).slice(2),[5,6]);
  assert.deepEqual(read(2,P.MODFX),[1,44,77,61]);
  assert.deepEqual(read(5,P.LFO2),[20,5,6,-40]);
  assert.deepEqual(read(3,P.SCALE).slice(2),[-3,12]);
  assert.equal(read(10,P.ARP)[1],-4);
  assert.deepEqual(read(7,P.KEYS).slice(1),[1,90,15]);
  assert.deepEqual(read(7,P.SPEED).slice(2),[20,-12]);
  assert.equal(read(7,P.BT)[3],1);
  console.log('Four knobs per page; RILEY, RADIGUE, ENO, STOCKHAUSEN options, motion shape, transpose, tune, ensemble (stereo), off-tape pan, LFO 2, slow arpeggio; program and global storage.');
})().catch(e=>{console.error(e);process.exitCode=1;});
