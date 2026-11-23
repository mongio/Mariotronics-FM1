"""LCD artwork from the supplied A77 diagram, rendered at final pixel size."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
diagram = Image.open(root / "assets/Frippertronics.jpg").convert("RGB")
resample = Image.Resampling.LANCZOS

def deck(index):
    cx = [592,1682][index]
    cy = 324
    origin = [388,1140][index]
    plate = [(478,211,1130,773),(1152,211,1805,773)][index]
    image = Image.new("RGB",(752,652))
    body = diagram.crop(plate)
    image.paste(body,(plate[0]-origin,plate[1]-122))
    g = ImageDraw.Draw(image)
    # Clear the schematic's direction arrow from the metalwork.
    left,right = [(895,1122),(1160,1355)][index]
    g.rectangle((left-origin,442-122,right-origin,479-122),fill=(226,)*3)
    x,y = cx-origin,cy-122
    half=201
    box=(x-half,y-half,x+half+1,y+half+1)
    back=image.crop(box)
    gr=ImageDraw.Draw(back)
    gr.rectangle((0,0,402,402),fill='black')
    gr.rectangle((plate[0]-cx+half,plate[1]-cy+half,plate[2]-cx+half,plate[3]-cy+half),fill=(226,)*3,outline='#666666',width=2)
    gr.ellipse((76,77,326,328),fill='black')
    line=[(532,433),(694,516),(810,516)] if not index else [(1450,514),(1603,514),(1747,431)]
    gr.line([(a-cx+half,b-cy+half) for a,b in line],fill='black',width=4)
    flange=Image.new('RGBA',back.size)
    raw=diagram.load()
    for j in range(403):
        for i in range(403):
            r=math.hypot(i-half,j-half)
            if r>201:continue
            v=raw[cx+i-half,cy+j-half][0]
            if r<81: flange.putpixel((i,j),(v,v,v,255))
            else: flange.putpixel((i,j),(163,163,163,max(0,min(255,(15-abs(v-163))*25))))
    # Mount the two transports face to face, with their reel bays inward.
    rotation = [Image.Transpose.ROTATE_270,Image.Transpose.ROTATE_90][index]
    for step in range(36):
        patch=back.copy()
        wheel=flange.rotate(step*10,resample=Image.Resampling.BICUBIC)
        patch.paste(wheel,mask=wheel.getchannel('A'))
        image.paste(patch,box[:2])
        yield image.transpose(rotation).resize((104,120),resample).convert('L')

def arr(f,name,vals,typ):
    f.write('static const '+typ+' '+name+'[] = {\n')
    for j in range(0,len(vals),24):f.write(','.join(map(str,vals[j:j+24]))+',\n')
    f.write('};\n')

with (root/'firmware/gen/mt_art.h').open('w') as f:
    for i in range(2):
        sprites=[]
        # In cabinet-local coordinates, the right-hand drive is the mirror
        # of the left-hand one. Include the antialiased edge of each flange.
        x,y = [(38,0),(0,0)][i]
        for n,frame in enumerate(deck(i)):
            if n==0:arr(f,'cabinet'+str(i),list(frame.getdata()),'uint8_t')
            sprites.extend(frame.crop((x,y,x+66,y+66)).getdata())
        arr(f,'flange'+str(i),sprites,'uint8_t')
    palette=[]
    for v in range(256):
        rgb=((v>>3)<<11)|((v>>2)<<5)|(v>>3)
        palette.append((rgb>>8)|((rgb&255)<<8))
    arr(f,'grey565',palette,'uint16_t')
    font=ImageFont.truetype('DejaVuSans.ttf',13)
    glyphs=[]; widths=[]
    for c in range(32,127):
        cell=Image.new('L',(15,18))
        ImageDraw.Draw(cell).text((0,0),chr(c),font=font,fill=255)
        widths.append(round(font.getlength(chr(c))))
        glyphs.extend(cell.getdata())
    arr(f,'font_width',widths,'uint8_t')
    arr(f,'font_alpha',glyphs,'uint8_t')
    arr(f,'sine256',[round(127*math.sin(i*math.tau/256)) for i in range(256)],'int8_t')
    arr(f,'period256',[max(512,min(2046*256,round((44100/(440*2**((n-69)/12))-.25)*256))) for n in range(128)],'uint32_t')
