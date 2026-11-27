# Generates firmware/gen/mt_factory.h: the 1.20 factory bank, slots 17-64 (P17-P64).
# Each recipe: name, engine (1 ENO 2 RILEY 3 RADIGUE 4 STOCKHAUSEN, 0 FRIPP), stored voice, overrides.
# Overrides: (PAGE, k, value); ('M', slot, src, dst, amount) for the RADIGUE matrix.
H,S,ART,FUZZ,SCALE,WAH,AMP,PLATE,ENV,COMP,LFO=0,1,2,3,4,5,6,7,8,9,10
ARP,OFF,CHAR,EQ,FILT,HEAD2,TIMB,NFX,MOT,ROUTE,FEEL,LFO2,MODFX,RING,MLFO=13,14,16,17,18,19,23,24,25,27,28,29,30,31,33
LA,LB,WIFI,AGE=0,1,2,3
BEAT,COLOR,SUB,DRIFT,PITCH,LEVEL,FILTER,PLATET,TAPE=1,2,3,4,5,6,7,8,9
RF_TIMBRE,RF_COLOR,RF_LEVEL,RF_PITCH,RF_FILTER,RF_TAPE,RF_PLATE,RF_RATE=range(8)
def T(a,b,c,d):return [(TIMB,0,a),(TIMB,1,b),(TIMB,2,c),(TIMB,3,d)]
def home(delay,fb):return [(H,0,delay),(H,1,fb)]
def plate(mix,dec,damp=52):return [(PLATE,0,mix),(PLATE,1,dec),(PLATE,2,damp)]
def env(a,d,s,r):return [(ENV,0,a),(ENV,1,d),(ENV,2,s),(ENV,3,r)]
def mfx(t,rate,depth,mix):return [(MODFX,0,t),(MODFX,1,rate),(MODFX,2,depth),(MODFX,3,mix)]
def lfo2(rate,shape,target,depth):return [(LFO2,0,rate),(LFO2,1,shape),(LFO2,2,target),(LFO2,3,depth)]
def arp(pat,hz,gate=72,acc=38):return [(ARP,0,pat),(ARP,1,hz),(ARP,2,gate),(ARP,3,acc)]
def mlfo(a,b,spread,tri=0):return [(MLFO,0,a),(MLFO,1,b),(MLFO,2,spread),(MLFO,3,tri)]
def M(slot,src,dst,amt):return [('M',slot,src,dst,amt)]
ENS,CHO,PHA,FLA=1,2,3,4; POST=4
R=[]
# ENO (P17-P32)
R+=[("Felt Piano",1,0,home(60,70)+plate(30,80,60)+[(NFX,1,6),(NFX,2,60)]+env(0,44,85,50)),
("Slow Piano Phase",1,0,home(80,85)+plate(35,85)+arp(3,2,80)+[(HEAD2,0,1),(HEAD2,1,70)]),
("Reverse Piano",1,0,home(70,78)+plate(40,88)+[(NFX,0,1),(NFX,1,9)]),
("Suitcase Rhodes",1,4,T(4,6,0,0)+[(MOT,0,1),(MOT,1,4)]+mfx(CHO,30,40,35)+plate(22,70)+home(50,60)),
("Dyno Bark",1,4,T(6,8,0,0)+mfx(PHA,20,60,40)+home(50,62)+plate(18,64)),
("Rhodes at Night",1,4,T(3,2,0,2)+mfx(CHO+POST,24,50,45)+plate(35,85)+home(70,75)+[(NFX,1,8)]),
("Glass Orbit",1,2,T(4,6,3,4)+plate(40,90)+home(80,85)+arp(4,4,70)),
("Tape Choir",1,3,[(MOT,1,3)]+mfx(ENS+POST,30,60,50)+plate(35,88)+home(76,82)),
("Bowed Glass",1,5,T(4,4,0,5)+plate(45,92)+lfo2(8,0,RF_PLATE,40)+home(80,86)),
("Evening Glass",1,6,mfx(ENS,40,50,50)+plate(40,90)+home(80,88)),
("Dusk Choir",1,7,lfo2(6,6,RF_FILTER,50)+plate(38,90)+home(72,84)),
("Tidal Haze",1,8,mfx(FLA+POST,10,50,40)+plate(50,94)+home(80,90)),
("Abyss",1,9,T(6,6,3,5)+plate(45,94)+home(80,88)+env(60,44,100,80)),
("Nocturne",1,10,T(5,3,4,3)+mfx(ENS+POST,30,60,50)+plate(40,92)+home(78,86)+env(50,44,100,75)),
("Umbra",1,11,T(5,3,3,4)+lfo2(4,6,RF_FILTER,40)+plate(50,95)+home(80,90)+env(55,44,100,85)),
("Deep Field",1,9,T(8,7,6,8)+[(HEAD2,0,1),(HEAD2,1,60),(HEAD2,2,90),(HEAD2,3,1)]+plate(55,96)+home(80,92)+env(70,44,100,90))]
# RILEY (P33-P40)
R+=[("Pulse Organ",2,0,arp(1,8)+T(5,3,4,0)+plate(20,60)+home(40,70)),
("Rainbow Reed",2,1,arp(3,10)+[(FEEL,1,1)]+mfx(CHO,30,40,40)+home(48,74)),
("Time-Lag Organ",2,2,arp(4,12)+T(4,4,0,2)+home(50,82)+plate(26,72)),
("Folded Reed",2,1,arp(2,11,60)+home(60,85)+plate(24,70)),
("Chorale Leslie",2,0,T(4,4,0,3)+plate(30,75)+env(12,44,100,60)+home(64,70)),
("Fast Rotor",2,0,T(6,5,6,7)+arp(1,14,60)+plate(18,58)+home(44,66)),
("Harmonium Drone",2,3,T(5,4,0,0)+plate(40,88)+home(80,85)+mfx(ENS+POST,20,40,30)),
("Dorian Camel",2,2,arp(3,6)+[(SCALE,0,1),(SCALE,1,2)]+lfo2(10,0,RF_RATE,30)+home(56,78))]
# RADIGUE (P41-P52): the matrix does the slow work
R+=[("Slow Beating",3,0,T(2,3,0,2)+M(0,LA,BEAT,4)+mlfo(1,2,2)+plate(40,90)+home(80,90)),
("Veil of Age",3,2,M(0,LB,COLOR,5)+M(1,AGE,SUB,5)+mlfo(3,2,3)+plate(42,92)+home(80,88)),
("Fifth Horizon",3,3,M(0,LA,PITCH,2)+M(1,LB,FILTER,4)+mlfo(0,2,4)+plate(45,92)+home(78,88)),
("Sub Ascent",3,1,M(0,AGE,BEAT,6)+M(1,LA,LEVEL,3)+mlfo(2,3,1)+plate(36,88)+home(80,86)),
("Wandering Beat",3,0,M(0,LA,BEAT,-5)+M(1,LB,DRIFT,6)+M(2,AGE,COLOR,4)+mlfo(1,3,5,1)+plate(40,90)+home(76,88)),
("Veil Pitch",3,2,M(0,LB,PITCH,3)+M(1,LA,PLATET,4)+mlfo(2,1,2)+plate(38,90)+home(80,86)),
("Fifth Growth",3,3,M(0,AGE,SUB,7)+M(1,LA,COLOR,3)+mlfo(2,2,6)+plate(44,92)+home(80,90)),
("Closing Filter",3,0,M(0,LA,FILTER,-5)+M(1,LB,LEVEL,3)+M(2,AGE,DRIFT,5)+mlfo(1,2,3)+plate(40,90)+home(72,88)),
("Tape Breath",3,1,M(0,LA,TAPE,4)+M(1,LB,BEAT,3)+mlfo(3,2,2)+home(70,92)+plate(34,86)),
("Glacial Beat",3,0,T(0,4,0,0)+M(0,LA,BEAT,7)+mlfo(0,1,7)+plate(48,94)+home(80,92)),
("Veil of Sub",3,2,T(4,4,5,0)+M(0,LB,SUB,5)+M(1,LA,COLOR,-4)+mlfo(2,2,4)+plate(40,90)+home(78,88)),
("Fifth Wind",3,3,M(0,LB,DRIFT,7)+M(1,AGE,PLATET,5)+mlfo(1,4,5)+mfx(ENS+POST,20,40,30)+plate(36,90)+home(80,90))]
# STOCKHAUSEN (P53-P60)
R+=[("Shortwave",4,0,T(0,3,4,4)+[(RING,0,5),(RING,1,8),(RING,2,0),(RING,3,2)]+plate(30,80)+home(60,78)),
("Scraped Tam",4,0,T(0,4,6,4)+[(RING,0,3),(RING,1,5),(RING,2,2)]+mfx(PHA+POST,16,50,40)+plate(36,86)),
("Bowed Metal",4,0,T(0,5,3,4)+[(RING,0,2),(RING,1,2),(RING,2,1)]+plate(40,88)+home(70,80)),
("Ring Bells",4,0,T(0,2,5,4)+[(RING,0,7),(RING,1,6)]+plate(45,90)+home(64,82)),
("Field Voices",4,0,T(0,6,2,4)+mfx(ENS+POST,26,50,40)+plate(40,88)+home(80,86)),
("Tele Drift",4,0,T(0,4,4,4)+[(RING,0,6),(RING,1,0),(RING,3,6)]+lfo2(14,0,RF_PITCH,20)+home(56,76)),
("Spiral Scrape",4,0,T(0,3,7,4)+[(RING,0,4),(RING,1,7),(RING,2,2)]+home(60,85)+plate(32,84)),
("Static Hymn",4,0,T(0,7,8,4)+[(RING,0,8),(RING,1,8),(RING,3,8)]+[(FILT,0,1),(FILT,1,60),(FILT,2,40)]+plate(40,90))]
# Studies (P61-P64)
R+=[("Fripp Ensemble",0,0,mfx(ENS+POST,24,50,40)+plate(40,90)+home(80,84)),
("Choir Flanger",1,3,mfx(FLA+POST,8,60,45)+plate(42,90)+home(78,86)),
("Phased Organ",2,0,T(4,4,2,0)+mfx(PHA+POST,12,70,50)+arp(1,6)+home(60,80)),
("Random Abyss",1,9,T(7,5,6,6)+lfo2(6,6,RF_FILTER,60)+mfx(CHO+POST,14,50,40)+plate(50,95)+home(80,90))]
assert len(R)==48,len(R)
names=[r[0] for r in R];assert len(set(names))==48 and all(len(n)<=16 for n in names)
out=["/* Generated by tools/factory.py: the 1.20 factory programs P17-P64 (P01-P16 are FRIPP). */",
"typedef struct { const char *name; unsigned char engine, voice, count; const signed short *ops; } FactoryProgram;",
"/* ops: page, knob, value triples; page 200+n sets RADIGUE matrix slot n: source, target, amount */"]
for i,(n,e,v,ops) in enumerate(R):
    flat=[]
    for o in ops:
        if o[0]=='M':flat+=[200+o[1],0,o[2],200+o[1],1,o[3],200+o[1],2,o[4]]
        else:flat+=list(o)
    out.append(f"static const signed short factory_ops{i}[]={{{','.join(map(str,flat))}}};")
out.append("static const FactoryProgram factory[48]={")
for i,(n,e,v,ops) in enumerate(R):
    cnt=sum(3 if o[0]=='M' else 1 for o in ops)
    out.append(f'  {{"{n}",{e},{v},{cnt},factory_ops{i}}},')
out.append("};")
import sys;open(sys.argv[1],'w').write('\n'.join(out)+'\n')
