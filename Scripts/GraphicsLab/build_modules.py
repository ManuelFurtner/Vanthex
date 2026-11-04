"""Original beveled modules, centimeters after FBX import; no third-party geometry."""
import bpy, math, os
from mathutils import Vector
OUT='H:/Vanthex/Saved/GraphicsLab/Modules'
os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
def mat(name,col,metal,rough):
    m=bpy.data.materials.new(name); m.diffuse_color=(*col,1); m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value=(*col,1); p.inputs['Metallic'].default_value=metal; p.inputs['Roughness'].default_value=rough
    return m
steel=mat('LabSteel',(.08,.1,.12),.85,.4)
paint=mat('LabPaint',(.24,.28,.28),.4,.55)
black=mat('LabRubber',(.018,.022,.024),0,.85)
yellow=mat('LabAmber',(.7,.34,.04),.3,.5)
parts=[]
def box(name,p,s,m,bevel=.012):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p); o=bpy.context.object; o.name=name
    o.dimensions=s; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(m)
    if bevel:
        mod=o.modifiers.new('Machined edges','BEVEL'); mod.width=bevel; mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for f in o.data.polygons: f.use_smooth=False
    parts.append(o); return o
def save(name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.join(); o=bpy.context.object
    bpy.context.scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    o.name=name
    bpy.ops.export_scene.fbx(filepath=OUT+'/'+name+'.fbx',use_selection=True,apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',object_types={'MESH'},add_leaf_bones=False,bake_anim=False)
    bpy.data.objects.remove(o,do_unlink=True);parts.clear()
# Slender pilaster: front face at Y=-.12, back at0; center origin.
box('Folded column',(0,-.04,2.05),(.16,.08,4.1),steel)
for z in (.06,4.04):box('Anchor shoe',(0,-.07,z),(.24,.14,.12),steel)
save('SM_LabPilaster')
# North wall panel, 3m wide x2m high; 3cm deep. Inset ribbing and real bevels.
box('Cassette',(0,-.015,0),(2.96,.03,1.96),paint)
for x in (-1.46,1.46):box('Folded edge',(x,-.035,0),(.025,.025,1.92),steel,.004)
for z in (-.96,.96):box('Folded edge',(0,-.035,z),(2.92,.025,.025),steel,.004)
for x in (-1.42,1.42):
    for z in (-.88,.88):box('Fastener',(x,-.055,z),(.025,.016,.025),steel,.005)
save('SM_LabWallCassette')
# 2m open cable ladder; repeated rungs and side rails, no giant stretched texture.
for y in (-.17,.17):box('Tray rail',(0,y,0),(2,.03,.1),steel,.006)
for i in range(10):box('Rung',(-.9+i*.2,0,-.035),(.035,.34,.03),steel,.004)
for y in (-.09,-.03,.03,.09):box('Cable',(0,y,.005),(2,.025,.025),black,.01)
save('SM_LabCableTray')
# Ceiling modular coffers, room kept at original collision height 420cm.
box('Acoustic steel cassette',(0,0,-.025),(2.98,2.98,.05),paint,.008)
for x in (-1.48,1.48):box('Seam',(x,0,-.06),(.025,2.96,.025),steel,.003)
for y in (-1.48,1.48):box('Seam',(0,y,-.06),(2.96,.025,.025),steel,.003)
save('SM_LabCeilingCassette')
# 250 x210 x300cm cover enclosure, same envelope as existing boiler collision.
# Dense front geometry, inset service doors, louvers, feet.
box('Power enclosure',(0,0,1.5),(2.48,2.08,2.98),paint,.035)
box('Service panel',(0,-1.05,1.65),(2.23,.025,2.25),steel,.015)
for x in (-.57,.57):
    box('Service door',(x,-1.07,1.7),(1.07,.025,2.05),paint,.012)
    box('Pull handle',(x+.32,-1.105,1.72),(.04,.045,.28),steel,.009)
    for z in (.87,1.,1.13,1.26):box('Louver',(x,-1.095,z),(.85,.025,.045),black,.004)
box('Warning band',(0,-1.085,2.68),(2.15,.015,.085),yellow,.002)
for side in (-1,1):
    # Side heat exchanger: inset frame, grilles and circular fan guard.
    box('Side service panel',(side*1.247,0,1.6),(.025,1.85,2.35),steel,.01)
    for z in (.65,.77,.89,1.01,2.45,2.57):box('Side louver',(side*1.265,0,z),(.025,1.55,.045),black,.003)
    bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=.54,depth=.018,location=(side*1.271,0,1.74),rotation=(0,math.pi/2,0))
    o=bpy.context.object;o.name='Fan recess';o.data.materials.append(black);parts.append(o)
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=8,location=(side*1.29,0,1.74),rotation=(0,math.pi/2,0),major_radius=.54,minor_radius=.022)
    o=bpy.context.object;o.name='Fan guard ring';o.data.materials.append(steel);parts.append(o)
    for i in range(8):
        angle=i*math.pi/4
        o=box('Fan guard spoke',(side*1.3,math.sin(angle)*.26,1.74+math.cos(angle)*.26),(.018,.018,.52),steel,.004)
        o.rotation_euler.x=-angle
    for y in (-.82,.82):
        for z in (.52,2.68):box('Side fastener',(side*1.278,y,z),(.02,.028,.028),yellow,.004)
for x in (-1.1,1.1):box('Floor mounting',(x,0,.06),(.25,2.1,.12),steel,.012)
save('SM_LabPowerCabinet')
print('GRAPHICSLAB MODULES COMPLETE')
