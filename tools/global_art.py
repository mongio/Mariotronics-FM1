"""Generate palette/RLE artwork for the FM-1 LCD."""
from PIL import Image, ImageDraw
from pathlib import Path
import math
S=4; W=100; H=100
palette=['101111','1e201f','303331','40413c','575951','72766e','93978d','b9bbad','dddecb','f3ecd3','593625','94623d','bf844a','f5c474','ffe7a0','fff4ce']
out=['static const uint16_t panel_colors[]={'+','.join('RGB('+','.join(str(int(c[a:a+2],16)) for a in [0,2,4])+')' for c in palette)+'};']
for name in ['dial','din','bulb']:
 im=Image.new('RGB',(W*S,H*S),'#'+palette[0]);d=ImageDraw.Draw(im)
 def shape(coords):return tuple(round(n*S) for n in coords)
 def circ(coords,c):d.ellipse(shape(coords),fill='#'+palette[c])
 def line(pts,c,w):d.line([shape(p) for p in pts],fill='#'+palette[c],width=round(w*S),joint='curve')
 def poly(pts,c):d.polygon([shape(p) for p in pts],fill='#'+palette[c])
 if name=='dial':
  for r,c in [(46,1),(44,4),(43,2),(39,1),(38,6),(36,3),(32,2),(30,5),(29,3),(27,4)]:
   circ((50-r,50-r,50+r,50+r),c)
  for j in range(60):
   a=j*math.pi/30
   line([(50+r*math.sin(a),50+r*math.cos(a)) for r in (33,37)],5 if j<30 else 3,1.2)
  circ((24,24,76,76),3);circ((25,25,74,74),4)
 elif name=='din':
  for r,c in [(44,2),(42,6),(40,8),(38,3),(36,7),(34,2),(32,1)]:
   circ((50-r,50-r,50+r,50+r),c)
  for deg in (-90,-45,0,45,90):
   a=math.radians(deg);x=50+22*math.sin(a);y=50-22*math.cos(a)
   circ((x-5,y-5,x+5,y+5),5);circ((x-3.7,y-3.7,x+3.7,y+3.7),0)
  poly([(44,78),(56,78),(56,84),(44,84)],6)
 else:
  pts=[(42,75),(40,65),(29,53),(24,43),(23,33),(26,22),(35,13),(45,10),(55,10),(66,14),(74,23),(77,34),(76,44),(71,54),(61,65),(58,75)]
  poly(pts,2);line(pts,6,1.5);line([(28,34),(30,25),(36,19),(40,17)],8,2)
  line([(44,72),(41,43)],5,1.5);line([(56,72),(59,43)],5,1.5)
  pts=[(41+i*1.5,43+(i%2)*4) for i in range(13)]
  line(pts,11,3);line(pts,15,1.2)
  for j in range(5):line([(41,75+3*j),(59,73+3*j)],6-j%2,3)
  line([(45,90),(55,90)],6,3)
 im=im.resize((W,H),Image.Resampling.LANCZOS)
 pal=Image.new('P',(1,1)); rgb=[int(c[a:a+2],16) for c in palette for a in [0,2,4]];pal.putpalette(rgb+rgb[:3]*(256-16))
 data=list(im.quantize(palette=pal,dither=Image.Dither.NONE).getdata())
 packed=[];i=0
 while i<len(data):
  j=i+1
  while j<len(data) and data[j]==data[i] and j-i<16:j+=1
  packed.append((j-i-1)*16+(data[i]%16));i=j
 out.append('static const uint8_t '+name+'_art[]={\n'+',\n'.join(','.join(map(str,packed[i:i+48])) for i in range(0,len(packed),48))+'};')
(Path(__file__).resolve().parents[1]/'firmware/gen/mt_global.h').write_text('\n'.join(out)+'\n')
