"""Render a compact struck-string piano bank and its playback tables."""
from pathlib import Path
import numpy as np

rate = 14700
duration = 2.45
roots = range(36, 93, 8)
time = np.arange(round(rate * duration)) / rate
samples = []
for note in roots:
    frequency = 440 * 2 ** ((note - 69) / 12)
    signal = np.zeros_like(time)
    # Coupled unison strings, stiff-string partials and hammer contact point.
    for harmonic in range(1, 33):
        partial = frequency * harmonic * np.sqrt(1 + 0.00012 * harmonic**2)
        if partial >= rate * 0.46:
            break
        weight = np.sin(np.pi * harmonic / 8.5) / harmonic**1.65
        decay = 0.63 + frequency/1700 + harmonic**1.35 * 0.21
        envelope = 0.73*np.exp(-time*decay*2.6) + 0.27*np.exp(-time*decay)
        for detune, gain in ((0, 0.55), (-0.0008, 0.26), (0.0012, 0.19)):
            signal += gain * weight * envelope * np.sin(2*np.pi*partial*(1+detune)*time)
    hammer = np.random.default_rng(note).normal(size=len(time))
    signal += 0.019*np.convolve(hammer,[0.2,0.4,0.3,0.1],mode='same')*np.exp(-time*150)
    signal *= np.minimum(time/0.0018,1)*np.minimum((duration-time)/0.09,1)
    signal /= np.max(np.abs(signal))
    samples.extend(np.rint(127*np.sign(signal)*np.log1p(63*np.abs(signal))/np.log(64)).astype(int))
def table(f,typ,name,values):
    f.write(f'static const {typ} {name}[] = {{\n')
    for i in range(0,len(values),32):
        f.write(','.join(map(str,values[i:i+32]))+',\n')
    f.write('};\n')
with (Path(__file__).resolve().parents[1]/'firmware/gen/mt_piano.h').open('w') as f:
    f.write(f'#define PIANO_FRAMES {len(time)}u\n')
    table(f,'int8_t','piano_bank',samples)
    table(f,'int16_t','piano_decode',[round(np.sign(i)*14500*np.expm1(abs(i)*np.log(64)/127)/63) for i in range(-127,128)])
    table(f,'uint32_t','piano_step',[round(65536/3*2**((n-min(roots,key=lambda r:(abs(n-r),-r)))/12)) for n in range(128)])
