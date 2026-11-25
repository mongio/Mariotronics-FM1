const fs=require('node:fs'),assert=require('node:assert/strict');
const html=fs.readFileSync(process.argv[2],'utf8');
const binary=Buffer.from(html.match(/wasm='([^']+)'/)[1],'base64');
async function deck(){
 const w=(await WebAssembly.instantiate(binary)).instance.exports;w.init();let frames=0;
 function tick(){w.step(Math.floor(frames*1000/44100));frames+=256;return Int16Array.from(new Int32Array(w.memory.buffer,w.audio(),512).filter((_,i)=>i%2===0))}
 function run(n){let buf=[];for(let i=0;i<n;i++)buf.push(...tick());return buf;}
 function button(k){w.button(k,1);tick();w.button(k,0);tick()}
 w.enc(1,2);tick();w.enc(0,7);tick();w.set_param(0,0);button(8);
 w.set_param(1,100);w.set_param(2,90);w.set_param(3,100);
 button(13);w.midi(144,60,110);run(700);button(13);w.midi(128,60,0);run(140);
 return {w,tick,run,button};
}
function rms(a){return Math.sqrt(a.reduce((s,v)=>s+v*v,0)/a.length)}
function pitch(a){let best=0,score=-1;for(let p=150;p<195;p++){let xx=0,xy=0,yy=0;for(let i=0;i<a.length-p;i++){xx+=a[i]**2;xy+=a[i]*a[i+p];yy+=a[i+p]**2}let v=xy/Math.sqrt(xx*yy);if(v>score){score=v;best=p}}return 44100/best}
(async()=>{
 for(const rev of [false,true]){
  const {w,run,tick,button}=await deck();
  if(rev)button(11);
  w.enc(0,-1);tick();assert.equal(w.page(),21);
  w.set_param(0,1);w.set_param(1,1);w.button(13,1);
  const monitored=rms(run(60));w.button(13,0);tick();
  assert.equal(w.param(1),0);assert.equal(w.state()>>4&3,3);
  run(632);let gap=rms(run(35));assert(monitored>100 && gap<monitored*.1);
  run(40);assert(rms(run(18))>100);
 }
 const {w,run,tick,button}=await deck();
 let start=pitch(run(7));w.set_param(0,43);run(1);const down=pitch(run(7));
 run(75);w.set_param(0,42);run(1);const up=pitch(run(7));assert(down<start*.986 && up>start*1.014);
 button(12);w.set_param(0,40);tick();assert.equal(rms(run(8)),0);
 button(12);assert(Math.abs(pitch(run(7))-start)<3);
 w.button(13,1);run(220);w.button(13,0);tick();assert.equal(w.state()>>4&3,0);
 console.log('Tape return Hz (steady / farther / closer):',start,down,up,'; forward and reverse range erase, stop and whole erase verified.');
})().catch(e=>{console.error(e);process.exitCode=1});
