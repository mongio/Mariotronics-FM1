const targetIdentity='FM-1_879';
const isFm1=id=>id.model==='FM-1'||id.model==='ota-FM-1';
let servicing=false;
async function service(status,action){
  if(servicing)return;
  servicing=true;
  const ids=['install','pianoRead','pianoSustain','pianoFactory','pianoWrite','bankRead','bankWrite','power','bankTarget'];
  const previous=ids.map(id=>$(id).disabled);
  ids.forEach(id=>$(id).disabled=true);
  try{await action($(status));}
  catch(e){$(status).textContent=e.message;}
  finally{ids.forEach((id,i)=>$(id).disabled=previous[i]);servicing=false;}
}
async function usbDevice(){
  if(!navigator.requestMIDIAccess)throw Error('Open this editor in desktop Chrome or Edge with USB MIDI access.');
  const up=new Updater(await navigator.requestMIDIAccess({sysex:true}));
  const dev=await up.find(isFm1);
  if(!dev)throw Error('FM-1 not found. Check USB and see the recovery steps below.');
  return {dev,up};
}
$('kit').onclick=()=>download(rescueKit,'FM1_GRANA_rescue.zip');
$('install').onclick=()=>service('installNote',async msg=>{
  msg.textContent='Identifying FM-1…';
  const {dev,up}=await usbDevice();
  const loader=dev.id.model==='ota-FM-1';
  const identity=dev.name+' · '+dev.id.text;
  msg.textContent=identity+' → Mariotronics 1.20 · '+targetIdentity;
  const leave=e=>{e.preventDefault();e.returnValue='';};
  let lock,paused=false;
  try{
    if(!confirm(msg.textContent+'\n\n'+(loader?'Resume':'Install')+' firmware? Keep power and USB connected until verification finishes.'))return;
    window.addEventListener('beforeunload',leave);
    if(audio && audio.state==='running'){await audio.suspend();paused=true;}
    try{lock=await navigator.wakeLock?.request('screen');}catch{}
    const image=logicalImage(bytes(firmware));
    $('progress').hidden=false;$('progressFill').style.width='0';
    const step=(phase,n)=>{
      const words={start:'FM-1 identified',verify:'Checking package',loader:'Starting update loader',write:'Writing firmware',reboot:'Verifying identity after restart',done:'Installed and verified · '+n};
      msg.textContent=words[phase]||phase;
      if(phase==='write')$('progressFill').style.width=Math.min(99,100*n/Math.ceil(image.length/512))+'%';
      if(phase==='done')$('progressFill').style.width='100%';
    };
    if(loader){
      if(!await up.resume(image,step,targetIdentity,dev))throw Error('Transfer interrupted. Reconnect and select Identify and install to resume.');
    }else await up.install(image,targetIdentity,step,dev);
  }finally{
    dev.link.close();window.removeEventListener('beforeunload',leave);
    if(lock)await lock.release();if(paused && audio)await audio.resume();
  }
});

const presetPrefix=[0x7d,0x47,0x52,0x4d];
async function presetConnection(){
  if($('bankTarget').value==='sim'){
    if(audio || starting)throw Error('Power off the simulator first.');
    const w=(await WebAssembly.instantiate(bytes(wasm))).instance.exports;
    const store=new Uint8Array(w.memory.buffer,w.store(),w.store_len());
    const previous=localStorage.getItem('mt.programBank.v1');
    if(previous)store.set(bytes(previous));
    w.init();
    return {name:'simulator',close(){},async request(frame){
      new Uint8Array(w.memory.buffer,w.sx_in(),frame.length).set(frame);
      w.sx_feed(frame.length);
      return Array.from(new Uint8Array(w.memory.buffer,w.sx_out(),w.sx_out_len()));
    },commit(){localStorage.setItem('mt.programBank.v1',btoa(String.fromCharCode(...store)));}};
  }
  const {dev}=await usbDevice();
  if(dev.id.text!==targetIdentity && dev.id.text!=="FM-1_878" && dev.id.text!=="FM-1_877" && dev.id.text!=="FM-1_876" && dev.id.text!=="FM-1_875" && dev.id.text!=="FM-1_874" && dev.id.text!=="FM-1_873" && dev.id.text!=="FM-1_872" && dev.id.text!=="FM-1_871" && dev.id.text!=="FM-1_870" && dev.id.text!=="FM-1_869" && dev.id.text!=="FM-1_868" && dev.id.text!=="FM-1_857" && dev.id.text!=="FM-1_856" && dev.id.text!=="FM-1_855" && dev.id.text!=="FM-1_854"){dev.link.close();throw Error('USB preset transfer requires a running Mariotronics 1.4 through 1.7, 1.18, 1.19 or 1.20.');}
  return {name:dev.id.text,close:()=>dev.link.close(),commit(){},async request(frame){
    dev.link.drain();
    if(!dev.link.send([0xf0,...frame,0xf7]))throw Error('FM-1 disconnected.');
    const until=Date.now()+6000;
    while(Date.now()<until){
      const packet=await dev.link.read(until-Date.now());
      if(packet && presetPrefix.every((v,i)=>packet[i+1]===v))return packet;
    }
    throw Error('No preset reply. Reconnect and retry.');
  }};
}
async function exchange(conn,op,index,data=[]){
  const packet=await conn.request([...presetPrefix,op,index,...data]);
  if(!packet || packet[0]!==240 || packet.at(-1)!==247 || !presetPrefix.every((v,i)=>packet[i+1]===v) || packet[5]!==op || packet[6]!==index || packet[7]!==0)
    throw Error('The instrument rejected the preset transfer.');
  return packet.slice(8,-1);
}
async function digest(raw){
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',raw)),n=>n.toString(16).padStart(2,'0')).join('');
}
$('bankRead').onclick=()=>service('bankStatus',async msg=>{
  const conn=await presetConnection();
  try{
    const [count]=await exchange(conn,1,0),raw=[];
    for(let i=0;i<count;i++){
      const digits=await exchange(conn,2,i);
      if(!digits.length || digits.length%2 || digits.some(n=>n>15))throw Error('Incomplete preset reply.');
      for(let k=0;k<digits.length;k+=2)raw.push(digits[k]*16+digits[k+1]);
    }
    const data=Uint8Array.from(raw),file={format:'Mariotronics presets 1',sha256:await digest(data),data:btoa(String.fromCharCode(...data))};
    download(btoa(JSON.stringify(file)),'Mariotronics-'+conn.name+'.mtbank');
    msg.textContent='Exported 64 preset slots from '+conn.name+'.';
  }finally{conn.close();}
});
$('bankWrite').onclick=()=>{$('bankFile').value='';$('bankFile').click();};
$('bankFile').onchange=()=>service('bankStatus',async msg=>{
  const file=$('bankFile').files[0];if(!file)return;
  if(file.size>16000)throw Error('Invalid preset file size.');
  const saved=JSON.parse(await file.text());let raw=bytes(saved.data);
  if(saved.format!=='Mariotronics presets 1'||await digest(raw)!==saved.sha256)throw Error('Invalid or damaged preset backup.');
  const conn=await presetConnection();
  try{
    if(!confirm('Replace all 64 saved preset slots on '+conn.name+' with '+file.name+'?'))return;
    const [count,sizeHigh,sizeLow]=await exchange(conn,3,0);
    const size=sizeHigh===undefined?raw.length:sizeHigh*128+sizeLow;
    if(raw.length<size){                         // older backups: Lab 6 has no Lab record, Lab 7 has 8-byte records
      const padded=new Uint8Array(size),base=5056;
      if(raw.length===base+512){padded.set(raw.subarray(0,base));for(let i=0;i<64;i++)padded.set(raw.subarray(base+i*8,base+i*8+8),base+i*32);}
      else padded.set(raw);
      raw=padded;
    }
    if(Math.ceil(raw.length/80)!==count)throw Error('Incompatible bank size.');
    for(let i=0;i<count;i++){
      const digits=[];for(const b of raw.slice(i*80,(i+1)*80))digits.push(b>>4,b&15);
      await exchange(conn,4,i,digits);
    }
    await exchange(conn,5,0);conn.commit();msg.textContent='Restored 64 preset slots on '+conn.name+'.';
  }finally{conn.close();}
});

/* Piano samples: twelve slots (six notes, soft and hard). Bank format v2, see firmware/src/mt_eno_bank.h. */
const pianoNotes=['C2','A2','F♯3','D♯4','C5','A5'],pianoHz=[65.41,110,185,311.1,523.3,880],tailBlocks=665,headFrames=28544;
const BANK_MAX=0x45000,HEADER=128,BLOCK=35,MAX_BLOCKS=2048;
const imaSteps=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767],imaAdjust=[-1,-1,-1,-1,2,4,6,8];
function imaEncode(pcm){            // the same 64-frame blocks as tools/piano.py
  const blocks=Math.ceil(pcm.length/64),out=new Uint8Array(blocks*BLOCK);let idx=35;
  for(let b=0;b<blocks;b++){
    const start=b*64,o=b*BLOCK;let pred=pcm[start]|0;
    out[o]=pred&255;out[o+1]=(pred>>8)&255;out[o+2]=idx;
    const codes=[];
    for(let i=1;i<64;i++){
      const sample=start+i<pcm.length?pcm[start+i]:0,diff=sample-pred,step=imaSteps[idx];
      const mag=Math.min(7,Math.floor(Math.abs(diff)*4/step)),code=mag+(diff<0?8:0);
      const change=(step>>3)+(mag&4?step:0)+(mag&2?step>>1:0)+(mag&1?step>>2:0);
      pred=Math.max(-32768,Math.min(32767,pred+(code&8?-change:change)));
      idx=Math.max(0,Math.min(88,idx+imaAdjust[mag]));codes.push(code);
    }
    codes.push(0);
    for(let i=0;i<32;i++)out[o+3+i]=codes[2*i]|(codes[2*i+1]<<4);
  }
  return out;
}
const pianoSlots=Array.from({length:12},()=>({mode:1,data:null,name:''}));   // 0 factory, 1 sustain, 2 your file, 3 kept on the FM-1
let pianoKept=null;                 // the FM-1's current bank, when read: needed to keep its own files
function slotBlocks(z){const s=pianoSlots[z];return s.mode===1?tailBlocks:s.mode>=2?s.data.length/BLOCK:0;}
function pianoBytes(){let n=HEADER;for(let z=0;z<12;z++)n+=slotBlocks(z)*BLOCK;return n;}
function seconds(z){const s=pianoSlots[z];return s.mode===0?headFrames/16000:s.mode===1?(headFrames+tailBlocks*64)/16000:s.data.length/BLOCK*64/16000;}
function drawPianoSlots(){
  const t=$('pianoSlots');if(!t)return;
  t.innerHTML='<tr><th>Note</th><th>Soft</th><th>Hard</th></tr>';
  pianoNotes.forEach((note,r)=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`<td><b>${note}</b><br><small>record at ${pianoHz[r]} Hz</small></td>`;
    for(let layer=0;layer<2;layer++){
      const z=2*r+layer,s=pianoSlots[z],td=document.createElement('td'),sel=document.createElement('select');
      sel.setAttribute('aria-label',`${note} ${layer?'hard':'soft'}`);
      [['0','Factory'],['1','Sustain'],['2','Your file…']].concat(s.mode===3?[['3','Kept on the FM-1']]:[]).forEach(([v,l])=>{const o=document.createElement('option');o.value=v;o.textContent=l;sel.append(o);});
      sel.value=String(s.mode===2&&!s.data?1:s.mode);
      sel.onchange=()=>{const v=+sel.value;if(v===2){pickPianoFile(z);sel.value=String(s.mode);return;}s.mode=v;if(v<2){s.data=null;s.name='';}drawPianoSlots();};
      td.append(sel);
      const info=document.createElement('small');info.textContent=' '+seconds(z).toFixed(1)+' s'+(s.name?' · '+s.name:'');td.append(document.createElement('br'),info);
      tr.append(td);
    }
    t.append(tr);
  });
  const used=pianoBytes();
  $('pianoFill').style.width=Math.min(100,100*used/BANK_MAX)+'%';
  $('pianoUse').textContent=`Sample memory: ${Math.round(used/1024)} of ${Math.round(BANK_MAX/1024)} KB`+(used>BANK_MAX?' · too long: shorten a file or use Sustain/Factory elsewhere':'');
  $('pianoWrite').disabled=used>BANK_MAX;
}
function pickPianoFile(z){
  const input=$('pianoFile');input.value='';
  input.onchange=()=>service('pianoStatus',async msg=>{
    const file=input.files[0];if(!file)return;
    msg.textContent='Reading '+file.name+'…';
    const raw=await file.arrayBuffer();
    const decoded=await new OfflineAudioContext(1,1,44100).decodeAudioData(raw);
    const frames=Math.ceil(decoded.duration*16000),off=new OfflineAudioContext(1,frames,16000);
    const src=off.createBufferSource();src.buffer=decoded;src.connect(off.destination);src.start();
    const y=(await off.startRendering()).getChannelData(0);
    let peak=0;for(const v of y)peak=Math.max(peak,Math.abs(v));
    if(!peak)throw Error(file.name+' is silent.');
    let onset=0;while(onset<y.length&&Math.abs(y[onset])<=peak*0.003)onset++;
    onset=Math.max(0,onset-16);
    const others=pianoBytes()-slotBlocks(z)*BLOCK,room=Math.floor((BANK_MAX-others)/BLOCK);
    const blocks=Math.min(MAX_BLOCKS,room,Math.ceil((y.length-onset)/64));
    if(blocks<64)throw Error('Not enough sample memory left. Set another slot to Factory or Sustain.');
    const gain=(z%2?19500:13500)/peak,pcm=new Int16Array(blocks*64);
    for(let i=0;i<pcm.length;i++){const v=y[onset+i]||0;pcm[i]=Math.max(-32767,Math.min(32767,Math.round(v*gain)));}
    for(let i=0;i<64*8&&i<pcm.length;i++)pcm[pcm.length-1-i]=Math.round(pcm[pcm.length-1-i]*i/(64*8));   // a short fade at the end
    pianoSlots[z]={mode:2,data:imaEncode(pcm),name:file.name};
    drawPianoSlots();
    msg.textContent=`${file.name}: ${(blocks*64/16000).toFixed(1)} s for ${pianoNotes[z>>1]} ${z%2?'hard':'soft'}`+(blocks*64<y.length-onset?' (shortened to fit)':'')+'. Write to FM-1 to install.';
  });
  input.click();
}
function pianoImage(){
  const size=pianoBytes(),img=new Uint8Array(size);
  const sig=new TextEncoder().encode('MTEP2-SAMPLES');img.set(sig,0);
  const tail=bytes(enoTail);let at=HEADER;
  for(let z=0;z<12;z++){
    const s=pianoSlots[z],blocks=slotBlocks(z),r=32+8*z,mode=s.mode===3?2:s.mode;
    if(blocks){
      if(s.mode===1)img.set(tail.subarray(16+BLOCK*z*tailBlocks,16+BLOCK*(z+1)*tailBlocks),at);else img.set(s.data,at);
      img[r]=at&255;img[r+1]=(at>>8)&255;img[r+2]=(at>>16)&255;img[r+3]=at>>>24;
      img[r+4]=blocks&255;img[r+5]=blocks>>8;img[r+6]=mode;at+=blocks*BLOCK;
    }
  }
  let hash=2166136261;for(let i=HEADER;i<size;i++)hash=Math.imul(hash^img[i],16777619)>>>0;
  const put32=(o,v)=>{img[o]=v&255;img[o+1]=(v>>8)&255;img[o+2]=(v>>16)&255;img[o+3]=v>>>24;};
  put32(16,size);put32(20,hash);
  return img;
}
async function pianoLink(){
  const {dev}=await usbDevice();
  if(dev.id.text!==targetIdentity){dev.link.close();throw Error('Install Mariotronics 1.20 on the FM-1 first (Identify and install).');}
  const request=async(op,lo,data=[],wait=8000)=>{
    dev.link.drain();
    if(!dev.link.send([240,...presetPrefix,op,lo,...data,247]))throw Error('FM-1 disconnected.');
    const until=Date.now()+wait;
    while(Date.now()<until){
      const p=await dev.link.read(until-Date.now());
      if(p?.length>=9 && p[0]===240 && p[p.length-1]===247 && presetPrefix.every((v,i)=>p[i+1]===v) && p[5]===op && p[6]===lo)return p;
    }
    throw Error('USB timeout. Reconnect and try again.');
  };
  return {dev,request};
}
const bankNames=['empty','sustain (earlier version)','piano samples','another firmware’s samples (GRANA or VIRGO)'];
$('pianoRead').onclick=()=>service('pianoStatus',async msg=>{
  const {dev,request}=await pianoLink();
  try{
    const p=await request(13,0),version=p[8],modes=Array.from(p.slice(9,21));
    if(version===3){msg.textContent='The sample memory holds another firmware’s samples (GRANA or VIRGO). ENO plays its factory recordings. Writing piano samples would erase them.';return;}
    modes.forEach((m,z)=>{pianoSlots[z]=m===2?{mode:3,data:null,name:'on the FM-1'}:{mode:m,data:null,name:''};});
    pianoKept=modes.some(m=>m===2)?true:null;
    drawPianoSlots();
    msg.textContent='Read from the FM-1: '+bankNames[version]+'.';
  }finally{dev.link.close();}
});
$('pianoSustain').onclick=()=>{pianoSlots.forEach((s,z)=>{pianoSlots[z]={mode:1,data:null,name:''};});drawPianoSlots();};
$('pianoFactory').onclick=()=>{pianoSlots.forEach((s,z)=>{pianoSlots[z]={mode:0,data:null,name:''};});drawPianoSlots();};
$('pianoWrite').onclick=()=>service('pianoStatus',async msg=>{
  const {dev,request}=await pianoLink();
  try{
    const status=await request(13,0);let force=0;
    if(status[8]===3){
      if(!confirm('This FM-1 holds another firmware’s samples (GRANA or VIRGO). Writing the piano samples ERASES them. Save them first with that firmware’s editor if you want them back.\n\nErase them and write the piano samples?'))return;
      force=1;
    }
    // Recordings kept on the FM-1 are read back first, so writing does not lose them.
    for(let z=0;z<12;z++)if(pianoSlots[z].mode===3){
      const info=await request(16,z),off=info[8]|info[9]<<7|info[10]<<14,blocks=info[11]|info[12]<<7,data=new Uint8Array(blocks*BLOCK);
      const first=Math.floor(off/64),last=Math.floor((off+data.length-1)/64);
      for(let piece=first;piece<=last;piece++){
        const p=await request(15,piece&127,[piece>>7]);
        for(let i=0;i<64;i++){const at=piece*64+i-off;if(at>=0&&at<data.length)data[at]=p[8+2*i]*16+p[9+2*i];}
        if(!(piece%32))msg.textContent=`Reading ${pianoNotes[z>>1]} ${z%2?'hard':'soft'} from the FM-1 · ${Math.floor(100*(piece-first)/(last-first+1))}%`;
      }
      pianoSlots[z]={mode:3,data,name:'on the FM-1'};
    }
    if(pianoSlots.every(s=>s.mode===0)){
      if((await request(14,force))[7]===2)throw Error('Sample memory not erased.');
      msg.textContent='Factory piano restored: the sample memory is empty.';return;
    }
    const img=pianoImage();
    const begin=await request(10,force,[img.length&127,(img.length>>7)&127,(img.length>>14)&127],60000);
    if(begin[7]===2)throw Error('Another firmware’s samples are in the sample memory; nothing was changed.');
    if(begin[7])throw Error('Sample memory unavailable.');
    for(let off=0;off<img.length;off+=128){
      const index=off/128,data=[index>>7];
      for(const b of img.subarray(off,off+128))data.push(b>>4,b&15);
      // 1.20: a piece whose answer does not come is sent again (the FM-1 accepts a repeat of the last one)
      let reply=null;
      for(let attempt=0;attempt<5 && !reply;attempt++){
        try{reply=await request(11,index&127,data,attempt<4?4000:20000);}
        catch(e){if(attempt===4)throw Error('USB timeout at '+Math.round(100*off/img.length)+'%. Reconnect and write again.');msg.textContent='No answer, sending again · '+Math.floor(100*off/img.length)+'%';}
      }
      if(reply[7])throw Error('Writing failed at '+Math.round(100*off/img.length)+'%. Reconnect and write again.');
      if(!(index%16))msg.textContent='Writing piano samples · '+Math.floor(100*off/img.length)+'%';
    }
    if((await request(12,0,[],20000))[7])throw Error('Verification failed. Write again.');
    msg.textContent='Piano samples written and verified. ENO uses them now.';
  }finally{dev.link.close();}
});
drawPianoSlots();
