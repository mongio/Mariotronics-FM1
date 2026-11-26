"""Bake the panel at 8x, then quantize to the FM-1 display's RGB565 and 4-bit coverage."""
from pathlib import Path
from PIL import Image,ImageFont,ImageDraw
import numpy as np

root=Path(__file__).resolve().parents[1]
scale=8
size=37
v=(np.arange(size*scale)+.5)/scale-size/2
x,y=np.meshgrid(v,v)
r=np.hypot(x,y)
angle=np.arctan2(y,x)
light=np.cos(angle+2.28)
rgb=np.zeros((*r.shape,3))+[5,5,4]
def disc(radius,base,level):
    global rgb
    rgb=np.where((r<radius)[:,:,None],np.clip(np.array(base)+level[:,:,None],0,255),rgb)
disc(16.3,[39,37,32],light*14)
disc(15.5,[104,99,87],light*46)
disc(14.8,[34,33,30],light*12)
disc(13.9,[56,54,48],light*17+np.cos(30*angle)*13)
disc(11.6,[160,155,141],light*52)
disc(10.8,[103,100,91],(-x-y)*2.3+5*np.sin(r*12)+11*np.cos(2*angle-1.5))
# Fine engraved ticks around the fixed bezel.
for a in np.linspace(-np.pi*.75,np.pi*.75,9):
    along=x*np.sin(a)-y*np.cos(a)
    cross=x*np.cos(a)+y*np.sin(a)
    rgb[(along>16.5)&(along<18.2)&(np.abs(cross)<.5)]=[137,123,98]
im=Image.fromarray(rgb.astype('uint8')).resize((size,size),Image.Resampling.LANCZOS)
a=np.array(im).astype(int)
pixels=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
# Pack each glyph at its own advance. Both faces then fit in the app region.
tables=[('uint16_t','knob_cap',pixels.ravel())]
alpha=[];widths=[];offsets=[]
for path,height,baseline in [
    ('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',13,14),
    ('/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',16,14)]:
    font=ImageFont.truetype(path,height*scale)
    for i in range(32,127):
        ch=chr(i)
        width=max(3,round(font.getlength(ch)/scale))
        cell=Image.new('L',(width*scale,18*scale))
        ImageDraw.Draw(cell).text((0,baseline*scale),ch,font=font,fill=255,anchor='ls')
        raw=(np.asarray(cell.resize((width,18),Image.Resampling.LANCZOS)).astype(int).ravel()+8)//17
        offsets.append(len(alpha));widths.append(width)
        alpha.extend(16*raw[::2]+raw[1::2])
tables.extend([('uint8_t','panel_adv',widths),('uint16_t','panel_off',offsets),('uint8_t','panel_alpha',alpha)])
with (root/'firmware/gen/eno_panel.h').open('w') as out:
    for typ,name,data in tables:
        out.write(f'static const {typ} {name}[]={{\n')
        for i in range(0,len(data),32):out.write(','.join(map(str,data[i:i+32]))+',\n')
        out.write('};\n')
