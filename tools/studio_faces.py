"""Quantize the approved instrument illustrations for the 240-pixel LCD."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
atlas = Image.open(root / "assets/studio_faces.png")
with (root / "firmware/gen/mt_faces.h").open("w") as out:
    for index in range(6):
        tile = atlas.crop((0, 104*index, 240, 104*(index+1))).quantize(32)
        colors = tile.getpalette()[:96]
        rgb565 = [(r>>3)<<11 | (g>>2)<<5 | b>>3 for r,g,b in zip(colors[::3],colors[1::3],colors[2::3])]
        print(f"static const uint16_t face_pal_{index}[]={{" + ",".join(map(str,rgb565)) + "};",file=out)
        pixels = list(tile.getdata())
        print(f"static const uint8_t face_{index}[]={{",file=out)
        # Eight palette indices fit into five bytes. Pad the trailing read.
        packed=[]
        for start in range(0,len(pixels),8):
            group=sum(v<<(5*i) for i,v in enumerate(pixels[start:start+8]))
            packed.extend(group.to_bytes(5,"little"))
        packed.append(0)
        for start in range(0,len(packed),80):
            print(",".join(map(str,packed[start:start+80]))+",",file=out)
        print("};",file=out)
    for typ,name,prefix in [('uint8_t','faces','face'),('uint16_t','face_palettes','face_pal')]:
        print(f"static const {typ} *const {name}[]={{"+",".join(f"{prefix}_{i}" for i in range(6))+"};",file=out)
