"""Export the locally licensed Energy capsule for Vanthex. Run with Blender --background --python.
No licensed geometry or textures belong in the public repository; output stays in Saved.
"""
import bpy, json, struct, sys
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
source=Path(args[0]) if args else root/'Energy-542e5bc7/blender/source/p_146_MechanicalDoodle_004.blend'
out=root/'Saved/EnergyImport';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False,use_scripts=False)
# Explicit capsule selection excludes the display hand, moths, cameras and unused decal templates.
objects=[o for o in bpy.data.objects if o.type=='MESH' and (o.name.startswith(('Bulb','Glass_','Decals_')) or o.name=='Light') and not o.hide_render]
materials=[]; meshes=[]; points=[]
for obj in objects:
 for mod in obj.modifiers:
  if mod.type=='SUBSURF':mod.levels=min(mod.levels,2)
 evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get()); mesh=evaluated.to_mesh(preserve_all_data_layers=True,depsgraph=bpy.context.evaluated_depsgraph_get())
 mesh.calc_loop_triangles(); meshes.append((obj,evaluated,mesh))
 points.extend(obj.matrix_world@v.co for v in mesh.vertices)
 for mat in mesh.materials:
  if mat and mat.name not in materials:materials.append(mat.name)
lo=Vector(tuple(min(v[i] for v in points)for i in range(3)));hi=Vector(tuple(max(v[i] for v in points)for i in range(3)))
center=(lo+hi)*.5;scale=48/(hi.z-lo.z)
records=[];report=[]
for obj,evaluated,mesh in meshes:
 matrix=obj.matrix_world;normals=matrix.to_3x3().inverted().transposed();uv=mesh.uv_layers.active
 for tri in mesh.loop_triangles:
  mat=mesh.materials[tri.material_index] if mesh.materials else None
  if not mat:continue
  chunk=struct.pack('<I',materials.index(mat.name))
  for idx in (0,2,1):
   loop=tri.loops[idx];v=(matrix@mesh.vertices[tri.vertices[idx]].co-center)*scale
   n=(normals@mesh.corner_normals[loop].vector).normalized();t=uv.data[loop].uv if uv else (0,0)
   chunk+=struct.pack('<8f',v.x,-v.y,v.z,n.x,-n.y,n.z,t[0],1-t[1])
  records.append(chunk)
 report.append({'object':obj.name,'triangles':len(mesh.loop_triangles)})
 evaluated.to_mesh_clear()
assert 0<len(records)<1000000
(out/'energy.vxm').write_bytes(struct.pack('<4I',0x314d5856,1,len(records),len(materials))+b''.join(records))
matdata=[]
for name in materials:
 mat=bpy.data.materials[name];nodes=mat.node_tree.nodes if mat.node_tree else []
 p=next((n for n in nodes if n.type=='BSDF_PRINCIPLED'),None)
 e=next((n for n in nodes if n.type=='EMISSION'),None)
 color=list(p.inputs['Base Color'].default_value[:3])if p else [.12,.15,.18]
 emission=list(e.inputs['Color'].default_value[:3])if e else [0,0,0]
 d={'name':name.replace('.','_'),'color':color,'metallic':float(p.inputs['Metallic'].default_value)if p else .8,'roughness':max(.12,float(p.inputs['Roughness'].default_value))if p else .32,'emission':[c*6 for c in emission],'glass':name.startswith('Glass'),'textures':{}}
 if name=='Bulb_Info':d['textures']={'color':'Bulb_Info_color.png','alpha':'Bulb_Info_alpha.png'}
 if name=='Bulb_Normal':d['textures']={'normal':'Bulb_Normal_normal.png','alpha':'Bulb_Normal_alpha.png','ao':'Bulb_Normal_ao.png'}
 if d['glass']:d.update(color=[.22,.37,.43],metallic=0,roughness=.12)
 matdata.append(d)
(out/'materials.json').write_text(json.dumps({'textures_dir':str(source.parent.parent/'textures'),'materials':matdata},indent=2))
(out/'export_report.json').write_text(json.dumps({'triangles':len(records),'height_cm':48,'source':str(source),'objects':report},indent=2))
print('ENERGY_EXPORT',len(records),'triangles',len(materials),'materials; height 48 cm')
