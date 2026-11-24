// Exercise factory recall, all writable slots, reboot and interrupted saves.
const assert=require('node:assert/strict'), fs=require('node:fs');
async function check() {
  const html=fs.readFileSync(process.argv[2],'utf8');
  const mod=await WebAssembly.compile(Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64'));
  async function boot(saved) {
    const w=(await WebAssembly.instantiate(mod)).exports;
    const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());
    if(saved)store.set(saved);
    w.init();
    let time=0;
    function tick(ms=6){time+=ms;w.step(time);w.audio()}
    function select(i){w.midi(192,i,0);tick();assert.equal(w.state()>>>16,i)}
    function save(){w.button(9,1);tick();tick(1001);w.button(9,0);tick()}
    return {w,store,select,save,tick};
  }
  let e=await boot(), sounds=new Set();
  for(let i=0;i<64;i++){
    e.select(i);
    const values=[];
    for(const page of [0,1,2,3,6,7,14]){
      e.w.enc(0,page-(e.w.state()&15));e.tick();
      for(let k=0;k<4;k++)values.push(e.w.param(k));
    }
    sounds.add(values.join(','));
  }
  assert.equal(sounds.size,64);
  e.w.enc(0,-14);e.tick();
  for(let i=0;i<64;i++){e.select(i);e.w.set_param(1,i);e.save()}
  const before=e.store.slice();e=await boot(before);assert.equal(e.w.state()>>>16,0);
  for(let i=0;i<64;i++){e.select(i);assert.equal(e.w.param(1),i)}
  e.w.set_param(1,88);e.save();
  const damaged=e.store.slice();damaged[2052+20]^=255;
  const recovered=await boot(damaged);recovered.select(63);assert.equal(recovered.w.param(1),63);
  console.log('64 distinct factory programs, 64 saved edits survive reboot, previous bank recovered');
}
check().catch(e=>{console.error(e);process.exitCode=1});
