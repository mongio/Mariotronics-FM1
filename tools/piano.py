"""Build the factory and optional sustain banks from Salamander Grand Piano v3."""
from pathlib import Path
import numpy as np

rate = 16000
frames = 28544
roots = [36, 45, 54, 63, 72, 81]

bank = []
steps = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
adjust = [-1,-1,-1,-1,2,4,6,8]

def encode(wave):
    # Independent 64-frame IMA blocks support random access in either direction.
    idx=35
    for start in range(0,len(wave),64):
        pred=int(wave[start])
        yield from (pred&255,(pred>>8)&255,idx)
        codes=[]
        for sample in wave[start+1:start+64]:
            diff=int(sample)-pred
            step=steps[idx]
            mag=min(7,abs(diff)*4//step)
            code=mag+(8 if diff<0 else 0)
            change=(step>>3)+(step if mag&4 else 0)+(step>>1 if mag&2 else 0)+(step>>2 if mag&1 else 0)
            pred=max(-32768,min(32767,pred+(-change if code&8 else change)))
            idx=max(0,min(88,idx+adjust[mag]))
            codes.append(code)
        codes.append(0)
        yield from (a|(b<<4) for a,b in zip(codes[::2],codes[1::2]))

# Input recordings are pinned so the factory and extension always match.
from urllib.parse import quote
import subprocess
import sys
import tempfile
source = "https://raw.githubusercontent.com/sfzinstruments/SalamanderGrandPiano/3382bf9496bba2486f5ab0de55a264d1dfc38404/Samples/"
cache = Path(sys.argv[1]) if len(sys.argv)>1 else Path(tempfile.gettempdir())/"salamander-v3"
cache.mkdir(exist_ok=True)
tail_frames = 42560
tails = bytearray(b"MTEP1-SAL-V3" + bytes(4))
for root, name in zip(roots, ["C2","A2","F#3","D#4","C5","A5"]):
    for velocity in (5,13):
        path = cache / f"{name}v{velocity}.flac"
        if not path.exists():
            subprocess.run(["curl","-fLsS","--retry","2",source+quote(path.name),"-o",str(path)],check=True)
        raw = subprocess.check_output(["ffmpeg","-v","error","-i",str(path),"-ac","1","-ar",str(rate),"-f","f32le","-"])
        y = np.frombuffer(raw,dtype="<f4")
        # Retain the hammer transient, trimming only the leading recording silence.
        onset = np.flatnonzero(abs(y)>np.max(abs(y))*0.003)[0]
        y = y[max(0,onset-16):][:frames+tail_frames].copy()
        assert len(y)==frames+tail_frames
        y *= (13500 if velocity==5 else 19500)/np.max(abs(y))
        pcm = np.rint(y).astype(int)
        bank.extend(encode(pcm[:frames]))
        tails.extend(encode(pcm[frames:]))
root_dir=Path(__file__).resolve().parents[1]
assert len(tails)<=0x45000
(root_dir/'assets/eno/piano-tail.bin').write_bytes(tails)
fingerprint=2166136261
for value in tails: fingerprint=((fingerprint^value)*16777619)&0xffffffff

def table(f,typ,name,values):
    f.write(f'static const {typ} {name}[] = {{\n')
    for i in range(0,len(values),32):
        f.write(','.join(map(str,values[i:i+32]))+',\n')
    f.write('};\n')

with (Path(__file__).resolve().parents[1]/'firmware/gen/mt_piano.h').open('w') as f:
    f.write(f'#define PIANO_FRAMES {frames}u\n')
    f.write(f'#define TAIL_FRAMES {tail_frames}u\n#define TAIL_BYTES {len(tails)}u\n#define TAIL_HASH {fingerprint}u\n')
    table(f,'uint8_t','piano_bank',bank)
    table(f,'int16_t','ima_step',steps)
    table(f,'int8_t','ima_adjust',adjust)
    table(f,'uint32_t','piano_step',[round(65536*rate/44100*2**((n-min(roots,key=lambda r:abs(n-r)))/12)) for n in range(128)])
