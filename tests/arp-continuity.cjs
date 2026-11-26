const fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
  const html=fs.readFileSync(process.argv[2],'utf8');
  const bytes=Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64');
  for(const engine of [1,2,3]){
    const w=(await WebAssembly.instantiate(bytes)).instance.exports;
    w.init();let n=0;
    const tick=()=>{w.step(Math.floor(n*1000/44100));n+=256;w.audio();};
    const press=id=>{w.button(id,1);tick();w.button(id,0);tick();};
    w.enc(1,engine);tick();
    press(10);                       // ARP page
    w.set_param(0,1);tick();         // Climb
    const arp=()=>{const v=w.arp();return {step:v&255,size:(v>>8)&255,left:v>>>16};};
    w.midi(144,60,100);
    for(let i=0;i<300;i++)tick();
    const before=arp();assert.equal(before.size,1);
    w.midi(144,64,100);tick();
    const added=arp();
    assert.equal(added.size,2);
    assert.equal(added.step,before.step,'engine '+engine+': adding a note restarted the arpeggio');
    assert(added.left>0 && added.left<before.left+1000,'engine '+engine+': step timer restarted');
    w.midi(128,60,0);tick();
    const lifted=arp();assert.equal(lifted.size,1);assert.equal(lifted.step,before.step);
    w.midi(128,64,0);tick();assert.equal(arp().size,0);
  }
  console.log('ENO, RILEY and RADIGUE arpeggios keep their place when notes are added or lifted.');
})().catch(e=>{console.error(e);process.exitCode=1;});
