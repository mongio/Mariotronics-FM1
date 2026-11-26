const targetIdentity='FM-1_869';
const isFm1=id=>id.model==='FM-1'||id.model==='ota-FM-1';
let servicing=false;
async function service(status,action){
  if(servicing)return;
  servicing=true;
  const ids=['install','bankRead','bankWrite','power','bankTarget'];
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
  msg.textContent=identity+' → Mariotronics Stockhausen Lab · '+targetIdentity;
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
  if(dev.id.text!==targetIdentity && dev.id.text!=="FM-1_868" && dev.id.text!=="FM-1_857" && dev.id.text!=="FM-1_856" && dev.id.text!=="FM-1_855" && dev.id.text!=="FM-1_854"){dev.link.close();throw Error('USB preset transfer requires a running Mariotronics 1.4 through 1.7 or version 1.18.');}
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
  const saved=JSON.parse(await file.text()),raw=bytes(saved.data);
  if(saved.format!=='Mariotronics presets 1'||await digest(raw)!==saved.sha256)throw Error('Invalid or damaged preset backup.');
  const conn=await presetConnection();
  try{
    if(!confirm('Replace all 64 saved preset slots on '+conn.name+' with '+file.name+'?'))return;
    const [count]=await exchange(conn,3,0);
    if(Math.ceil(raw.length/80)!==count)throw Error('Incompatible bank size.');
    for(let i=0;i<count;i++){
      const digits=[];for(const b of raw.slice(i*80,(i+1)*80))digits.push(b>>4,b&15);
      await exchange(conn,4,i,digits);
    }
    await exchange(conn,5,0);conn.commit();msg.textContent='Restored 64 preset slots on '+conn.name+'.';
  }finally{conn.close();}
});
