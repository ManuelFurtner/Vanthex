"""Original Vanthex armory geometry. No third-party model or texture inputs.

Run with Python 3. Output is a versioned triangle stream consumed by
VanthexBuildWeaponModelsCommandlet; units preserve the existing weapon grip.
Each firearm has independent Body/Magazine/Bolt meshes with shared origins.
"""
from __future__ import annotations
import json, math, struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt' / 'Weapons'
MATS = ['Anodized', 'Steel', 'Silver', 'Polymer', 'Rubber', 'Olive', 'Walnut', 'Brass', 'Optic', 'Marking']
A, S, C, P, R, O, W, B, G, M = range(10)
PALETTE = [(0.105,.125,.15),(.23,.27,.30),(.57,.61,.65),(.075,.086,.095),(.025,.03,.034),(.26,.30,.19),(.29,.115,.045),(.62,.43,.15),(.04,.32,.39),(.71,.74,.68)]

def unit(v):
    n = np.linalg.norm(v)
    return np.asarray(v, float) / n if n > 1e-10 else np.array([0.,0.,1.])

class Mesh:
    def __init__(self): self.triangles = []
    def tri(self, a,b,c,mat,normal=None, smooth=None):
        a,b,c = [np.asarray(v,float) for v in (a,b,c)]
        n = np.cross(b-a,c-a)
        if np.linalg.norm(n) < 1e-9: return
        if normal is not None and np.dot(n,normal)<0:
            b,c=c,b
            if smooth is not None: smooth=[smooth[0],smooth[2],smooth[1]]
            n=-n
        n=unit(n)
        normals=[n]*3 if smooth is None else [unit(v) for v in smooth]
        axis=int(np.argmax(np.abs(n)))
        axes=[i for i in range(3) if i!=axis]
        self.triangles.append((int(mat),[(v.tolist(),nn.tolist(),(v[axes]/8).tolist()) for v,nn in zip((a,b,c),normals)]))
    def quad(self, a,b,c,d,mat,normal=None):
        self.tri(a,b,c,mat,normal); self.tri(a,c,d,mat,normal)
    def save(self,path):
        with path.open('wb') as f:
            f.write(struct.pack('<4sIII',b'VXM1',1,len(self.triangles),len(MATS)))
            for mat,vertices in self.triangles:
                f.write(struct.pack('<I',mat))
                for p,n,uv in vertices: f.write(struct.pack('<8f',*p,*n,*uv))

def area(poly): return sum(p[0]*q[1]-q[0]*p[1] for p,q in zip(poly,poly[1:]+poly[:1]))*.5

def ears(poly):
    ids=list(range(len(poly))); result=[]
    def cross(a,b,c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    while len(ids)>3:
        found=False
        for k,b in enumerate(ids):
            a,c=ids[k-1],ids[(k+1)%len(ids)]
            if cross(poly[a],poly[b],poly[c])<=1e-8: continue
            def inside(j): return min(cross(poly[a],poly[b],poly[j]),cross(poly[b],poly[c],poly[j]),cross(poly[c],poly[a],poly[j]))>=-1e-8
            if any(inside(j) for j in ids if j not in (a,b,c)): continue
            result.append((a,b,c)); ids.pop(k); found=True; break
        if not found: raise ValueError(f'Non-simple outline: {poly}')
    result.append(tuple(ids)); return result

def profile(mesh,poly,depth,mat,cy=0,bevel=.08):
    """Extruded authored side silhouette with bevelled perimeter and real thickness."""
    poly=[tuple(p) for p in poly]
    if area(poly)<0: poly.reverse()
    centre=np.mean(poly,axis=0)
    extent=np.ptp(poly,axis=0)
    inset=[tuple(centre+(np.array(p)-centre)*np.maximum(.6,1-2*bevel/np.maximum(extent,.1))) for p in poly]
    ys=[cy-depth/2,cy-depth/2+min(bevel,depth/3),cy+depth/2-min(bevel,depth/3),cy+depth/2]
    rings=[[(x,y,z) for x,z in pp] for pp,y in zip((inset,poly,poly,inset),ys)]
    for ri in range(3):
        for i,p in enumerate(poly):
            j=(i+1)%len(poly); q=poly[j]
            normal=(q[1]-p[1],0,p[0]-q[0])
            mesh.quad(rings[ri][i],rings[ri][j],rings[ri+1][j],rings[ri+1][i],mat,normal)
    for cap,normal in [(0,(0,-1,0)),(3,(0,1,0))]:
        for i,j,k in ears(inset): mesh.tri(rings[cap][i],rings[cap][j],rings[cap][k],mat,normal)

def box(mesh,center,size,mat,bevel=.12,angle=0):
    x,y,z=center; dx,dy,dz=np.array(size)/2
    b=min(bevel,dx*.5,dz*.5)
    pp=[(-dx+b,-dz),(dx-b,-dz),(dx,-dz+b),(dx,dz-b),(dx-b,dz),(-dx+b,dz),(-dx,dz-b),(-dx,-dz+b)]
    cs,sn=math.cos(math.radians(angle)),math.sin(math.radians(angle))
    profile(mesh,[(x+xx*cs-zz*sn,z+xx*sn+zz*cs) for xx,zz in pp],dy,mat,y,min(b,dy*.18))

def lathe(mesh,sections,mat,centre=(0,0),axis='x',segments=32,caps=True):
    """Turned surface; sections=(axial position,radius), supports open muzzle rims."""
    def point(t,r,theta):
        v=(t,centre[0]+r*math.cos(theta),centre[1]+r*math.sin(theta))
        return v if axis=='x' else (v[1],v[0],v[2])
    rings=[[point(t,r,i*2*math.pi/segments) for i in range(segments)] for t,r in sections]
    for k in range(len(rings)-1):
        t0,r0=sections[k]; t1,r1=sections[k+1]
        for i in range(segments):
            j=(i+1)%segments
            theta=(i+.5)*2*math.pi/segments
            n=np.array([r0-r1,(t1-t0)*math.cos(theta),(t1-t0)*math.sin(theta)])
            if axis!='x': n=n[[1,0,2]]
            # Smooth longitudinal cylindrical faces, retain hard lathe shoulders.
            smooth=[]
            for index in (i,j,j,i):
                th=index*2*math.pi/segments
                nn=np.array([r0-r1,(t1-t0)*math.cos(th),(t1-t0)*math.sin(th)])
                smooth.append(nn if axis=='x' else nn[[1,0,2]])
            a,b,c,d=rings[k][i],rings[k][j],rings[k+1][j],rings[k+1][i]
            mesh.tri(a,b,c,mat,n,smooth[:3]); mesh.tri(a,c,d,mat,n,[smooth[0],smooth[2],smooth[3]])
    if caps:
        for k,direction in [(0,-1),(-1,1)]:
            t,r=sections[k]
            n=(direction,0,0) if axis=='x' else (0,direction,0)
            for i in range(segments): mesh.tri(point(t,0,0),rings[k][i],rings[k][(i+1)%segments],mat,n)

def barrel(mesh,start,end,r=.48,z=1.5,y=0,mat=S):
    lathe(mesh,[(start,r*1.2),(start+.25,r*1.2),(start+.3,r),(end-.4,r),(end-.1,r*1.07),(end,r*.95),(end,r*.48),(end-.6,r*.48)],mat,(y,z),caps=False)
    lathe(mesh,[(end-.61,r*.48),(end-.63,0)],R,(y,z),caps=False)

def tube_path(mesh,points,r,mat,sides=10):
    points=np.asarray(points,float); rings=[]
    for i,p in enumerate(points):
        tangent=unit(points[min(i+1,len(points)-1)]-points[max(0,i-1)])
        side=unit(np.cross(tangent,[0,1,0])); up=unit(np.cross(side,tangent))
        rings.append([p+r*(math.cos(a)*side+math.sin(a)*up) for a in np.arange(sides)*2*math.pi/sides])
    for k in range(len(rings)-1):
        for i in range(sides):
            j=(i+1)%sides
            n=(rings[k][i]+rings[k][j])/2-points[k]
            mesh.quad(rings[k][i],rings[k][j],rings[k+1][j],rings[k+1][i],mat,n)
    for k,sign in [(0,-1),(-1,1)]:
        n=unit(points[1]-points[0])*sign if k==0 else unit(points[-1]-points[-2])
        for i in range(sides): mesh.tri(points[k],rings[k][i],rings[k][(i+1)%sides],mat,n)

def screw(mesh,x,z,y=1.7,r=.2):
    sign=1 if y>0 else -1
    lo,hi=sorted([y,y+sign*.1])
    lathe(mesh,[(lo,r*.8),(lo+.02,r),(hi-.02,r),(hi,r*.8)],S,(x,z),'y',16)
    lathe(mesh,[(y+sign*.105,r*.4),(y+sign*.11,r*.4)],R,(x,z),'y',6)

def rail(mesh,start,end,z=4,y=0,width=1.6):
    box(mesh,((start+end)/2,y,z),(end-start,width*.72,.35),A,.08)
    for i in range(max(1,int((end-start)/.75))):
        box(mesh,(start+.3+i*.75,y,z+.32),(.4,width,.36),S,.07)

def trigger(mesh,x=2,z=-2,large=False):
    w=4.4 if large else 3.5; h=2.5 if large else 2
    pts=[(x-1,0,z),(x-.8,0,z-h+.3),(x,0,z-h),(x+w-1,0,z-h),(x+w,0,z-h+.6),(x+w,0,z+.3)]
    tube_path(mesh,pts,.2,A,8)
    tube_path(mesh,[(x+.8,0,z+.6),(x+.9,0,z-.7),(x+1.6,0,z-1.3)],.15,S,8)

def grip(mesh,mat=P,short=False):
    bottom=-7.2 if short else -8.5
    profile(mesh,[(-3,-1.1),(.4,-1.1),(.9,-2.5),(-.3,bottom),(-3.8,bottom-.3),(-4.4,bottom+.5),(-3.3,-3)],2.1,mat,bevel=.18)
    for side in (-1,1):
        profile(mesh,[(-2.9,-3),(-.2,-3.2),(-1.1,bottom+.7),(-3.5,bottom+.6)],.12,R,side*1.08,.06)
        for i in range(5): box(mesh,(-2-i*.14,side*1.17,-3.6-i*.7),(2.1,.11,.15),mat,.035,-13)
        screw(mesh,-2,-3.3,side*1.25,.15)

def stock(mesh,rear=-20,style='tactical',mat=P):
    if style=='tactical':
        lathe(mesh,[(rear+3,.65),(-5,.65)],S,(0,.3))
        profile(mesh,[(rear,-4.6),(rear+1,-4.9),(rear+8,-1.7),(rear+8,1.1),(rear+3,1.5),(rear,1.1)],2.8,mat,bevel=.2)
        profile(mesh,[(rear+.8,-2.8),(rear+6,-.9),(rear+6,.3),(rear+1,.3)],3.0,R,bevel=.12)
        box(mesh,(rear+.1,0,-1.8),(.6,3.1,6.5),R,.22,-5)
        for i in range(6): box(mesh,(rear-.22,0,-4.1+i*.85),(.16,3,.16),P,.025)
        box(mesh,(rear+5,0,-2.1),(3.2,1.2,.8),S,.12)
    else:
        profile(mesh,[(rear,-5.2),(rear+1,-5.5),(-5,-1),(-3,1),(-6,2),(rear+2,.8),(rear,.3)],3.1,mat,bevel=.25)
        box(mesh,(rear+.1,0,-2.3),(.5,3.3,6.6),R,.2,-3)
        for side in (-1,1): screw(mesh,rear+2.6,-2.4,side*1.65,.22)

def magazine(mesh,top=(5,-1.5),length=10,width=3.3,curve=0,mat=A):
    x,z=top
    pp=[(x-width/2,z),(x+width/2,z),(x+width/2+curve*.25,z-length*.48),(x+width/2+curve,z-length),(x-width/2+curve,z-length-.2),(x-width/2+curve*.25,z-length*.48)]
    profile(mesh,pp,1.8,mat,bevel=.14)
    box(mesh,(x+curve,0,z-length),(width+.35,2.1,.45),R,.1,-curve*2)
    for side in (-1,1):
        for j in (-.8,.0,.8):
            tube_path(mesh,[(x+j,side*.96,z-.9),(x+j+curve*.22,side*.98,z-length*.48),(x+j+curve*.8,side*.96,z-length+.9)],.09,S,6)
        screw(mesh,x,z-1.2,side*1,.12)

def optic(mesh,x,z,kind='scope'):
    if kind=='scope':
        lathe(mesh,[(x-7,1.2),(x-5,1.2),(x-4,.8),(x+2,.8),(x+4,1.45),(x+7,1.45),(x+7,1.15),(x+6.6,1.15)],A,(0,z),caps=False)
        lathe(mesh,[(x+6.55,1.12),(x+6.57,1.12)],G,(0,z),caps=True)
        lathe(mesh,[(x-6.8,.95),(x-6.77,.95)],G,(0,z))
        for xx in (x-2.8,x+1.8):
            box(mesh,(xx,0,z-1.5),(1.1,2.1,2),A,.15)
            lathe(mesh,[(xx-.42,.87),(xx+.42,.87)],S,(0,z))
        lathe(mesh,[(-1.1,.5),(1.1,.5)],S,(x,z+.1),'y',20)
        for xx in (x-5.5,x+4.8): lathe(mesh,[(xx-.25,1.5),(xx+.25,1.5)],R,(0,z))
    else:
        box(mesh,(x,0,z-1),(3,2.2,.7),A,.1)
        # Open reflex sight frame with a physical tinted lens recessed in its hood.
        tube_path(mesh,[(x,-1,z-.6),(x,-1.25,z+1),(x,-.8,z+1.6),(x,.8,z+1.6),(x,1.25,z+1),(x,1,z-.6)],.18,A)
        box(mesh,(x+.05,0,z+.45),(.10,1.8,1.9),G,.03)
        screw(mesh,x,z-1,-1.2,.18)

def handguard(mesh,start,end,z=1.5,style='rail',mat=P):
    if style=='round':
        lathe(mesh,[(start,1.6),(start+.6,1.9),(end-.5,1.65),(end,1.35)],mat,(0,z),segments=16)
        for i in range(int((end-start)/1.0)):
            lathe(mesh,[(start+.5+i,.0+1.8),(start+.65+i,1.8)],R,(0,z),segments=16)
    else:
        # Open vent windows are gaps between rails/ribs, not painted black squares.
        for side in (-1,1):
            box(mesh,((start+end)/2,side*1.32,z-1),(end-start,.45,.65),mat,.12)
            box(mesh,((start+end)/2,side*1.32,z+1),(end-start,.45,.65),mat,.12)
            for i in range(int((end-start)/1.7)+1):
                box(mesh,(start+i*1.7,side*1.32,z),(.6,.48,2),mat,.12,-12)
        rail(mesh,start,end,z+1.7,width=2.7)
        for xx in (start+.6,end-.6):
            for side in (-1,1): screw(mesh,xx,z,side*1.65,.17)

def details(mesh,xmin,xmax,z=0,y=1.5):
    for side in (-1,1):
        for xx,zz in [(xmin+.7,z),(xmax-.8,z),(xmin+2,z-1)]: screw(mesh,xx,zz,side*y)
        box(mesh,((xmin+xmax)/2,side*(y+.03),z+.5),((xmax-xmin)*.45,.07,.15),S,.025)
        # Tiny engraved identification dashes, not a borrowed manufacturer's logo.
        for i in range(5): box(mesh,(xmin+1.2+i*.28,side*(y+.06),z-.6),(.12,.035,.25),M,.015)

def make_pistol(name):
    body,mag,bolt=Mesh(),Mesh(),Mesh(); premium=name=='P250'
    grip(body,short=True); trigger(body,x=.2,z=-1.3)
    profile(body,[(-4,-1.2),(7,-1.2),(8.8,.1),(8.4,1),(-3.8,1)],2.2,P,bevel=.16)
    barrel(body,-1,9.5,.38,1.8,mat=C)
    profile(bolt,[(-4.4,.6),(8.7,.6),(9,1),(8.7,3.1),(8,3.4),(-4.1,3.4),(-4.5,2.7)],2.25,C if premium else A,bevel=.14)
    for side in (-1,1):
        for x in (-3.7,-3.2,-2.7,-2.2,-1.7,6.2,6.7,7.2): box(bolt,(x,side*1.16,2),(.15,.16,1.5),R,.025,-12)
    details(body,-3,5,0,1.15)
    if premium:
        for side in (-1,1): profile(bolt,[(-.8,1.1),(5.8,1.1),(6,1.8),(-.6,1.9)],.06,S,side*1.16,.04)
        box(body,(5.5,0,-1.35),(3.8,1.6,.45),A,.09)
    # Ejection port/chamber and three-dot iron sights.
    box(bolt,(2.4,0,3.42),(2.4,1.4,.08),R,.1)
    box(bolt,(2.3,0,3.48),(1.8,1.05,.07),C,.08)
    box(bolt,(-3.5,0,3.65),(.6,1.5,.45),A,.08)
    box(bolt,(7.8,0,3.65),(.45,.35,.45),A,.05)
    for yy in (-.5,.5): lathe(bolt,[(-3.84,.1),(-3.86,.1)],M,(yy,3.72),caps=True,segments=10)
    box(bolt,(7.8,0,3.9),(.14,.2,.06),M,.015)
    magazine(mag,(-1.7,-2),5.5,2.8,-1.0,S)
    return [body,mag,bolt],(9.55,0,1.8)

def make_rifle(name):
    body,mag,bolt=Mesh(),Mesh(),Mesh()
    if name in ('M4','DMR'):
        dmr=name=='DMR'; coating=O if dmr else A
        stock(body,-21 if dmr else -19,'fixed' if dmr else 'tactical',P)
        grip(body); trigger(body,1,-1.8)
        profile(body,[(-5,-1.5),(3,-1.5),(4,-3.6),(8,-3.6),(9,-.8),(10,1),(9,3.5),(-4,3.5),(-5,2.2)],3,coating,bevel=.16)
        handguard(body,9,24 if dmr else 21,1.5,mat=coating)
        barrel(body,9,38 if dmr else 30,.47,1.5)
        rail(body,-4,9,4,width=2.2)
        details(body,-4,9,1,1.58)
        magazine(mag,(5,-2.5),7 if dmr else 10,3.3,1.1,A)
        box(bolt,(2,-1.68,2),(4.4,.24,1.1),C,.14)
        box(body,(2,-1.54,2),(5.3,.2,1.55),R,.12)
        box(bolt,(-4,0,3.55),(1.2,3.7,.5),S,.12)
        optic(body,3,7.4,'scope' if dmr else 'reflex')
        if dmr:
            for side in (-1,1):
                tube_path(body,[(20,side*.7,0),(19,side*2,-6),(22,side*2.5,-9)],.24,S)
                box(body,(22,side*2.5,-9.2),(2,1,.5),R,.15)
        else:
            # Protective front sight wings and hollow muzzle brake rings.
            for side in (-1,1): tube_path(body,[(25,side*.6,1.5),(25,side*.6,5)],.18,A)
            box(body,(25,0,4.3),(.4,1.3,.35),S,.05)
        end=38 if dmr else 30
        lathe(body,[(end-1.8,.7),(end-.2,.7)],A,(0,1.5),caps=False)
        return [body,mag,bolt],(end+.05,0,1.5)
    if name=='AK':
        stock(body,-21,'fixed',W); grip(body,W); trigger(body,1,-1.8)
        profile(body,[(-6,-1.5),(9,-1.5),(9,2),(-5,2.5),(-6,1.5)],2.7,S,bevel=.1)
        lathe(body,[(-5,1.5),(-4,1.55),(8,1.45)],A,(0,2),segments=16)
        handguard(body,9,18,1.5,'round',W)
        barrel(body,17,31,.43,1.7)
        barrel(body,9,24,.35,3.3)
        magazine(mag,(6,-1.8),13,3.3,5.2,A)
        details(body,-5,8,.1,1.45)
        box(bolt,(3,-1.65,2),(4.8,.4,.9),C,.12)
        lathe(bolt,[(-2.4,.22),(-1.5,.22)],S,(4,2),'y',16)
        box(body,(5,1.5,.3),(4,.2,.3),A,.07,-12)
        for side in (-1,1): tube_path(body,[(27,side*.55,1.5),(27,side*.55,5.4)],.17,S)
        tube_path(body,[(27,-.55,5.4),(27,0,5.8),(27,.55,5.4)],.17,S)
        rail(body,-3,1,4.1,width=1.6)
        return [body,mag,bolt],(31.05,0,1.7)
    if name=='AUG':
        # Distinct bullpup receiver and rear magazine; open thumb/trigger space.
        profile(body,[(-19,-4.8),(-17,-5.2),(-8,-3.6),(-5,-1.5),(5,-1.1),(7,.6),(9,2.5),(7.5,4),(-17,4),(-19,2.7)],3.2,O,bevel=.25)
        box(body,(-19,0,-.3),(.65,3.5,8.7),R,.22)
        grip(body,O); trigger(body,.8,-1.4,True)
        tube_path(body,[(.4,0,-7),(5.4,0,-6.2),(6.5,0,-3),(6.4,0,0)],.38,O,12)
        barrel(body,7,26,.5,2)
        lathe(body,[(8,1),(10,1),(10.4,.65)],S,(0,2),segments=20)
        profile(body,[(8,-.1),(10,-.1),(9.7,-6.5),(8.1,-6.7)],1.6,P,bevel=.15)
        for i in range(5): box(body,(9,0,-1.2-i*.9),(1.6,1.7,.2),R,.06)
        magazine(mag,(-11,-3),8.3,3.4,1.2,P)
        box(bolt,(-9,-1.7,2.2),(4.2,.2,1.3),S,.12)
        box(body,(-9,-1.62,2.2),(5,.15,1.7),R,.14)
        details(body,-17,5,.8,1.7)
        box(body,(-3,0,4.3),(11,1.5,.9),O,.15)
        optic(body,-3,7.5,'scope')
        return [body,mag,bolt],(26.05,0,2)
    raise ValueError(name)

def make_smg(name):
    body,mag,bolt=Mesh(),Mesh(),Mesh(); mp7=name=='MP7'
    if mp7:
        grip(body,short=True); trigger(body,.6,-1.1)
        profile(body,[(-4.5,-1.4),(7,-1.4),(9,.5),(9,3.3),(7.5,4.1),(-4,4.1),(-5,3)],2.6,P,bevel=.22)
        # Twin telescoping stock rails and a sculpted shoulder pad.
        for y in (-1.25,1.25): tube_path(body,[(-13,y,-.2),(-4,y,-.2),(1,y,.2)],.18,S)
        profile(body,[(-13,-3.6),(-12,-3.8),(-11.7,2.5),(-12.5,2.7),(-13,2)],2,R,bevel=.12)
        barrel(body,7,12.7,.4,1.6)
        magazine(mag,(-1.9,-3.1),6.5,2.8,-1,S)
        profile(body,[(5,-1),(7,-1),(6.6,-6.4),(5.4,-6.5)],1.4,P,bevel=.15)
        for i in range(4): box(body,(6,0,-2.6-i*.8),(1.35,1.5,.15),R,.05)
        for side in (-1,1):
            for i in range(4): box(body,(4.2+i*.85,side*1.36,1.3),(.42,.13,1.5),R,.08)
        rail(body,-4,8,4.5,width=2)
        optic(body,1,6.2,'reflex')
        details(body,-4,4,.1,1.4)
        box(bolt,(.5,-1.45,2),(3.8,.2,1.2),S,.1)
        return [body,mag,bolt],(12.75,0,1.6)
    stock(body,-17,'fixed',P); grip(body); trigger(body,.5,-1.5)
    profile(body,[(-5,-1.8),(5,-1.8),(6,-2.8),(8,-2.8),(8,.3),(-4,1)],2.4,P,bevel=.18)
    lathe(body,[(-5,1.3),(-4.5,1.45),(12,1.45),(13,1.1)],S,(0,1.6),segments=24)
    handguard(body,10,19,1.5,'round',P)
    barrel(body,18,23,.42,1.6)
    magazine(mag,(6,-2.2),10,2.5,1.4,A)
    box(bolt,(1.5,-1.45,2),(4.2,.22,1.1),C,.12)
    barrel(body,-3,15,.25,3.35,mat=S)
    tube_path(body,[(13,-.2,3.35),(13,-2,3.35)],.22,A)
    details(body,-4,7,-.3,1.3)
    for xx in (-3,20):
        box(body,(xx,0,3.4),(1.3,1.2,1.6),A,.1)
        lathe(body,[(xx-.2,.68),(xx+.2,.68),(xx+.2,.48),(xx-.2,.48)],S,(0,4.5),caps=False,segments=24)
    return [body,mag,bolt],(23.05,0,1.6)

def make_shotgun():
    body,mag,bolt=Mesh(),Mesh(),Mesh()
    stock(body,-22,'fixed',W)
    profile(body,[(-5,-2.4),(7,-2.4),(8,-.5),(8,2.8),(6,3.6),(-4,3.6),(-5,2.6)],3,S,bevel=.22)
    # Swept wrist under receiver, traditional stock rather than rifle pistol grip.
    profile(body,[(-8,-1),(-3,-1),(1,-2),(-1,-5),(-5,-4.2)],2.8,W,bevel=.2)
    trigger(body,-.2,-2.2)
    barrel(body,6,35,.6,2)
    lathe(body,[(5,.75),(28,.75),(28.4,.65)],S,(0,.2))
    handguard(bolt,12,21,-.2,'round',W)
    box(bolt,(3,-1.58,1.7),(4.2,.15,1.5),C,.12)
    box(body,(3,-1.53,1.7),(4.7,.1,2),R,.13)
    details(body,-4,7,.3,1.6)
    # Reloadable shell carrier, animated into the loading port (tube-fed weapon).
    lathe(mag,[(-1,.54),(2.4,.54),(2.55,.6)],P,(0,-3.1),segments=24)
    lathe(mag,[(2.4,.59),(2.9,.59)],B,(0,-3.1),segments=24)
    box(body,(33,0,2.75),(.3,.3,.4),B,.05)
    return [body,mag,bolt],(35.05,0,2)

def make_knife():
    body=Mesh()
    # Full tang recurved drop point, ridge and separately faceted sharpened edge.
    outline=[(1,-1.1),(13,-1.55),(19,-.5),(22,1),(17,2.5),(3,2.1),(1,1.4)]
    profile(body,outline,.25,S,bevel=.015)
    for side in (-1,1):
        # A bevel from the central ridge to the cutting edge gives a real bright facet.
        ridge=[(2,side*.3,.8),(12,side*.31,.6),(18,side*.2,.65),(22,0,1)]
        edge=[(1,0,-1.1),(13,0,-1.55),(19,0,-.5),(22,0,1)]
        for i in range(3): body.quad(ridge[i],edge[i],edge[i+1],ridge[i+1],C,(0,side,0))
        profile(body,[(3,1),(14,1.25),(17,1.6),(3,1.5)],.025,A,side*.145,.01)
    profile(body,[(-10,-1.15),(-9,-1.8),(-3,-1.4),(.5,-1.6),(1.3,-.7),(1.3,1),(-1,1.3),(-8,1.4),(-10,.8)],1.7,P,bevel=.2)
    profile(body,[(.2,-2.6),(1.4,-2.6),(2,2.7),(.8,2.7)],2.3,A,bevel=.14)
    for side in (-1,1):
        for xx in (-8,-4,0): screw(body,xx,0,side*.9,.3)
        for i in range(6): box(body,(-7.2+i*1.2,side*.94,-.05),(.35,.12,1.7),R,.07,-15)
    for i in range(5): box(body,(3+i*.7,0,2.18),(.22,.35,.23),A,.035)
    return [body],(22,0,1)

def render_preview(models, destination):
    """Independent orthographic source-geometry contact sheet; UE render remains acceptance."""
    from PIL import Image,ImageDraw,ImageFont
    width,height=1800,2100
    im=Image.new('RGB',(width,height),(14,20,28)); draw=ImageDraw.Draw(im)
    try: font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',24)
    except OSError: font=ImageFont.load_default()
    for index,(name,parts,muzzle) in enumerate(models):
        col,row=index%2,index//2; ox,oy=col*900,row*410+52
        draw.rounded_rectangle((ox+16,oy,ox+884,oy+390),radius=16,fill=(23,31,42),outline=(48,65,82))
        draw.text((ox+40,oy+18),name,fill=(220,231,237),font=font)
        # Three-quarter view, slight elevation; +X to the right and near -Y visible.
        yaw=.27; pitch=.30
        ux=np.array([math.cos(yaw),math.sin(yaw),0]); uy=np.array([-math.sin(yaw)*math.sin(pitch),math.cos(yaw)*math.sin(pitch),math.cos(pitch)])
        view=unit(np.cross(ux,uy))
        allv=np.array([p for mesh in parts for _,tri in mesh.triangles for p,n,uv in tri])
        xs=allv@ux; ys=allv@uy; scale=min(790/max(np.ptp(xs),1),255/max(np.ptp(ys),1))
        cx,cy=(xs.min()+xs.max())/2,(ys.min()+ys.max())/2
        faces=[]; light=unit((-1,-2,4))
        for mesh in parts:
            for mat,tri in mesh.triangles:
                v=np.array([t[0] for t in tri]); n=np.mean([t[1] for t in tri],axis=0)
                if np.dot(n,view)<-.01: continue
                shade=.3+.7*max(0,float(np.dot(unit(n),light)))
                spec=max(0,float(np.dot(unit(n),unit(light+view))))**28 *(.35 if mat in (A,S,C,B,G) else .07)
                color=tuple(int(255*min(1,max(0,c*shade+spec))**.70) for c in PALETTE[mat])
                xy=[(ox+450+(float(p@ux)-cx)*scale,oy+220-(float(p@uy)-cy)*scale) for p in v]
                faces.append((float(v.mean(axis=0)@view),xy,color))
        for depth,xy,color in sorted(faces): draw.polygon(xy,fill=color)
    im.save(destination)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    models=[]; manifest={'format':'VXM1','materials':MATS,'weapons':[],'license':'Original procedural Vanthex geometry; no third-party mesh inputs.'}
    for name in ['Pistol','P250','MP5','MP7','M4','AK','AUG','DMR','Shotgun','Knife']:
        if name in ('Pistol','P250'): parts,muzzle=make_pistol(name)
        elif name in ('MP5','MP7'): parts,muzzle=make_smg(name)
        elif name=='Shotgun': parts,muzzle=make_shotgun()
        elif name=='Knife': parts,muzzle=make_knife()
        else: parts,muzzle=make_rifle(name)
        entries=[]
        for suffix,mesh in zip(('Body','Magazine','Bolt'),parts):
            filename=f'{name}_{suffix}.vxm'; mesh.save(OUT/filename)
            entries.append({'part':suffix,'file':filename,'triangles':len(mesh.triangles)})
        vertices=np.array([p for mesh in parts for _,tri in mesh.triangles for p,n,uv in tri])
        manifest['weapons'].append({'id':name,'muzzle':muzzle,'bounds_min':vertices.min(axis=0).tolist(),'bounds_max':vertices.max(axis=0).tolist(),'parts':entries})
        models.append((name,parts,muzzle))
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
    preview=ROOT/'Saved'/'WeaponModels-source-preview.png'; preview.parent.mkdir(exist_ok=True)
    render_preview(models,preview)
    print(json.dumps({'weapons':len(models),'meshes':sum(len(m[1]) for m in models),'triangles':sum(len(p.triangles) for _,parts,_ in models for p in parts),'preview':str(preview)}))

if __name__=='__main__': main()
