#!/usr/bin/env python3
"""Generate a conceptual ReqFlow AI promo motion graphic (H.264 MP4 + GIF preview).
Requires Pillow, NumPy, and PyAV. Uses original vector-like UI illustrations, not app screenshots.
"""
from PIL import Image, ImageDraw, ImageFont
import av, math, os
from pathlib import Path
import numpy as np

W,H=1280,720
FPS=20
SCENE_SEC=3.0
SCENES=6
TOTAL=SCENE_SEC*SCENES
OUT=Path(__file__).parent
MP4=OUT/'reqflow-ai-super-motion.mp4'
GIF=OUT/'reqflow-ai-super-motion-preview.gif'
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
MONO='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
if not Path(FONT).exists(): FONT='/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf'; BOLD='/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf'; MONO=FONT
C={'bg':'#202427','bg2':'#252a2d','panel':'#2c3236','panel2':'#333a3e','line':'#465056','text':'#f4f1ec','muted':'#b0b5b6','terra':'#c07c62','terra2':'#8b523f','green':'#a3bda5','yellow':'#d4b878','blue':'#93afc0','white':'#ffffff','black':'#171a1c'}

def font(size,bold=False,mono=False): return ImageFont.truetype(MONO if mono else BOLD if bold else FONT,size)
def ease(x):
 x=max(0,min(1,x));return x*x*(3-2*x)
def mix(a,b,t):
 def rgb(c):return tuple(int(c.strip('#')[i:i+2],16) for i in (0,2,4))
 aa,bb=rgb(a),rgb(b);return tuple(round(x+(y-x)*t) for x,y in zip(aa,bb))
# Precompute the smooth background once with vectorized NumPy (the prior per-pixel
# Python loop made each frame prohibitively slow).
def _make_base():
    yy, xx = np.mgrid[0:H, 0:W]
    radius = np.sqrt(((xx-980)/760.0)**2 + ((yy-160)/570.0)**2)
    v = np.clip(1.0-radius, 0.0, 1.0) * .65
    c0=np.array((32,36,39), dtype=np.float32)
    c1=np.array((37,42,45), dtype=np.float32)
    arr=np.clip(c0[None,None,:]*(1-v[:,:,None])+c1[None,None,:]*v[:,:,None],0,255).astype(np.uint8)
    image=Image.fromarray(arr, 'RGB')
    d=ImageDraw.Draw(image, 'RGBA')
    for x in range(0,W,48): d.line((x,0,x,H), fill=(255,255,255,7), width=1)
    for y in range(0,H,48): d.line((0,y,W,y), fill=(255,255,255,6), width=1)
    return image
_BASE = _make_base()
def bg(t):
    im=_BASE.copy(); d=ImageDraw.Draw(im,'RGBA')
    cx=1050+int(15*math.sin(t*.55)); cy=160+int(12*math.cos(t*.4))
    d.ellipse((cx-210,cy-210,cx+210,cy+210),outline=(192,124,98,17),width=2)
    d.ellipse((cx-150,cy-150,cx+150,cy+150),outline=(192,124,98,12),width=1)
    return im

def rr(d,box,fill,outline=None,r=12,width=1):d.rounded_rectangle(box,radius=r,fill=fill,outline=outline,width=width)
def text(d,xy,s,size=24,color=None,bold=False,mono=False,anchor=None):d.text(xy,s,font=font(size,bold,mono),fill=color or C['text'],anchor=anchor)
def line(d,xy,fill=None,width=2):d.line(xy,fill=fill or C['line'],width=width)
def pill(d,x,y,label,color,fill=None,size=15):
 f=font(size,True);bb=d.textbbox((0,0),label,font=f);w=bb[2]+24;h=32
 rr(d,(x,y,x+w,y+h),fill or '#303b37',r=16)
 d.text((x+12,y+7),label,font=f,fill=color)
 return w

def header(d,kicker,title,sub=None):
 text(d,(82,91),kicker.upper(),17,C['terra'],True)
 text(d,(82,129),title,43,C['text'],True)
 if sub:text(d,(84,186),sub,20,C['muted'])

def footer(d):
 line(d,(82,672,1198,672), '#42494d',1)
 text(d,(84,686),'REQFLOW AI  /  CONCEPTO VISUAL · PROYECTO EN DESARROLLO',13,'#929a9d',True)
 text(d,(1195,686),'C++20  ·  Qt 6  ·  RUST PARCIAL',13,'#929a9d',True,anchor='ra')

def window(d,x,y,w,h,title='ReqFlow AI'):
 rr(d,(x,y,x+w,y+h),C['panel'],outline=C['line'],r=15,width=2)
 d.rounded_rectangle((x,y,x+w,y+44),radius=14,fill='#353b3f')
 d.rectangle((x,y+28,x+w,y+44),fill='#353b3f')
 for i,col in enumerate([C['terra'],C['yellow'],C['green']]):d.ellipse((x+17+i*19,y+15,x+27+i*19,y+25),fill=col)
 text(d,(x+82,y+11),title,16,C['muted'],True)

def scene0(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA')
 # logo icon scales in
 scale=.78+.22*ease(p/.65);x=88;y=208;size=int(74*scale)
 rr(d,(x,y,x+size,y+size),C['terra'],r=18)
 text(d,(x+size//2,y+size//2),'RF',27,C['white'],True,anchor='mm')
 text(d,(88,315),'ReqFlow AI',62,C['text'],True)
 text(d,(90,405),'Trabaja con APIs.',44,C['text'],True)
 text(d,(90,462),'Con más claridad.',44,C['text'],True)
 # decorative request route
 points=[(790,438),(920,438),(920,319),(1060,319),(1060,226),(1170,226)]
 d.line(points,fill=(192,124,98,150),width=4,joint='curve')
 for i,(x,y) in enumerate(points[::2]):
  r=8+int(3*math.sin(t*3+i));d.ellipse((x-r,y-r,x+r,y+r),fill=C['terra'])
 rr(d,(835,222,1094,278),C['panel2'],outline=C['line'],r=13)
 text(d,(859,240),'GET',17,C['green'],True,mono=True);text(d,(922,240),'/api/v1/data',17,C['text'],False,mono=True)
 pill(d,90,554,'APLICACIÓN DE ESCRITORIO',C['green'],fill='#303b37')
 footer(d);return im

def scene1(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA');header(d,'01 / SOLICITUDES HTTP','Prepara cada petición','Método, URL, parámetros, cabeceras y cuerpo en un solo espacio.')
 # interface mockup slides in from right
 x=int(245+160*(1-ease(p/.65)));y=247;w=790;h=356
 window(d,x,y,w,h,'Request workspace  ·  proyecto local')
 # sidebar
 rr(d,(x+15,y+58,x+190,y+h-18),'#262b2e',r=8)
 text(d,(x+31,y+75),'PROYECTO',13,C['muted'],True)
 for i,label in enumerate(['Requests','Environments','History','Test suites']):
  yy=y+112+i*43
  if i==0:rr(d,(x+24,yy-5,x+180,yy+29),'#3b4245',r=6)
  text(d,(x+39,yy),label,16,C['text'] if i==0 else C['muted'],i==0)
 # request panel
 xx=x+216
 text(d,(xx,y+72),'GET',16,C['green'],True,mono=True)
 rr(d,(xx+75,y+60,x+w-26,y+99),'#23282a',outline=C['line'],r=6)
 text(d,(xx+91,y+70),'https://api.example.com/v1/users',15,C['text'],False,mono=True)
 pill(d,x+w+x*0 if False else x+0,y+0,'',C['text']) if False else None
 rr(d,(x+w-115,y+60,x+w-30,y+99),C['terra2'],r=6)
 text(d,(x+w-72,y+72),'Send',15,C['white'],True,anchor='mm')
 for i,label in enumerate(['Params','Headers','Body','Auth']):
  tx=xx+i*92;text(d,(tx,y+127),label,14,C['terra'] if i==0 else C['muted'],i==0)
 line(d,(xx,y+152,x+w-28,y+152),C['line'],1)
 text(d,(xx,y+172),'Query parameters',15,C['muted'],True)
 for i,(a,b) in enumerate([('page','1'),('limit','20')]):
  yy=y+207+i*39
  rr(d,(xx,yy,x+490,yy+31),'#252a2d',outline=C['line'],r=5)
  text(d,(xx+10,yy+6),a,14,C['text'],False,mono=True)
  text(d,(xx+235,yy+6),b,14,C['green'],False,mono=True)
 pill(d,82,587,'MÉTODOS HTTP',C['blue'],fill='#30373b')
 pill(d,232,587,'REQUESTS LOCALES',C['green'],fill='#303b37')
 footer(d);return im

def scene2(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA');header(d,'02 / PROYECTOS Y RESPUESTAS','Mantén el contexto a mano','Proyectos, peticiones guardadas, historial y suites en almacenamiento local.')
 x=int(235+130*(1-ease(p/.7)));y=250
 window(d,x,y,810,345,'Workspace  ·  local SQLite')
 # project list
 rr(d,(x+18,y+60,x+265,y+326),'#252a2d',r=8)
 text(d,(x+34,y+78),'HISTORIAL RECIENTE',13,C['muted'],True)
 rows=[('GET','/users',C['green'],'200 OK'),('POST','/sessions',C['terra'],'201 CREATED'),('GET','/metrics',C['green'],'200 OK')]
 for i,(method,path,col,status) in enumerate(rows):
  yy=y+112+i*60
  if i==0:rr(d,(x+27,yy-7,x+255,yy+47),'#373e42',r=6)
  text(d,(x+39,yy),method,13,col,True,mono=True);text(d,(x+91,yy),path,14,C['text'],False,mono=True);text(d,(x+39,yy+24),status,12,C['muted'],False,mono=True)
 # response json pane
 xx=x+293
 text(d,(xx,y+73),'RESPONSE  /  JSON',13,C['muted'],True)
 pill(d,x+672,y+63,'200 OK',C['green'],fill='#303b37')
 rr(d,(xx,y+105,x+770,y+312),'#202528',outline=C['line'],r=8)
 code=[('{','white'),('  "data": {','blue'),('    "id": 1842,','terra'),('    "name": "ReqFlow",','green'),('    "active": true','yellow'),('  },','blue'),('  "items": [ ... ]','muted'),('}','white')]
 for i,(s,k) in enumerate(code):text(d,(xx+20,y+119+i*22),s,15, C.get(k,C['muted']),False,mono=True)
 pill(d,82,587,'HISTORIAL Y RESPUESTAS',C['blue'],fill='#30373b')
 pill(d,332,587,'SQLITE LOCAL',C['green'],fill='#303b37')
 footer(d);return im

def scene3(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA');header(d,'03 / EDITOR VISUAL','Conecta pasos en un flujo API','El editor y el motor de flujos están en desarrollo; algunas capacidades siguen pendientes de validar.')
 # horizontal flow nodes with smoothly animated connectors
 cards=[(120,340,200,130,'START','Inicio',C['blue']),(380,340,220,130,'HTTP REQUEST','Consultar API',C['terra']),(670,340,210,130,'JSON EXTRACT','Leer campo',C['green']),(960,340,200,130,'CONDITION','¿Correcto?',C['yellow'])]
 for i,(x,y,w,h,k,title,col) in enumerate(cards):
  if i < len(cards)-1:
   x1=x+w;y1=y+64;nx=cards[i+1][0];ny=cards[i+1][1]+64
   progress=max(0,min(1,(p*1.4-i*.16)))
   d.line((x1,y1,x1+(nx-x1)*ease(progress),y1),fill=C['line'],width=4)
   if progress>.8:d.polygon([(nx-8,ny-7),(nx,ny),(nx-8,ny+7)],fill=C['terra'])
 for i,(x,y,w,h,k,title,col) in enumerate(cards):
  rr(d,(x,y,x+w,y+h),C['panel'],outline=col,r=13,width=2)
  d.ellipse((x+18,y+18,x+30,y+30),fill=col)
  text(d,(x+44,y+15),k,12,col,True)
  text(d,(x+18,y+55),title,18,C['text'],True)
  if k=='HTTP REQUEST':text(d,(x+18,y+91),'GET  /users/{id}',14,C['muted'],False,mono=True)
  elif k=='JSON EXTRACT':text(d,(x+18,y+91),'$.data.id',14,C['muted'],False,mono=True)
  elif k=='CONDITION':text(d,(x+18,y+91),'status == 200',13,C['muted'],False,mono=True)
 # branch labels
 pill(d,873,472,'TRUE',C['green'],fill='#303b37',size=13);pill(d,962,472,'FALSE',C['terra'],fill='#44332e',size=13)
 rr(d,(165,525,1118,574),'#2d3336',outline='#41494d',r=8)
 text(d,(189,540),'NODOS · CONEXIONES · CONDICIONES',14,C['text'],True)
 text(d,(852,540),'CICLOS Y LÍMITES: VALIDACIÓN PENDIENTE',13,C['yellow'],True)
 footer(d);return im

def scene4(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA');header(d,'04 / IMPORTACIÓN Y ANÁLISIS','Del esquema a una lectura más clara','Importación OpenAPI parcial · comparación JSON en beta experimental.')
 # left openapi card
 rr(d,(82,250,611,574),C['panel'],outline=C['line'],r=14,width=2)
 text(d,(112,274),'OPENAPI  /  YAML',15,C['blue'],True)
 rr(d,(106,312,588,540),'#1e2325',outline=C['line'],r=8)
 yaml=[('openapi:','3.0.3',C['blue']),('paths:','',C['terra']),('  /users:','',C['green']),('    get:','',C['yellow']),('      summary:','List users',C['muted']),('      parameters:','...',C['muted'])]
 for i,(a,b,col) in enumerate(yaml):
  text(d,(127,331+i*30),a,15,col,True if i<4 else False,mono=True)
  if b:text(d,(360,331+i*30),b,15,C['text'],False,mono=True)
 pill(d,112,542,'JSON + YAML · SUBCONJUNTO',C['blue'],fill='#30373b',size=13)
 # center arrow
 d.line((632,404,680,404),fill=C['terra'],width=3);d.polygon([(680,396),(692,404),(680,412)],fill=C['terra'])
 # right compare panel
 rr(d,(707,250,1198,574),C['panel'],outline=C['line'],r=14,width=2)
 text(d,(737,274),'JSON DIFF  /  BETA',15,C['terra'],True)
 rr(d,(731,312,1174,540),'#1e2325',outline=C['line'],r=8)
 diffs=[('$.user.name','Modified',C['yellow']),('$.items[2]','Added',C['green']),('$.status','Removed',C['terra'])]
 for i,(path,label,col) in enumerate(diffs):
  yy=340+i*58;rr(d,(751,yy,1153,yy+43),'#292f32',r=6);text(d,(770,yy+10),path,15,C['text'],False,mono=True);pill(d,1015,yy+6,label,col,fill='#343b3e',size=12)
 text(d,(752,509),'Por índice · hasta 2000 cambios / 48 niveles',12,C['muted'])
 footer(d);return im

def scene5(p,t):
 im=bg(t);d=ImageDraw.Draw(im,'RGBA')
 # final end card, significant subtle pulse
 rr(d,(92,120,1188,601),C['panel'],outline='#4a5357',r=20,width=2)
 rr(d,(146,173,214,241),C['terra'],r=16);text(d,(180,207),'RF',25,C['white'],True,anchor='mm')
 text(d,(242,179),'REQFLOW AI',17,C['terra'],True)
 text(d,(145,288),'Tu espacio local',50,C['text'],True)
 text(d,(145,351),'para trabajar con APIs.',50,C['text'],True)
 text(d,(148,433),'Explora el código. Sigue la evolución.',21,C['muted'])
 rr(d,(148,490,485,545),C['terra2'],r=8)
 text(d,(316,507),'CÓDIGO FUENTE EN GITHUB',15,C['white'],True,anchor='mm')
 pill(d,148,562,'SIN BINARIO LINUX VERIFICADO',C['yellow'],fill='#423c2e',size=13)
 # abstract graph motif
 cx,cy=927,355
 for i,(dx,dy,lab,col) in enumerate([(-145,-100,'HTTP',C['blue']),(0,-6,'JSON',C['green']),(140,-115,'FLOW',C['terra']),(123,105,'LOCAL',C['yellow']),(-100,139,'API',C['blue'])]):
  xx,yy=cx+dx,cy+dy
  d.line((cx,cy,xx,yy),fill='#596267',width=3)
  rr(d,(xx-58,yy-24,xx+58,yy+24),'#242a2d',outline=col,r=10,width=2);text(d,(xx,yy),lab,14,col,True,anchor='mm')
 d.ellipse((cx-36,cy-36,cx+36,cy+36),fill=C['terra2'],outline=C['terra'],width=3);text(d,(cx,cy),'RF',20,C['white'],True,anchor='mm')
 footer(d);return im

RENDER=[scene0,scene1,scene2,scene3,scene4,scene5]

def frame_at(i):
 t=i/FPS; idx=min(SCENES-1,int(t/SCENE_SEC));local=(t-idx*SCENE_SEC)/SCENE_SEC
 curr=RENDER[idx](ease(local),t)
 fade=.32
 if idx>0 and local<fade/SCENE_SEC:
  prev=RENDER[idx-1](1.0,t)
  curr=Image.blend(prev,curr,ease(local/(fade/SCENE_SEC)))
 return curr

def main():
 n=int(TOTAL*FPS)
 container=av.open(str(MP4),'w')
 stream=container.add_stream('libx264',rate=FPS);stream.width=W;stream.height=H;stream.pix_fmt='yuv420p';stream.options={'preset':'veryfast','crf':'21','movflags':'faststart'}
 preview=[]
 for i in range(n):
  im=frame_at(i)
  vf=av.VideoFrame.from_image(im)
  for packet in stream.encode(vf):container.mux(packet)
  if i%4==0:
   preview.append(im.resize((480,270),Image.Resampling.LANCZOS).convert('P',palette=Image.Palette.ADAPTIVE,colors=96))
  if i%60==0:print(f'{i}/{n}',flush=True)
 for packet in stream.encode():container.mux(packet)
 container.close()
 # GIF preview at 10fps, moderate palette and file size
 preview[0].save(GIF,save_all=True,append_images=preview[1:],duration=200,loop=0,optimize=True,disposal=2)
 print('MP4',MP4,MP4.stat().st_size,'GIF',GIF,GIF.stat().st_size)
if __name__=='__main__':main()
