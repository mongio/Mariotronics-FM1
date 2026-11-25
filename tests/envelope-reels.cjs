const fs=require('fs'),assert=require('node:assert/strict');
const html=fs.readFileSync(process.argv[2],'utf8');
const bytes=Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64');
async function boot() {
 const w=(await WebAssembly.instantiate(bytes)).instance.exports;w.init();let ms=0;
 function tick(){w.step(ms+=6);return Int16Array.from(new Int32Array(w.memory.buffer,w.audio(),512).filter((v,i)=>!(i%2)))}
 function press(id){w.button(id,1);tick();w.button(id,0);tick()}
 function page(p){w.enc(0,p-w.page());tick()}
 function render(n){return Buffer.concat(Array.from({length:n},()=>Buffer.from(tick().buffer)))}
 return {w,tick,press,page,render}
}
function energy(buf) { let n=0;for(let i=0;i<buf.length;i+=2)n+=buf.readInt16LE(i)**2;return Math.sqrt(n/(buf.length/2)) }
async function sound(engine,voice,adsr) {
 const {w,tick,render,page}=await boot();w.enc(1,engine);tick();page(1);w.set_param(0,voice);
 page(7);w.set_param(0,0);page(8);adsr.forEach((v,k)=>w.set_param(k,v));w.midi(144,60,110);
 const attack=energy(render(8));render(12);const decay=energy(render(18));render(64);const sustain=energy(render(8));w.midi(128,60,0);render(12);const release=energy(render(8));return [attack,decay,sustain,release]
}
(async()=>{
 for(const [engine,voice] of [[0,0],[0,1],[1,0],[1,1],[1,2],[1,3],[2,0],[2,1],[1,4],[1,5],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3]]){
 const base=await sound(engine,voice,[0,0,35,0]),a=await sound(engine,voice,[100,0,35,0]),d=await sound(engine,voice,[0,100,35,0]),s=await sound(engine,voice,[0,0,100,0]),r=await sound(engine,voice,[0,0,35,100]);
 assert(base[0]>a[0]*1.4);assert(d[1]>base[1]*1.15);assert(s[2]>base[2]*1.2);assert(r[3]>base[3]+4);
 }
 for(const eng of [0,1,2,3]){
 const {w,tick,render,page,press}=await boot();w.enc(1,eng);tick();
 const img=()=>{w.idle_preview();return Buffer.from(new Uint8Array(w.memory.buffer,w.fb(),240*146*2))};
 const stopped=img();press(10);assert.equal(w.page(),13);assert(!stopped.equals(img()));
 if(eng===1){press(6);press(6);press(6);assert.equal(w.page(),25);press(6);assert.equal(w.page(),24);press(6);assert.equal(w.page(),19)}
 press(8);press(13);render(710);press(13);page(0);
 const start=img();render(30);const end=img();assert(!start.equals(end));page(8);
 w.button(11,1);w.button(11,0);tick();render(30);assert.equal(w.page(),0);assert(w.state()&1024);
 assert(start.equals(img()),'Reversed reels must retrace frames'); 
 w.button(11,1);w.button(11,0);tick();render(29);assert(!(w.state()&1024));assert(end.equals(img()));
 }
 console.log('ADSR measured on sixteen voices; ARP and SEQ deck views and both reel directions verified on all four engines.');
})().catch(e=>{console.error(e);process.exitCode=1})
