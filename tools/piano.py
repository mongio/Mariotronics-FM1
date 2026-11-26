"""Build two velocity layers from a struck, coupled stiff-string model."""
from pathlib import Path
import numpy as np

rate = 16000
frames = 28544
roots = [38, 47, 56, 65, 74, 83]
t = np.arange(frames) / rate
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

for root in roots:
    f = 440 * 2**((root-69)/12)
    for hard in (0,1):
        y = np.zeros(frames)
        contact = (0.0031 if not hard else 0.0014) * (220/f)**0.25
        for k in range(1,49):
            freq = f*k*np.sqrt(1+(0.000065+0.00004*(f/440)**1.5)*k*k)
            if freq >= rate*0.46:
                break
            strike = np.sin(np.pi*k/8.1)
            stiffness = (1+(freq*contact*0.42)**2)**-0.62
            initial = strike*stiffness/k**0.88
            decay = 0.58+0.00065*f+0.085*k**1.36
            for cents, weight, loss in ((0,0.47,1.65),(-1.4,0.27,0.83),(1.9,0.26,1.03)):
                y += weight*initial*np.exp(-t*decay*loss)*np.sin(2*np.pi*freq*2**(cents/1200)*t)
        hammer=np.random.default_rng(root).standard_normal(frames)
        y += (0.015+0.045*hard)*np.convolve(hammer,[0.22,0.43,0.35],'same')*np.exp(-t/contact)
        # Broad soundboard modes excited by the hammer, common to all keys.
        for hz,gain in ((109,0.085),(203,0.065),(336,0.04),(591,0.025)):
            y += gain*np.sin(2*np.pi*hz*t)*np.exp(-t*(16+hz/25))
        y *= (1-np.exp(-t/(0.0005+0.0004*(1-hard))))*np.clip((t[-1]-t)/0.18,0,1)
        y /= max(abs(y))
        bank.extend(encode(np.rint(15000*y).astype(int)))

def table(f,typ,name,values):
    f.write(f'static const {typ} {name}[] = {{\n')
    for i in range(0,len(values),32):
        f.write(','.join(map(str,values[i:i+32]))+',\n')
    f.write('};\n')

with (Path(__file__).resolve().parents[1]/'firmware/gen/mt_piano.h').open('w') as f:
    f.write(f'#define PIANO_FRAMES {frames}u\n')
    table(f,'uint8_t','piano_bank',bank)
    table(f,'int16_t','ima_step',steps)
    table(f,'int8_t','ima_adjust',adjust)
    table(f,'uint32_t','piano_step',[round(65536*rate/44100*2**((n-min(roots,key=lambda r:abs(n-r)))/12)) for n in range(128)])
