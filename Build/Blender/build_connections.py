"""Original small construction fittings; no replacement architecture or collision."""
import bpy, math, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'ArtSource/Photoreal';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
palette=['Steel','Aluminium','Rubber','Yellow','Teal','Screen','Glass']
mats={n:bpy.data.materials.new(n) for n in palette};assets={};current=None
def finish(obj,material):
    obj.data.materials.append(mats[material]);assets[current].append(obj)
    if obj.type=='MESH':
        bevel=obj.modifiers.new('manufactured radius','BEVEL');bevel.width=.18;bevel.segments=2
        normal=obj.modifiers.new('weighted normals','WEIGHTED_NORMAL');normal.keep_sharp=True
    return obj
def box(p,s,m='Steel'):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=s
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,m)
def cyl(p,r,h,m='Aluminium',axis='Z',vertices=16):
    rotation=(math.pi/2,0,0) if axis=='Y' else (0,math.pi/2,0) if axis=='X' else (0,0,0)
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=h,location=p,rotation=rotation)
    return finish(bpy.context.object,m)
def bolt(p,axis='Z'):
    cyl(p,1.6,.35,'Steel',axis);cyl(p,1.1,1.1,'Aluminium',axis,6)
def tube(points,r=1,m='Rubber'):
    data=bpy.data.curves.new('formed conduit','CURVE');data.dimensions='3D';data.bevel_depth=r;data.bevel_resolution=2
    spline=data.splines.new('BEZIER');spline.bezier_points.add(len(points)-1)
    for p,co in zip(spline.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new('conduit',data);bpy.context.collection.objects.link(o);return finish(o,m)
def begin(name):
    global current
    current=name;assets[name]=[]

begin('PhotoRailJoint')
box((0,0,-47),(20,20,4));box((0,0,-35),(13,13,20))
for x in (-7,7):
    for y in (-7,7):bolt((x,y,-44))
for z in (44,17):
    box((0,0,z),(18,16,8));box((0,-8,z),(16,2,6),'Rubber')
    for x in (-6,6):bolt((x,-9,z),'Y')
for z in (-22,37):box((0,0,z),(15,15,1.2),'Aluminium')

begin('PhotoCableSaddle')
for y in (-64,64):
    cyl((0,y,0),1.5,46,'Aluminium');box((0,y,23),(20,18,4))
    for x in (-6,6):bolt((x,y,25))
box((0,0,-22),(10,142,6))
for y in (-60,-20,20,60):box((0,y,-18),(8,14,2),'Rubber');bolt((0,y,-16))

begin('PhotoCoreJoint')
for z in (-12,12):
    box((0,0,z),(34,34,5))
    for x in (-12,12):
        for y in (-12,12):bolt((x,y,z+3))
cyl((0,0,0),9,20,'Aluminium');cyl((0,0,0),11,7,'Rubber')
for x in (-13,13):box((x,0,0),(4,30,24),'Teal')
for side in (-1,1):tube([(side*10,0,-13),(side*18,0,-6),(side*18,0,13)],1.4,'Yellow')

begin('PhotoMonitor')
box((0,5,0),(38,14,28));box((0,-3,0),(34,2,24),'Rubber')
box((0,-4.2,1),(28,1,17),'Screen');box((0,-4.9,1),(28,.2,17),'Glass')
for x in (-10,-5,0,5,10):box((x,-5,-10),(2,1,2),'Yellow' if x==10 else 'Aluminium')
for x in (-16,16):
    for z in (-11,11):bolt((x,-4,z),'Y')
tube([(0,8,-14),(0,10,-25),(0,11,-52),(0,11,-66)],1.5)
for z in (-22,-58):box((0,10,z),(10,4,4))
box((0,12,0),(26,3,26))
bpy.ops.object.text_add(location=(0,-5.2,6),rotation=(math.pi/2,0,0))
text=bpy.context.object;text.data.body='VX-07 / COOLANT\nFLOW NOMINAL';text.data.size=1.5;text.data.align_x='CENTER';text.data.extrude=.005
finish(text,'Aluminium')

result={'generator':'Blender '+bpy.app.version_string,'units':'centimetres','materials':[{'name':n} for n in palette],'meshes':[]}
graph=bpy.context.evaluated_depsgraph_get()
for index,(name,objects) in enumerate(assets.items()):
    triangles=[]
    for obj in objects:
        evaluated=obj.evaluated_get(graph);mesh=evaluated.to_mesh();mesh.calc_loop_triangles()
        normal_matrix=obj.matrix_world.to_3x3().inverted().transposed()
        for tri in mesh.loop_triangles:
            corners=[]
            for li in tri.loops:
                p=obj.matrix_world@mesh.vertices[mesh.loops[li].vertex_index].co
                n=(normal_matrix@mesh.corner_normals[li].vector).normalized()
                corners.append([round(v,6) for v in (*p,*n)])
            triangles.append({'m':palette.index(obj.data.materials[tri.material_index].name),'v':corners})
        evaluated.to_mesh_clear()
    result['meshes'].append({'name':'SM_B_'+name,'triangles':triangles})
    collection=bpy.data.collections.new(name);bpy.context.scene.collection.children.link(collection)
    for obj in objects:
        for old in list(obj.users_collection):old.objects.unlink(obj)
        collection.objects.link(obj);obj.location.x+=index*220
(OUT/'industrial.json').write_text(json.dumps(result,separators=(',',':')))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Vanthex_Connections.blend'))
print('PHOTO_CONNECTIONS_PASS',sum(len(m['triangles']) for m in result['meshes']))
