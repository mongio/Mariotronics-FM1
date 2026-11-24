"""Frequency and gain tables for 44.1 kHz EQ and filter."""
import math
from pathlib import Path
cutoffs=[round(40*400**(i/100)) for i in range(101)]
mids=[250,350,500,700,1000,1400,2000,2800,4000]
with (Path(__file__).resolve().parents[1]/'firmware/gen/mt_tone.h').open('w') as out:
 def table(name,vals,t='float'):
    out.write('static const '+t+' '+name+'[]={\n')
    out.write(','.join(format(x,'.10g') for x in vals)+'};\n')
 table('cutoff_hz',cutoffs,'uint16_t')
 table('mid_hz',mids,'uint16_t')
 table('eq_gain',[10**((i-12)/40) for i in range(25)])
 table('eq_sqrt_gain',[10**((i-12)/80) for i in range(25)])
 for name,fs in [('cut',cutoffs),('band',[180,4200]+mids),('plot',[40*400**(i/40) for i in range(41)])]:
    for fn in (math.sin,math.cos):
        table(name+'_'+fn.__name__,[fn(2*math.pi*f/44100) for f in fs])
 table('plot_power',[10**(i/10) for i in range(-24,25)])
