"""Downsample the approved HOME panels and preserve their RGB565 colours."""
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]

def array(out, name, values, typ, attr=''):
    out.write(f'static const {typ} {name}[]{attr} = {{\n')
    for j in range(0, len(values), 24):
        out.write(','.join(map(str, values[j:j+24])) + ',\n')
    out.write('};\n')

with (root/'firmware/gen/mt_art.h').open('w') as out:
    for skin in ('fripp', 'eno'):
        art = Image.open(root / 'assets' / (skin + '.png')).convert('RGB')
        # Seven reference pixels per LCD pixel. Reel hubs land at (69,31)
        # and (31,31); each cropped deck includes its full circular flange.
        for i, left in enumerate((90, 805)):
            frame = art.crop((left, 20, left+707, 825)).resize((101,115), Image.Resampling.LANCZOS)
            pixels = []
            for r,g,b in frame.getdata():
                c = ((r>>3)<<11) | ((g>>2)<<5) | (b>>3)
                pixels.append((c>>8) | ((c&255)<<8))
            array(out, skin+str(i), pixels, 'uint16_t')
    font = ImageFont.truetype('DejaVuSans.ttf',13)
    glyphs, widths = [], []
    for c in range(32,127):
        cell = Image.new('L',(15,18))
        ImageDraw.Draw(cell).text((0,0),chr(c),font=font,fill=255)
        widths.append(round(font.getlength(chr(c))))
        glyphs.extend(cell.getdata())
    array(out,'font_width',widths,'uint8_t')
    array(out,'font_alpha',[(glyphs[i]//17)*16+glyphs[i+1]//17 for i in range(0,len(glyphs),2)],'uint8_t')
    array(out,'sine256',[round(127*math.sin(i*math.tau/256)) for i in range(256)],'int8_t',' AUDIO_TABLE')
    array(out,'period256',[max(512,min(2046*256,round((44100/(440*2**((n-69)/12))-.25)*256))) for n in range(128)],'uint32_t',' AUDIO_TABLE')
