"""Pack the startup artwork (mt_intro.h) with PackBits: 37,920 bytes become about 13 KB.
   Byte n < 128: n+1 literal pixels follow. Byte n >= 128: the next pixel repeats n-125 times."""
from pathlib import Path
import re
path = Path(__file__).resolve().parents[1] / 'firmware/gen/mt_intro.h'
s = path.read_text()
start = s.index('static const unsigned char intro_art[]')
end = s.index('};', start) + 2
pixels = [int(v) for v in re.findall(r'\d+', s[s.index('{', start) + 1:end - 2])]
assert len(pixels) == 240 * 158
def pack(a):
    out, i, n = [], 0, len(a)
    while i < n:
        j = i
        while j < n and a[j] == a[i] and j - i < 130: j += 1
        if j - i >= 3:
            out += [125 + j - i, a[i]]; i = j; continue
        k = i
        while k < n and k - i < 128 and not (k + 2 < n and a[k] == a[k + 1] == a[k + 2]): k += 1
        out += [k - i - 1] + a[i:k]; i = k
    return out
art, title = pack(pixels[:240 * 136]), pack(pixels[240 * 136:])
def table(name, data):
    rows = [','.join(map(str, data[i:i + 40])) for i in range(0, len(data), 40)]
    return f'static const unsigned char {name}[] = {{\n' + ',\n'.join(rows) + '\n};'
path.write_text(s[:start] + table('intro_art', art) + '\n' + table('intro_title', title) + s[end:])
print(len(art), len(title))
