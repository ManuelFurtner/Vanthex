"""Original reference-directed industrial detailing; executed in the authoring context.
Geometry replaces visual modules only. Their placement and gameplay proxies remain owned
by the unchanged GRID V4 layout. Nothing is traced or extracted from reference pictures.
"""
def paint(name,color,rough=.65,metal=.05,glow=0):
    PALETTE[name]=(color,rough,metal,glow)
    if name not in MATS:
        material=bpy.data.materials.new(name);material.use_nodes=True;MATS[name]=material
    material=MATS[name];material.diffuse_color=(*color,1)
    s=material.node_tree.nodes.get('Principled BSDF')
    s.inputs['Base Color'].default_value=(*color,1)
    s.inputs['Roughness'].default_value=rough;s.inputs['Metallic'].default_value=metal
    s.inputs['Emission Color'].default_value=(*color,1);s.inputs['Emission Strength'].default_value=glow

paint('Concrete',[.27,.285,.26],.86)
paint('Ivory',[.51,.52,.46],.73)
paint('Steel',[.13,.16,.145],.62,.45)
paint('Aluminium',[.25,.27,.245],.42,.94)
paint('Rubber',[.022,.025,.021],.9)
paint('Floor',[.19,.205,.18],.83,.02)
paint('Yellow',[.62,.35,.065],.67,.12)
paint('Teal',[.055,.13,.115],.64,.15)
paint('Light',[1,.94,.79],.4,0,2.6)
paint('AmberLight',[1,.29,.025],.5,0,1.5)
paint('Stair',[.17,.19,.17],.8,.25)
paint('RedOxide',[.24,.045,.028],.71,.1)
paint('Copper',[.26,.15,.075],.58,.72)
paint('Screen',[.04,.33,.29],.4,.05,.8)
paint('Glass',[.12,.18,.17],.19,.15)

# Aged, maintained utility panels are built as shallow wall skins. All hardware
# stays inside the original 30 cm visual wall envelope on both faces.

def replace(name):
    for obj in ASSETS.get(name,[]):bpy.data.objects.remove(obj,do_unlink=True)
    begin(name)

def pipe_path(points,radius,mat='Aluminium',resolution=10):
    curve=bpy.data.curves.new(current+'_formed_tube','CURVE');curve.dimensions='3D'
    curve.resolution_u=resolution;curve.bevel_depth=radius;curve.bevel_resolution=2
    curve.use_fill_caps=True
    spline=curve.splines.new('BEZIER');spline.bezier_points.add(len(points)-1)
    for bp,p in zip(spline.bezier_points,points):
        bp.co=p;bp.handle_left_type='AUTO';bp.handle_right_type='AUTO'
    obj=bpy.data.objects.new(current+'_tube',curve);bpy.context.collection.objects.link(obj)
    return finish_object(obj,mat)

def torus(pos,major,minor,mat='Steel',rotation=(0,0,0),segments=32):
    bpy.ops.mesh.primitive_torus_add(major_segments=segments,minor_segments=8,
        location=pos,rotation=rotation,major_radius=major,minor_radius=minor)
    return finish_object(bpy.context.object,mat)

def bolt(pos,axis='Y',size=1.15):
    cylinder(pos,size*1.35,.35,'Steel',axis,12)
    cylinder(pos,size,.9,'Aluminium',axis,6)

def flange(pos,radius,axis='X',mat='Steel'):
    cylinder(pos,radius,3,mat,axis,24)
    for i in range(8):
        a=i*math.tau/8;u,v=math.cos(a)*radius*.8,math.sin(a)*radius*.8
        p=(pos[0]+2,pos[1]+u,pos[2]+v) if axis=='X' else (pos[0]+u,pos[1]-2,pos[2]+v) if axis=='Y' else (pos[0]+u,pos[1]+v,pos[2]+2)
        bolt(p,axis,.85)

def hazard_line(pos,length,height=8,face='Y'):
    # Separate painted segments on a steel base, no copied signs or typography.
    box(pos,(length,.5,height),'Rubber',.05)
    for i in range(int(length/15)):
        x=pos[0]-length*.5+8+i*15
        obj=box((x,pos[1]-.4,pos[2]),(7,.4,height),'Yellow',.04)
        obj.rotation_euler.y=-.35

replace('Wall')
box((0,0,0),(400,29.8,200),'Concrete',.22)
for side in (-1,1):
    # Formwork tie caps and narrow real construction joints, not repeated enamel panels.
    for x in (-148,148):
        for z in (-68,68):
            cylinder((x,side*14.9,z),2.2,.15,'Concrete','Y',12)
            cylinder((x,side*15,z),.6,.11,'Rubber','Y',8)

replace('Ceiling')
box((0,0,0),(100,100,16),'Concrete',.12)

begin('WallTechnical')
box((0,0,0),(400,20,200),'Concrete',.22)
for side in (-1,1):
    for x in (-181,-156,156,181):
        pipe_path([(x,side*12,-95),(x,side*12,0),(x,side*12,95)],1.7,'RedOxide',4)
        for z in (-76,0,76):box((x,side*12,z),(8,4,4),'Steel',.15)
    box((0,side*11,-6),(253,2,136),'Steel',.4)
    for x in (-90,0,90):
        box((x,side*12,-6),(83,2,125),'Teal',.4)
        for z in range(-54,41,9):box((x,side*13,z),(65,1.3,2),'Rubber',.08)
        box((x-25,side*13,45),(8,2,7),'Screen',.15)
        box((x+27,side*13,40),(4,2,13),'Yellow',.15)
        for z in (-64,53):bolt((x,side*14,z),'Y',.65)
    for z in (74,85,96):
        pipe_path([(-199,side*12,z),(-120,side*12,z),(120,side*12,z),(199,side*12,z)],1.8,'Copper',4)
    for x in (-120,120):
        box((x,side*13,85),(8,3,29),'Steel',.2)
    for x in (-196,196):box((x,side*12,0),(6,4,197),'Aluminium',.3)

begin('WallProcess')
box((0,0,0),(400,20,200),'Concrete',.22)
for side in (-1,1):
    for x,r in ((-157,3.5),(-121,4),(130,3)):
        pipe_path([(x,side*10,-99),(x,side*10,-20),(x,side*10,70),(x,side*10,99)],r,'Aluminium',5)
        for z in (-68,54):
            cylinder((x,side*10,z),r+2.4,3,'Steel','Z',16)
            box((x,side*11,z-9),(14,5,5),'Steel',.3)
    box((8,side*11,-16),(190,2,124),'Steel',.6)
    for x in (-54,8,70):
        cylinder((x,side*12,13),19,3,'Steel','Y',32)
        cylinder((x,side*13.4,13),16,1,'Ivory','Y',32)
        for angle in range(10):
            a=angle*math.pi/6+.3
            px,pz=x+math.cos(a)*12,13+math.sin(a)*12
            marker=box((px,side*14,pz),(2,.4,.7),'Rubber',.02);marker.rotation_euler.y=-a
        pointer=box((x+3,side*14,17),(1,.4,12),'RedOxide',.03);pointer.rotation_euler.y=.6
        pipe_path([(x,side*12,-80),(x,side*12,-50),(x,side*12,-22)],2.3,'Copper',4)
        torus((x,side*12,-40),10,1.6,'RedOxide',(math.pi/2,0,0),20)
    for z in (75,90):
        pipe_path([(-199,side*11,z),(-100,side*11,z),(90,side*11,z),(199,side*11,z)],3,'Copper',4)
    for x in (-194,194):
        box((x,side*11,0),(8,7,196),'RedOxide',.4)
        for z in (-83,-40,0,40,83):bolt((x,side*14,z),'Y',1)

begin('WallData')
box((0,0,0),(400,20,200),'Concrete',.22)
for side in (-1,1):
    for x in (-108,0,108):
        box((x,side*11,-5),(98,2,164),'Steel',.4)
        for z in range(-68,69,19):
            box((x,side*12,z),(87,2,16),'Rubber',.2)
            for dx in range(-33,34,8):box((x+dx,side*13,z),(3,1,9),'Aluminium',.1)
            box((x+39,side*13,z),(2,1,3),'Screen',.1)
    for z in (84,91,98):pipe_path([(-199,side*11,z),(0,side*11,z),(199,side*11,z)],1.5,'Rubber',4)
    for x in (-184,184):
        for dx in (-4,0,4):pipe_path([(x+dx,side*11,-99),(x+dx,side*11,0),(x+dx,side*11,99)],1.4,'RedOxide',4)

replace('Floor')
box((0,0,0),(600,600,16),'Floor',.16)
# Expansion joints remain small relative to real slab size.
for x in (-299,299):box((x,0,7.9),(1,598,.2),'Rubber',.02)

begin('FloorTechnical')
# Recessed utilities under walkable steel grating; the existing continuous floor
# proxy stays at exactly the same height. Slab pieces leave a real visual recess.
box((-92.5,0,0),(415,600,16),'Floor',.2)
box((267.5,0,0),(65,600,16),'Floor',.2)
for y in (-260,260):box((175,y,0),(120,80,16),'Floor',.2)
box((175,0,-7),(120,440,2),'Steel',.2)
for x in (132,153,175,197,218):
    cylinder((x,0,0),2.6,430,'Copper' if x==175 else 'Aluminium','Y',12)
for x in (117,233):box((x,0,5),(5,440,6),'Steel',.25)
for y in range(-214,215,10):box((175,y,6),(112,2.5,3),'Steel',.25)
for x in (147,175,203):box((x,0,6),(2,435,3),'Steel',.2)
for y in (-223,223):
    box((175,y,6),(127,7,3),'Yellow',.2)
    for x in (125,225):bolt((x,y,7),'Z',1)

replace('Beam')
box((0,0,0),(100,9,80),'Steel',.4)
for z in (-44,44):box((0,0,z),(100,76,12),'Steel',.45)
for x in (-43,43):
    for y in (-27,27):bolt((x,y,50),'Z',2)

begin('Column')
box((0,0,0),(100,100,100),'Concrete',.65)
for x in (-46,46):
    for y in (-46,46):box((x,y,0),(6,6,99),'Steel',.2)

begin('StructuralRib')
# Bolted red-oxide channel, including gussets and mounting shoes, not a stretched cube.
box((0,0,0),(10,28,400),'RedOxide',.55)
for side in (-1,1):
    box((0,side*18,0),(44,8,400),'RedOxide',.5)
    for z in (-185,-110,0,110,185):
        box((0,side*22,z),(58,3,19),'Steel',.4)
        for x in (-19,19):bolt((x,side*24,z),'Y',1.6)
for z in (-197,197):
    box((0,0,z),(64,50,6),'Steel',.4)
    for x in (-24,24):
        for y in (-17,17):bolt((x,y,z+4),'Z',1.8)

replace('Lamp')
box((0,0,2),(160,34,12),'Steel',.8)
box((0,0,-3.5),(154,30,2),'Aluminium',.3)
for y in (-8,8):cylinder((0,y,-7),3.5,142,'Light','X',16)
for x in (-74,74):box((x,0,-7),(4,29,9),'Steel',.4)
for x in (-55,55):
    for y in (-15,15):bolt((x,y,8),'Z',.65)

replace('Sign')
box((0,0,0),(100,100,100),'Steel',.2)

replace('PipeRack')
for index,y in enumerate((-29,0,29)):
    r=8 if index!=1 else 11
    pipe_path([(-198,y,0),(-145,y,0),(145,y,0),(198,y,0)],r,'Aluminium')
    for x in (-165,165):flange((x,y,0),r+4,'X')
for x in (-172,172):
    box((x,0,-15),(7,85,6),'Steel',.3)
    for y in (-39,39):box((x,y,0),(5,5,39),'Steel',.3)

begin('ServiceSpine')
for index,y in enumerate((-68,-24,24)):
    r=(12,9,6)[index]
    mat=('Aluminium','Copper','RedOxide')[index]
    pipe_path([(-200,y,10),(-125,y,10),(125,y,10),(200,y,10)],r,mat)
    for x in (-165,165):flange((x,y,10),r+5,'X')
# Cable ladder with real rungs and visibly bundled conductors.
for y in (53,89):box((0,y,3),(400,4,18),'Steel',.3)
for x in range(-190,191,25):box((x,71,-4),(4,34,4),'Aluminium',.2)
for i in range(7):pipe_path([(-200,57+i*4,2),(-60,57+i*4,4),(60,57+i*4,3),(200,57+i*4,2)],1.2,'Rubber',4)
for x in (-170,170):
    box((x,0,-15),(10,195,7),'Steel',.4)
    for y in (-90,90):box((x,y,12),(6,6,60),'Steel',.3)

replace('Duct')
box((0,0,0),(400,150,65),'Aluminium',.65)
for x in (-196,0,196):
    for y in (-76,76):box((x,y,0),(7,5,68),'Steel',.2)
    for z in (-33,33):box((x,0,z),(7,154,4),'Steel',.2)
    for y in (-65,65):
        for z in (-25,25):bolt((x,y,z),'X',.7)

replace('Vent')
box((0,0,0),(160,140,18),'Steel',.7)
box((0,0,-9),(150,130,2),'Rubber',.2)
for y in range(-57,58,9):
    obj=box((0,y,-10),(144,5,3),'Aluminium',.2);obj.rotation_euler.x=.35
for x in (-75,75):
    for y in (-65,65):bolt((x,y,-11),'Z',1)

replace('WallServices')
box((36,4,0),(120,12,140),'Steel',.8)
box((36,-3,0),(112,3,132),'Teal',.5)
for z in (-54,54):box((87,-5,z),(7,3,18),'Aluminium',.4)
box((-4,-6,-4),(4,3,18),'Rubber',.3)
hazard_line((36,-5,30),75,10)
label('VX / POWER\nISOLATE BEFORE SERVICE',(36,-6,7),4,'Ivory')
for x in (-84,-65,-46):
    pipe_path([(x,5,-70),(x,5,-30),(x,5,40),(x,5,70)],2.6,'RedOxide')
    for z in (-49,49):box((x,5,z),(9,8,5),'Steel',.2)
for x in (14,36,58):
    pipe_path([(x,4,-70),(x,4,-84),(x-10,7,-96)],2,'Rubber',6)

begin('ServerRack')
box((0,0,0),(220,140,190),'Steel',1.2)
for side in (-1,1):
    for x in (-54,54):
        box((x,side*69,0),(100,3,176),'Rubber',.4)
        for z in range(-78,79,20):
            box((x,side*71,z),(94,2,15),'Steel',.25)
            for dx in range(-35,36,10):box((x+dx,side*72,z),(3,.7,9),'Rubber',.1)
            box((x+38,side*72,z),(2,1,2),'Screen',.1)
    for x in (-105,0,105):box((x,side*72,0),(5,4,184),'Aluminium',.3)
label('GRID / DATA\nRACK 07',(0,-74,83),5)

replace('LabBench')
box((0,0,0),(220,140,110),'Steel',1)
box((0,0,53),(220,140,4),'Aluminium',.3)
for x in (-74,0,74):
    box((x,-69,0),(70,3,94),'Ivory',.4)
    for z in (-26,0,26):
        box((x,-71,z),(62,1,23),'Ivory',.2)
        box((x,-72,z+5),(24,2,2),'Steel',.3)
label('VX / RESEARCH\nCONTROL MODULE',(0,-73,39),4,'Teal')

replace('Generator')
box((0,0,0),(220,140,110),'Steel',1.2)
for x in (-102,102):box((x,0,0),(12,140,110),'Yellow',.6)
for side in (-1,1):
    for x in (-48,48):
        cylinder((x,side*69,0),39,4,'Rubber','Y',24)
        for angle in range(8):
            a=angle*math.tau/8
            obj=box((x+math.cos(a)*20,side*72,math.sin(a)*20),(30,2,9),'Aluminium',.2)
            obj.rotation_euler.y=-a
        for dx in range(-31,32,7):box((x+dx,side*74,0),(1.4,1,65),'Steel',.1)
hazard_line((0,-76,45),180,7)
label('VX / COOLANT DRIVE',(0,-77,-48),5)

replace('Cover')
box((0,0,0),(220,140,110),'Teal',1.4)
for x in (-104,104):
    for y in (-64,64):box((x,y,0),(12,12,110),'Steel',.5)
for x in (-80,80):box((x,0,53),(14,138,4),'Steel',.5)
for x in (-70,70):
    box((x,-70,0),(45,2,24),'Steel',.4)
    box((x,-72,0),(32,3,7),'Rubber',.5)
label('VANTHEX / SERVICE\nMODULE VX-07',(0,-71,27),6,'Ivory')

replace('Reactor')
cylinder((0,0,-30),165,24,'Steel','Z',64)
cylinder((0,0,-15),151,8,'Aluminium','Z',64)
torus((0,0,-11),139,5,'Copper',segments=48)
for i in range(4):
    a=(i+.5)*math.tau/4;x,y=math.cos(a)*137,math.sin(a)*137
    cylinder((x,y,60),13,140,'Steel','Z',20)
    cylinder((x,y,132),18,7,'Aluminium','Z',20)
    cylinder((x,y,137),12,3,'Screen','Z',20)
    pipe_path([(x*.83,y*.83,-5),(x,y,14),(x,y,95)],4,'Copper')
for i in range(16):
    a=i*math.tau/16;bolt((math.cos(a)*156,math.sin(a)*156,-16),'Z',2)
# Serviceable reactor couplings, segmented deck plates and four independent
# coolant drives. The central Core remains unobstructed and the footprint fixed.
for i in range(12):
    a=i*math.tau/12
    x,y=math.cos(a)*116,math.sin(a)*116
    obj=box((x,y,-6),(48,20,8),'Steel',.7);obj.rotation_euler.z=a+math.pi/2
    cylinder((x,y,0),5,3,'Copper','Z',12)
    bolt((math.cos(a)*147,math.sin(a)*147,-7),'Z',1.1)
for i in range(4):
    a=(i+.5)*math.tau/4;x,y=math.cos(a)*137,math.sin(a)*137
    for offset in (-8,8):
        pipe_path([(x*.82,y*.82,0),(x+offset,y,20),(x+offset,y,108)],2,'Copper',5)
    for z in (18,57,106):
        cylinder((x,y,z),16,5,'Aluminium','Z',20)
    cylinder((x,y,35),19,22,'Teal','Z',20)
    for j in range(8):
        b=j*math.tau/8
        box((x+math.cos(b)*18,y+math.sin(b)*18,35),(2,2,17),'Steel',.2)
    box((x,y-14,84),(16,5,28),'Steel',.6)
    cylinder((x,y-17,88),6,2,'Ivory','Y',24)
    obj=box((x+1,y-18,89),(1,.3,7),'RedOxide',.05);obj.rotation_euler.y=.45
    box((x,y-17,76),(8,1,2),'Screen',.2)

begin('ContainmentCrown')
for z in (-80,20,95):
    torus((0,0,z),360 if z!=20 else 315,14,'Steel',segments=64)
    torus((0,0,z+17),360 if z!=20 else 315,4,'Aluminium',segments=64)
for i in range(12):
    a=i*math.tau/12;x,y=math.cos(a)*360,math.sin(a)*360
    cylinder((x,y,5),12,180,'Steel','Z',16)
    box((x,y,-5),(40,40,72),'Teal',.7)
    cylinder((x,y,-50),19,7,'Copper','Z',16)
    box((x,y,-24),(20,43,4),'Screen',.3)
for side in (-1,1):
    pipe_path([(side*430,0,80),(side*470,0,45),(side*420,0,-35),(side*330,0,-60)],22,'Aluminium')

begin('TransferReceiver')
# Flush deck, open capture footprint and low perimeter couplers.
box((0,0,-5),(440,440,10),'Steel',.5)
for side in (-1,1):
    box((side*215,0,1),(8,430,2),'Yellow',.15)
    box((0,side*215,1),(430,8,2),'Yellow',.15)
    for other in (-1,1):
        cylinder((side*206,other*206,5),10,10,'Aluminium','Z',16)
        cylinder((side*206,other*206,11),6,2,'Screen','Z',16)

begin('TransferBeacon')
box((0,0,0),(28,28,144),'Steel',.6)
box((0,0,-66),(38,38,12),'Steel',.4)
for z in (-38,0,38):box((0,-14,z),(16,2,24),'Light',.4)
for z in (-56,56):hazard_line((0,-16,z),26,5)

begin('CableDrop')
for i in range(5):
    x=-26+i*13
    pipe_path([(x,0,180),(x,0,120),(x+16,-8,80),(x+16,-8,-110),(x+30,0,-180)],2.2,'RedOxide' if i<3 else 'Rubber')
for z in (-110,-20,80,160):box((0,0,z),(100,7,6),'Steel',.3)

begin('MaintenanceBoard')
box((0,0,0),(180,16,160),'Steel',.7)
box((0,-9,0),(168,2,148),'Ivory',.3)
for x in (-55,-20,20,55):
    box((x,-12,5),(5,3,70),'Steel',.4)
    torus((x,-12,40),8,2,'Aluminium',(math.pi/2,0,0),16)
for z in (-50,-42,-34):box((0,-13,z),(110,3,3),'RedOxide',.3)
label('AEGIS / MAINTENANCE',(0,-14,60),6,'Teal')

# Texture content is authored below in Blender, not sampled from the references.
exec(compile((ROOT/'Build/Blender/refinery_textures.py').read_text(),'refinery_textures.py','exec'),globals())
