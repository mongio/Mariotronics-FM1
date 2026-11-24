"""Package source, native firmware and the WASM instrument in one offline HTML."""
import base64
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root=Path(__file__).resolve().parent.parent
def embed(path): return base64.b64encode(path.read_bytes()).decode()
def contents(patterns):
    for pat in patterns.split():
        yield from sorted(root.glob(pat))
files=contents('LICENSE LICENSING.md MARIOTRONICS.md build.sh firmware/app.ld firmware/crt0.S firmware/gen/mt_art.h firmware/gen/mt_intro.h firmware/gen/ble_rf_tables.h firmware/src/ble/* firmware/gen/felucca_font.h firmware/hal/* firmware/loader/* firmware/src/*.c firmware/src/*.h tests/host.c tests/panel-audio.cjs tests/panel-leds.cjs tests/presets.cjs tools/build.py tools/fm1pkg_make.py tools/lz4blk.py tools/get_toolchain.sh tools/mariotronics_art.py tools/a77_panel.py assets/Frippertronics.jpg assets/fonts/*LICENSE* web/fm1ota.js web/web_host.c web/mariotronics-worklet.js web/mariotronics.template.html web/make_mariotronics.py')
src=root/'build/Mariotronics_0_6_sorgenti.zip'
with ZipFile(src,'w',compression=ZIP_DEFLATED) as z:
    for f in files:
        if f.is_file() and not f.name.startswith(('virgo','vgfx')):z.write(f,f.relative_to(root))
tpl=(root/'web/mariotronics.template.html').read_text()
for key,body in {'__OTA__':(root/'web/fm1ota.js').read_text().replace('export class','class').replace('export function','function'),'__WORKLET__':(root/'web/mariotronics-worklet.js').read_text(),'__FW__':embed(root/'build/mariotronics-0.6.fwsc'),'__WASM__':embed(root/'web/mariotronics.wasm'),'__SOURCE__':embed(src)}.items():
    assert tpl.count(key)==1;tpl=tpl.replace(key,body)
(root/'build/Mariotronics_0_6.html').write_text(tpl)
