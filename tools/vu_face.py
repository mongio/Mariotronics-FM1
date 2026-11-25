"""Rasterize the twin meter faces at three times LCD resolution."""
from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
import math

root=Path(__file__).resolve().parents[1]
s=4
im=Image.new('RGB',(120*s,105*s),'#141719')
d=ImageDraw.Draw(im)
def box(coords,fill,r=0):
    d.rounded_rectangle(tuple(round(v*s) for v in coords),radius=r*s,fill=fill)
def line(a,b,color,width=1):
    d.line(tuple(round(v*s) for v in (*a,*b)),fill=color,width=round(width*s))
def word(x,y,text,size,color):
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',round(size*s))
    d.text((x*s,y*s),text,font=font,fill=color,anchor='mm')
def polar(angle,length):
    r=math.radians(angle)
    return (60+0.90*length*math.sin(r),108-length*math.cos(r))
box((0,1,119,103),'#525452',4);box((2,3,117,101),'#141414',3)
box((5,7,114,97),'#c6bba0',2)
for y in range(10,96):
    v=1-abs(y-46)/270
    box((7,y,112,y+1),tuple(int(i*v) for i in (249,237,198)))
for angle in range(-44,43):
    line(polar(angle,69),polar(angle+1,69),'#9e402f' if angle>=18 else '#322e27',1)
for a,label in [(-43,'−20'),(-32,'10'),(-17,'5'),(-4,'3'),(11,'1'),(19,'0'),(27,'1'),(35,'2'),(42,'3')]:
    red=a>=19
    line(polar(a,65),polar(a,71),'#a53f2e' if red else '#302c25',1)
    x,y=polar(a,78)
    word(x,y,label,6.4,'#923927' if red else '#292823')
word(60,58,'VU',11,'#493f31')
word(60,73,'REVOX',6.6,'#595549')
box((7,85,112,97),'#988c6e',1);box((8,87,111,96),'#393a35',1)
im=im.resize((120,105),Image.Resampling.LANCZOS).quantize(128)
palette=im.getpalette();indices=list(im.getdata())
with (root/'firmware/gen/mt_vu.h').open('w') as out:
    out.write('static const uint16_t vu_palette[]={'+','.join('RGB(%d,%d,%d)'%tuple(palette[i:i+3]) for i in range(0,384,3))+'};\nstatic const uint8_t vu_face[]={\n')
    packed=indices
    for i in range(0,len(packed),48): out.write(','.join(map(str,packed[i:i+48]))+',\n')
    out.write('};\n')
