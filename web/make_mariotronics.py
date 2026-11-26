"""Package source, native firmware and the WASM instrument in one offline HTML."""
import base64
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root=Path(__file__).resolve().parent.parent
def embed(path): return base64.b64encode(path.read_bytes()).decode()
def contents(patterns):
    for pat in patterns.split():
        yield from sorted(root.glob(pat))
files=contents('ENO-LAB.md assets/eno/* tools/eno_panel.py firmware/gen/eno_panel.h LICENSE APACHE-2.0.txt AIR-LAB.md LICENSING.md MARIOTRONICS.md build.sh firmware/app.ld firmware/crt0.S assets/studio_faces.png tools/studio_faces.py firmware/gen/mt_faces.h firmware/gen/mt_global.h firmware/gen/mt_vu.h firmware/gen/mt_tone.h firmware/gen/mt_piano.h firmware/gen/mt_art.h firmware/gen/mt_intro.h firmware/gen/ble_rf_tables.h firmware/src/ble/* firmware/gen/felucca_font.h firmware/hal/* firmware/loader/* firmware/src/*.c firmware/src/*.h tests/host.c tests/eno-bank.cjs tests/key-hold.cjs tests/air-lab.cjs tests/panel-audio.cjs tests/panel-leds.cjs tests/presets.cjs tests/instrument-edit.cjs tests/live-engines.cjs tests/envelope-reels.cjs tests/tape-speed.cjs tests/tape-motion.cjs tests/fripp-phrasing.cjs tests/service-display.cjs tools/build.py tools/fm1pkg_make.py tools/lz4blk.py tools/get_toolchain.sh tools/tone_tables.py tools/piano.py tools/global_art.py tools/vu_face.py tools/mariotronics_art.py tools/a77_panel.py assets/fripp.png assets/eno.png assets/Frippertronics.jpg assets/FM1_GRANA_rescue.zip web/mt_service.js assets/fonts/*LICENSE* web/fm1ota.js web/web_host.c web/mariotronics-worklet.js web/mariotronics.template.html web/make_mariotronics.py')
src=root/'build/Mariotronics_ENO_Studio_Lab_4_sorgenti.zip'
with ZipFile(src,'w',compression=ZIP_DEFLATED) as z:
    for f in files:
        if f.is_file() and not f.name.startswith(('virgo','vgfx')):z.write(f,f.relative_to(root))
tpl=(root/'web/mariotronics.template.html').read_text()
for key,body in {'__ENO_BANK__':embed(root/'assets/eno/piano-tail.bin'),'__KIT__':embed(root/'assets/FM1_GRANA_rescue.zip'),'__SERVICE__':(root/'web/mt_service.js').read_text(),'__OTA__':(root/'web/fm1ota.js').read_text().replace('export class','class').replace('export function','function'),'__WORKLET__':(root/'web/mariotronics-worklet.js').read_text(),'__FW__':embed(root/'build/mariotronics-1.18-eno4.fwsc'),'__WASM__':embed(root/'web/mariotronics.wasm'),'__SOURCE__':embed(src)}.items():
    assert tpl.count(key)==1;tpl=tpl.replace(key,body)
(root/'build/Mariotronics_ENO_Studio_Lab_4.html').write_text(tpl)
