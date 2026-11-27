// ARP FEEL (swing, accent, repeats), two Wi-Fi routes, persistence and migration of older banks.
const assert=require('node:assert/strict'),fs=require('node:fs');
const load=f=>WebAssembly.compile(Buffer.from(fs.readFileSync(f,'utf8').match(/wasm='([^']+)'/)[1],'base64'));
(async()=>{
  const mod=await load(process.argv[2]);
  async function boot(saved,m=mod){
    const w=(await WebAssembly.instantiate(m)).exports;
    const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());
    if(saved)store.set(saved);
    w.init();let time=0;
    const tick=(ms=6)=>{time+=ms;w.step(time);w.audio();};
    const press=id=>{w.button(id,1);tick();w.button(id,0);tick();};
    const select=i=>{w.midi(192,i,0);tick();};
    const save=()=>{w.button(9,1);tick();tick(1001);w.button(9,0);tick();w.button(9,1);tick();w.button(9,0);tick();};
    return {w,store,tick,press,select,save};
  }
  const lengths=(e,steps)=>{       // samples between arpeggio steps, measured on the audio clock
    const out=[];let last=e.w.arp()&255,count=0;
    while(out.length<steps){e.w.audio();e.w.step(0);count+=256;const s=e.w.arp()&255;if(s!==last){out.push(count);count=0;last=s;}}
    return out.slice(1);
  };

  let e=await boot();
  e.select(5);e.w.enc(1,1);e.tick();
  e.press(10);e.w.set_param(0,1);e.w.set_param(1,8);e.tick();       // ARP: Climb, 8 Hz
  e.press(10);                                                       // ARP FEEL
  assert.deepEqual([0,1,2,3].map(k=>e.w.param(k)),[0,1,0,1],'defaults: straight, 3+3+2, no human, x1');
  e.w.midi(144,60,100);for(let i=0;i<50;i++)e.tick();
  let straight=lengths(e,6);
  for(const n of straight)assert(Math.abs(n-straight[0])<=256,'straight steps are even: '+straight);
  e.w.set_param(0,50);e.tick();
  let swung=lengths(e,8);
  const longs=swung.filter((_,i)=>i%2===0),shorts=swung.filter((_,i)=>i%2===1);
  const ratio=Math.max(...longs,...shorts)/Math.min(...longs,...shorts);
  assert(ratio>2.5 && ratio<3.5,'swing 75% gives a 3:1 pair: '+swung);
  e.w.set_param(1,3);e.w.set_param(2,40);e.w.set_param(3,3);e.tick();
  e.w.midi(128,60,0);e.tick();

  // Two routes.
  e.press(5);e.press(5);e.press(5);e.press(5);                                  // LFO -> SOURCE -> ROUTING
  e.w.set_param(0,3);e.w.set_param(1,80);e.w.set_param(2,7);e.w.set_param(3,-60);e.tick();
  assert.deepEqual([0,1,2,3].map(k=>e.w.param(k)),[3,80,7,-60]);
  e.save();

  // Reboot: everything returns with the program.
  e=await boot(e.store.slice());e.select(5);
  e.press(10);e.press(10);
  assert.deepEqual([0,1,2,3].map(k=>e.w.param(k)),[50,3,40,3],'ARP FEEL recalled');
  e.press(5);e.press(5);e.press(5);e.press(5);
  assert.deepEqual([0,1,2,3].map(k=>e.w.param(k)),[3,80,7,-60],'both routes recalled');

  // A bank written by Lab 6 loads with the defaults.
  if(process.argv[3]){
    const old=await boot(null,await load(process.argv[3]));
    old.select(7);old.w.enc(1,1);old.tick();old.press(10);old.w.set_param(0,3);old.tick();old.save();
    e=await boot(old.store.slice());e.select(7);e.press(10);
    assert.equal(e.w.param(0),3,'older program recalled');
    e.press(10);assert.deepEqual([0,1,2,3].map(k=>e.w.param(k)),[0,1,0,1],'older program: default feel');
  }

  // The backup handshake reports the bank size, so the editor can pad older backups.
  const sx=frame=>{new Uint8Array(e.w.memory.buffer,e.w.sx_in(),frame.length).set(frame);e.w.sx_feed(frame.length);return Array.from(new Uint8Array(e.w.memory.buffer,e.w.sx_out(),e.w.sx_out_len()));};
  const reply=sx([0x7d,0x47,0x52,0x4d,3,0]);
  const [count,hi,lo]=reply.slice(8,-1),size=hi*128+lo;
  assert(count===Math.ceil(size/80) && size>5056,'size '+size+' in '+count+' chunks');
  // An older (Lab 6) backup, padded with zeros by the editor, restores.
  if(process.argv[3]){
    const old=await boot(null,await load(process.argv[3]));
    old.select(9);old.press(8);old.w.set_param(1,33);old.save();
    const osx=frame=>{new Uint8Array(old.w.memory.buffer,old.w.sx_in(),frame.length).set(frame);old.w.sx_feed(frame.length);return Array.from(new Uint8Array(old.w.memory.buffer,old.w.sx_out(),old.w.sx_out_len()));};
    const P=[0x7d,0x47,0x52,0x4d],[n]=osx([...P,1,0]).slice(8,-1),raw=[];
    for(let i=0;i<n;i++){const d=osx([...P,2,i]).slice(8,-1);for(let k=0;k<d.length;k+=2)raw.push(d[k]*16+d[k+1]);}
    assert.equal(raw.length,5056);
    const padded=new Uint8Array(size);padded.set(raw);
    assert.equal(sx([...P,3,0])[7],0);
    for(let i=0;i<count;i++){const digits=[];for(const b of padded.slice(i*80,(i+1)*80))digits.push(b>>4,b&15);assert.equal(sx([...P,4,i,...digits])[7],0);}
    assert.equal(sx([...P,5,0])[7],0,'padded older backup accepted');
    e.select(9);e.press(8);assert.equal(e.w.param(1),33);
  }
  console.log('ARP FEEL swing 3:1, accents, repeats; two routes; reboot recall; Lab 6 bank defaults; backup size handshake; padded Lab 6 backup restores.');
})().catch(e=>{console.error(e);process.exitCode=1;});
