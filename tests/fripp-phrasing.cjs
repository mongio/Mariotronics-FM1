const fs = require('node:fs');
const assert = require('node:assert/strict');
const html = fs.readFileSync(process.argv[2], 'utf8');
const moduleBytes = Buffer.from(html.match(/wasm='([^']+)'/)[1], 'base64');

async function instrument() {
  const w = (await WebAssembly.instantiate(moduleBytes)).instance.exports;
  w.init();
  let frames = 0;
  function block() {
    w.step(Math.floor(frames * 1000 / 44100));
    frames += 256;
    return new Int32Array(w.memory.buffer, w.audio(), 512).filter((_, i) => i % 2 === 0);
  }
  function button(id) {
    w.button(id, 1); block(); w.button(id, 0); block();
  }
  function controls(page, values) {
    const current = w.page() === 20 ? 19 : w.page();
    w.enc(0, (page === 20 ? 19 : page) - current);
    block();
    assert.equal(w.page(), page);
    values.forEach((v, k) => w.set_param(k, v));
  }
  controls(7, [0]); // measure dry attacks
  controls(8, [0, 0, 100, 0]);
  controls(10, [5, 0]);
  return { w, block, button, controls };
}
async function phrase(settings, secondKey) {
  const { w, block, button, controls } = await instrument();
  button(10);
  controls(13, [1, 5, 80, 0]);
  controls(20, settings);
  w.midi(144, 60, 100);
  const audio = [];
  for(let i=0;i<85;i++) {
    if(secondKey && i===8) w.midi(144, secondKey, 100);
    audio.push(...block());
  }
  return audio;
}
function energy(audio, begin, end) {
  const segment=audio.slice(begin,end);
  return Math.sqrt(segment.reduce((sum,x)=>sum+x*x,0)/segment.length);
}
function frequency(audio, offset) {
  let best=0, score=-1;
  for(let period=70;period<200;period++) {
    let xy=0,xx=0,yy=0;
    for(let i=offset;i<offset+600;i++) {
      let x=audio[i],y=audio[i+period];
      xy+=x*y;xx+=x*x;yy+=y*y;
    }
    const corr=xy/Math.sqrt(xx*yy);
    if(corr>score+0.0001){score=corr;best=period;}
  }
  return 44100/best;
}
(async()=>{
  const instant=await phrase([0,0,0,7]);
  const delayed=await phrase([20,0,0,7]);
  assert.equal(energy(delayed,0,8820),0);
  assert(energy(delayed,8900,10000)>100);
  const slow=await phrase([0,80,0,7]);
  assert(energy(slow,100,1000)<energy(instant,100,1000)*0.25);
  const third=frequency(await phrase([0,0,0,4]),10500);
  const fifth=frequency(instant,10500);
  assert(Math.abs(third-330)<10 && Math.abs(fifth-392)<10);
  const tied=await phrase([0,0,1,7]);
  assert(energy(tied,8250,8750)>2*energy(instant,8250,8750));
  const two=await phrase([20,0,1,7],65);
  assert(energy(two,8950,10100)>100); // adding a key does not restart Wait
  const {w,block,controls}=await instrument();
  controls(2,[1,50]);w.midi(144,60,100);
  for(let i=0;i<40;i++)block();
  w.midi(144,67,100);
  const slide=[];
  for(let i=0;i<52;i++)slide.push(...block());
  const pitches=[100,4800,11500].map(at=>frequency(slide,at));
  assert(pitches[0]<280 && pitches[1]>285 && pitches[1]<360 && pitches[2]>380);
  console.log('FRIPP measured: 200 ms wait, phrase rise, tied gate, two-key entry, third/fifth',third,fifth,'Hz; slide',pitches,'Hz.');
})().catch(e=>{console.error(e);process.exitCode=1});
