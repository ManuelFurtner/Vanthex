"""Author original GRID V4 architecture in Blender. No downloaded art assets.

Run: blender --background --factory-startup --python Build/Blender/build_industrial.py
Units in this authoring scene are centimetres. JSON exports evaluated, triangulated
Blender geometry with split normals; the UE commandlet handles the handedness change.
The .blend remains the editable source of truth.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
REFINERY = '--refinery' in sys.argv
OUT = ROOT / 'ArtSource' / ('Refinery' if REFINERY else 'Industrial')
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = .01

PALETTE = {
    'Concrete': ([.46, .43, .37], .88, 0, 0),
    'Ivory': ([.65, .68, .65], .53, .1, 0),
    'Steel': ([.18, .21, .23], .44, .55, 0),
    'Aluminium': ([.44, .49, .51], .3, .95, 0),
    'Rubber': ([.019, .025, .028], .83, 0, 0),
    'Floor': ([.31, .30, .27], .72, .06, 0),
    'Yellow': ([.72, .36, .055], .48, .25, 0),
    'Teal': ([.035, .26, .29], .47, .3, 0),
    'Light': ([.72, .85, 1.0], .29, .1, 6),
    'AmberLight': ([1.0, .36, .055], .35, 0, 4),
    'Stair': ([.27, .28, .27], .84, .12, 0),
}
MATS = {}
for name, (color, rough, metal, emission) in PALETTE.items():
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = rough
    shader.inputs['Metallic'].default_value = metal
    shader.inputs['Emission Color'].default_value = (*color, 1)
    shader.inputs['Emission Strength'].default_value = emission
    MATS[name] = mat

ASSETS = {}
current = None
def begin(name):
    global current
    current = name
    ASSETS[name] = []

def finish_object(obj, mat, bevel=0):
    obj.name = current + '_' + str(len(ASSETS[current]))
    obj.data.materials.append(MATS[mat])
    if bevel:
        modifier = obj.modifiers.new('Manufactured edge radius', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 2
        modifier.affect = 'EDGES'
        modifier = obj.modifiers.new('Weighted face normals', 'WEIGHTED_NORMAL')
        modifier.keep_sharp = True
    ASSETS[current].append(obj)
    return obj

def box(pos, size, mat='Steel', bevel=.3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    obj = bpy.context.object
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish_object(obj, mat, min(bevel, min(size)*.19))

def cylinder(pos, radius, depth, mat='Steel', axis='Z', vertices=16):
    rotation = {'Z': (0,0,0), 'X': (0,math.pi/2,0), 'Y': (math.pi/2,0,0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=pos, rotation=rotation)
    obj = bpy.context.object
    return finish_object(obj, mat, .22)

def label(body, pos, size=5, mat='Ivory'):
    bpy.ops.object.text_add(location=pos, rotation=(math.pi/2,0,0))
    obj=bpy.context.object
    obj.data.body=body
    obj.data.align_x='CENTER'
    obj.data.size=size
    obj.data.extrude=.012
    obj.data.space_line=1.2
    return finish_object(obj,mat)

def strut(a,b,width=5,mat='Steel'):
    a,b=Vector(a),Vector(b)
    obj=box((a+b)*.5,(width,width,(b-a).length),mat,.35)
    obj.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    return obj

# Two-sided concrete/panel wall. All ornament stays within the old 30cm envelope.
begin('Wall')
box((0,0,0),(400,23,200),'Concrete',1.0)
for side in (-1,1):
    for x in (-99,99):
        box((x,side*12,6),(190,3,172),'Ivory',.65)
        box((x,side*13.8,-68),(183,1.2,19),'Teal',.15)
    for z in (-96,96):
        box((0,side*12.7,z),(400,3.8,8),'Steel',.5)
    box((0,side*13.8,0),(3.2,2.3,180),'Steel',.3)
    for x in (-190,190):
        for z in (-83,83):
            cylinder((x,side*14,z),1.1,1.6,'Aluminium','Y',8)

begin('Floor')
box((0,0,-1),(600,600,14),'Concrete',.35)
for x in (-150,150):
    for y in (-150,150):
        box((x,y,6.1),(298,298,3.8),'Floor',.3)
        for dx in (-140,140):
            for dy in (-140,140): cylinder((x+dx,y+dy,8),.9,.25,'Aluminium','Z',8)

begin('Ceiling')
box((0,0,0),(100,100,16),'Concrete',.2)
box((0,0,-7.6),(94,94,.8),'Ivory',.1)
for x in (-49,49): box((x,0,-7.4),(2,100,1.2),'Steel',.1)
for y in (-49,49): box((0,y,-7.4),(100,2,1.2),'Steel',.1)

begin('Beam')
box((0,0,0),(100,72,76),'Steel',1)
for z in (-44,44): box((0,0,z),(100,100,12),'Steel',1)
for x in (-42,42):
    for y in (-37,37): cylinder((x,y,50),3,1.2,'Aluminium','Z',8)

begin('Lamp')
box((0,0,1),(120,38,22),'Steel',1.7)
box((0,0,-9.3),(112,30,3.4),'Aluminium',.5)
box((0,0,-11.1),(104,23,1.8),'Light',.5)
for x in (-56,56): box((x,0,-9),(3,32,6),'Rubber',.4)

begin('Cover')
box((0,0,0),(220,140,110),'Steel',3)
for y in (-1,1):
    box((0,y*68,0),(192,4,86),'Teal',1)
    for x in (-89,89): box((x,y*69,0),(10,2,87),'Aluminium',.5)
    box((0,y*70,24),(65,1,14),'Yellow',.3)
    box((0,y*71,24),(43,.5,7),'Rubber',.1)
for x in (-106,106):
    for y in (-65,65): box((x,y,0),(8,10,110),'Rubber',.8)
for x in (-84,84): box((x,0,54),(13,136,2),'Aluminium',.3)
label('VANTHEX\nFIELD STORAGE',(0,-71.6,-10),7)

begin('Tread')
box((0,0,0),(100,100,100),'Stair',.1)
box((0,-42,49.8),(99,14,.4),'Yellow',.1)
for y in (-32,-12,8,28): box((0,y,49.9),(98,1,.2),'Rubber',.05)

begin('LabBench')
box((0,0,-2),(220,140,106),'Ivory',1.2)
box((0,0,53),(220,140,4),'Aluminium',.5)
for y in (-69,69):
    for x in (-54,54):
        for z in (-30,0,30):
            box((x,y,z),(102,2,28),'Ivory',.3)
            box((x,y*1.016,z+5),(32,1.5,2),'Steel',.3)
for x in (-104,104):box((x,0,-46),(10,138,14),'Rubber',.5)

begin('Generator')
box((0,0,0),(220,140,110),'Steel',2.2)
for x in (-90,90):box((x,0,0),(18,144,114),'Yellow',1.2)
for side in (-1,1):
    box((0,side*69,2),(145,4,88),'Rubber',.5)
    for x in (-40,40):
        cylinder((x,side*72,2),33,4,'Aluminium','Y',24)
        cylinder((x,side*75,2),28,2,'Rubber','Y',24)
        cylinder((x,side*77,2),8,3,'Aluminium','Y',16)
        for a in range(5):
            angle=a*math.tau/5
            blade=box((x+math.cos(angle)*16,side*77,2+math.sin(angle)*16),(27,2,7),'Steel',.4)
            blade.rotation_euler.y=-angle
for z in (-40,40):box((0,-77,z),(145,4,4),'Aluminium',.4)

begin('WallServices')
box((0,8,0),(240,5,120),'Concrete',.4)
for x in (-74,-45):
    cylinder((x,0,0),7,114,'Aluminium','Z',16)
    for z in (-45,45):cylinder((x,0,z),10,5,'Steel','Z',16)
cylinder((-74,-7,0),14,3,'Yellow','Y',24)
cylinder((-74,-10,0),10,2,'Steel','Y',24)
box((40,0,0),(95,18,110),'Steel',1)
box((40,-10,0),(85,2,99),'Ivory',.6)
box((40,-12,27),(33,1,25),'Yellow',.2)
for z in (-35,-29,-23,-17):box((40,-12,z),(56,1.3,2),'Rubber',.1)
label('CAUTION\nHIGH VOLTAGE',(40,-12.9,31),4.2,'Rubber')

begin('CageLight')
cylinder((0,0,0),16,18,'Steel','Y',24)
cylinder((0,-10,0),12,5,'AmberLight','Y',24)
for x in (-12,0,12):box((x,-14,0),(2,3,27),'Steel',.3)
for z in (-12,12):box((0,-14,z),(29,3,2),'Steel',.3)

begin('Sign')
box((0,0,0),(100,100,100),'Steel',1)

begin('Truss')
for y in (-32,32):
    for z in (-32,32):box((0,y,z),(400,12,12),'Steel',.7)
    for x in (-190,-95,0,95,190):box((x,y,0),(8,10,64),'Aluminium',.4)
    for i in range(4):
        x=-190+i*95
        strut((x,y,-28),(x+95,y,28),7)
for x in (-190,190):box((x,0,0),(12,76,76),'Steel',.6)

begin('Duct')
box((0,0,0),(400,150,65),'Aluminium',1.3)
for x in (-192,0,192):
    for y in (-74,74):box((x,y,0),(8,4,65),'Steel',.3)
    for z in (-32,32):box((x,0,z),(8,150,4),'Steel',.3)
for x in range(-160,161,40):box((x,0,-32),(2,138,1),'Steel',.1)

# Service modules for selected ceiling margins; never enter the player capsule.
begin('PipeRack')
for y in (-30,0,30):
    cylinder((0,y,0),8 if y else 12,400,'Aluminium','X',16)
    for x in (-170,170):
        cylinder((x,y,0),11 if y else 15,9,'Steel','X',16)
for x in (-175,175):
    box((x,0,-15),(12,88,7),'Steel',.5)
    for y in (-39,39): box((x,y,2),(6,6,40),'Steel',.3)

begin('Vent')
box((0,0,0),(160,140,22),'Steel',1)
box((0,0,-10),(148,128,3),'Rubber',.4)
for x in range(-65,66,10): box((x,0,-12),(5,116,4),'Aluminium',.5)
for x in (-75,75):
    for y in (-65,65): cylinder((x,y,-12),1.8,2,'Aluminium','Z',8)

begin('Terminal')
box((0,0,0),(100,25,140),'Steel',1)
box((0,-13,15),(87,3,102),'Ivory',.8)
box((0,-15,27),(67,2,46),'Rubber',.5)
for x in range(-26,27,9):
    for z in (-32,-23): box((x,-16,z),(5,2,4),'Aluminium',.2)
for z in (15,25,35): box((-5,-16.3,z),(43,1,1.3),'Light',.1)
box((30,-16,-4),(6,2,6),'AmberLight',.5)
label('SYSTEM / 07',(0,-16.9,43),3.7,'Teal')

begin('Reactor')
cylinder((0,0,-22),165,36,'Steel','Z',48)
cylinder((0,0,-3),147,6,'Aluminium','Z',48)
for i in range(12):
    a=i*math.tau/12
    x,y=math.cos(a)*145,math.sin(a)*145
    cylinder((x,y,-10),4.5,22,'Yellow','Z',8)
for i in range(8):
    a=i*math.tau/8
    x,y=math.cos(a)*135,math.sin(a)*135
    box((x,y,80),(16,16,180),'Steel',1)
    box((x,y,171),(22,22,5),'AmberLight',.5)

if REFINERY:
    exec(compile((ROOT/'Build/Blender/refinery_details.py').read_text(), 'refinery_details.py', 'exec'), globals())

# Export evaluated Blender triangles, preserving material slots and real edge normals.
depsgraph=bpy.context.evaluated_depsgraph_get()
manifest={'generator':'Blender '+bpy.app.version_string,'units':'centimetres','edition':'Refinery' if REFINERY else 'Industrial','materials':[], 'meshes':[]}
names=list(PALETTE)
for name,(color,rough,metal,emission) in PALETTE.items():
    manifest['materials'].append({'name':name,'color':color,'roughness':rough,'metallic':metal,'emission':emission,**({'texture_family':REFINERY_FAMILIES[name]} if REFINERY else {})})
for name,objects in ASSETS.items():
    triangles=[]
    for obj in objects:
        evaluated=obj.evaluated_get(depsgraph)
        mesh=evaluated.to_mesh()
        mesh.calc_loop_triangles()
        matrix=obj.matrix_world
        normal_matrix=matrix.to_3x3().inverted().transposed()
        for tri in mesh.loop_triangles:
            vertices=[]
            for li in tri.loops:
                p=matrix @ mesh.vertices[mesh.loops[li].vertex_index].co
                n=(normal_matrix @ mesh.corner_normals[li].vector).normalized()
                vertices.append([round(v,6) for v in (*p,*n)])
            triangles.append({'m':names.index(obj.data.materials[tri.material_index].name),'v':vertices})
        evaluated.to_mesh_clear()
    manifest['meshes'].append({'name':'SM_B_'+name,'triangles':triangles})
    print('EXPORT',name,len(triangles),'triangles')
(OUT/'industrial.json').write_text(json.dumps(manifest,separators=(',',':')))
# Put module sources in separate named collections for human editing.
for index,(name,objects) in enumerate(ASSETS.items()):
    collection=bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    origin=Vector(((index%5)*700,(index//5)*700,0))
    collection['vanthex_asset_name']='SM_B_'+name
    collection['vanthex_origin']=list(origin)
    for obj in objects:
        for old in list(obj.users_collection):old.objects.unlink(obj)
        collection.objects.link(obj)
        obj.location+=origin
        obj.hide_render=False
bpy.context.scene['vanthex_palette_json']=json.dumps(manifest['materials'])
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/('Vanthex_Refinery.blend' if REFINERY else 'Vanthex_Industrial.blend')))
print('VANTHEX_BLENDER_EXPORT_OK',len(manifest['meshes']))
