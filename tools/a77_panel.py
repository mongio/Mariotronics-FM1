"""Original front elevation of two A77 recorders in the Fripp/Eno routing."""
from math import cos, sin, radians
from html import escape
def rect(x,y,w,h,fill,r=0):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}"/>'
def disc(x,y,r,fill):
    return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}"/>'
def line(x,y,X,Y,color,width=1):
    return f'<path d="M{x} {y} L{X} {Y}" fill="none" stroke="{color}" stroke-width="{width}"/>'
def label(x,y,s,size=6,col='#e6e4d8',align='middle'):
    return f'<text x="{x}" y="{y}" font-family="DejaVu Sans, sans-serif" font-size="{size}" fill="{col}" text-anchor="{align}">{escape(s)}</text>'
def grad(name,colors):
    stops=''.join(f'<stop offset="{i/(len(colors)-1)}" stop-color="{c}"/>' for i,c in enumerate(colors))
    return f'<linearGradient id="{name}" x2=".85" y2="1" gradientUnits="objectBoundingBox">{stops}</linearGradient>'
def machine(x,record):
    a=[f'<g transform="translate({x} 20)">',rect(0,0,430,309,'url(#walnut)',7),
       rect(9,5,412,299,'#221f1b',4),rect(12,8,406,171,'url(#cast)',3),
       rect(12,179,406,42,'url(#metal)'),rect(12,221,406,80,'#2e3030')]
    for cx in [109,321]:
        active=(cx==109)==record
        for r,c in ((76,'#262726'),(70,'#474846'),(59,'#292926'),(25,'#171916'),(16,'url(#metal)'),(8,'#252723')):
            a.append(disc(cx,92,r,c))
        if active:
            a.append(disc(cx,92,81,'#262622'))
            a.append(disc(cx,92,80,'url(#metal)'))
            a.append(f'<g transform="translate({cx} 92)"><g class="a77-wheel">')
            for angle in [0,120,240]:
                a.append(f'<g transform="rotate({angle})"><path d="M-13 -65 Q-4 -73 16 -66 L26 -47 Q28 -40 17 -34 L0 -27 Q-8 -25 -12 -34 L-19 -51 Q-23 -60 -13 -65Z" fill="#4a3427" stroke="#94958b" stroke-width="2"/></g>')
            for r,c in ((19,'#414340'),(14,'#b7b9ae'),(9,'#282b28'),(4,'#92978d')):
                a.append(disc(0,0,r,c))
            a.append('</g></g>')
    a += [rect(16,179,398,4,'#555a55'),label(57,204,'REVOX',16,'#303633'),label(215,204,'A 77  ·  STEREO TAPE RECORDER',9,'#353935'),
          rect(336,188,48,14,'#343837',2),label(360,199,'0 0 0 0',9,'#f4eee0')]
    for i,t in enumerate(['◀◀','▶▶','▶','■','●']):
        a += [rect(21+24*i,228,22,15,'url(#keymetal)',2),label(32+24*i,239,t,8,'#993b2f' if i==4 else '#252624')]
    for cx in (234,294):
        a+=[rect(cx-27,225,54,25,'#181918',2),rect(cx-24,227,48,20,'#d6bb87',2)]
        for j in range(13):
            t=radians(212+9*j)
            a.append(line(cx+22*cos(t),251+21*sin(t),cx+19*cos(t),251+18*sin(t),'#9b3228' if j>9 else '#534b3e'))
        a += [line(cx,247,cx-10,233,'#413c33'),label(cx+5,242,'VU',5,'#554635')]
    for i,(cx,title) in enumerate([(38,'VOLUME'),(102,'BALANCE'),(241,'LEVEL I'),(304,'LEVEL II'),(382,'POWER')]):
        a += [label(cx,263,title),disc(cx,280,14,'#101312'),disc(cx,279,12,'url(#knobmetal)'),line(cx-5,269,cx-2,275,'#f4efdc',2)]
    for cx,t in [(145,'PHONES'),(169,'MIC I'),(192,'MIC II')]:
        a += [label(cx,268,t,5),disc(cx,281,6,'#959791'),disc(cx,281,4,'#161a19')]
    a += [disc(380,236,4,'#ad4a37' if record else '#474640'),'</g>']
    return ''.join(a)
def artwork():
    a=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 350" width="1000" height="350" role="img" aria-label="Due Revox A77. Nastro dalla bobina sinistra attraverso le testine dei due registratori fino alla bobina destra."><defs>']
    for n,cs in [('walnut',['#614230','#967151','#543b2b']),('cast',['#626760','#434845','#565951']),('metal',['#e1e1d5','#a9ada4','#d9dacf','#90958c']),('keymetal',['#d1d3c7','#7e837e']),('knobmetal',['#9c9e94','#434844','#242724'])]:
        a.append(grad(n,cs))
    a+=['</defs>',rect(0,0,1000,350,'#273431'),machine(18,True),machine(552,False)]
    a += ['<path d="M66 151 Q83 178 123 185 L257 194 L767 194 L869 185 Q910 184 931 151" stroke="#bba287" stroke-width="5" fill="none"/>','<path d="M66 151 Q83 178 123 185 L257 194 L767 194 L869 185 Q910 184 931 151" stroke="#573b2d" stroke-width="3.2" fill="none"/>']
    for x in [18,552]:
        a += [rect(x+139,161,153,31,'url(#cast)',5),rect(x+144,186,143,6,'#292e2a',2)]
        for xx in [x+125,x+303]:
            a += [disc(xx,186,7,'#282a26'),disc(xx,186,4,'url(#metal)')]
    a+=['</svg>']
    return ''.join(a)
