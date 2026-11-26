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
font=ImageFont.truetype('/usr/share/fonts/truetype/open-sans/OpenSans-Semibold.ttf',13*scale)
alpha=[];advance=[]
for i in range(32,127):
    ch=chr(i)
    cell=Image.new('L',(16*scale,18*scale))
    ImageDraw.Draw(cell).text((0,13*scale),ch,font=font,fill=255,anchor='ls')
    alpha.extend((np.asarray(cell.resize((16,18),Image.Resampling.LANCZOS)).astype(int).ravel()+8)//17)
    advance.append(max(3,round(font.getlength(ch)/scale)))
with (root/'firmware/gen/eno_panel.h').open('w') as out:
    for typ,name,data in [('uint16_t','knob_cap',pixels.ravel()),('uint8_t','panel_adv',advance),('uint8_t','panel_alpha',[16*a+b for a,b in zip(alpha[::2],alpha[1::2])])]:
        out.write(f'static const {typ} {name}[]={{\n')
        for i in range(0,len(data),32):out.write(','.join(map(str,data[i:i+32]))+',\n')
        out.write('};\n')
