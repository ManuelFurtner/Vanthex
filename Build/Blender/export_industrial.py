"""Export an edited Vanthex_Industrial.blend without regenerating/overwriting it.

blender --background ArtSource/Industrial/Vanthex_Industrial.blend --python Build/Blender/export_industrial.py
The collection's export origin removes gallery spacing, while retaining edits.
"""
import bpy
import json
from pathlib import Path
from mathutils import Vector

root=Path(__file__).resolve().parents[2]
materials=json.loads(bpy.context.scene['vanthex_palette_json'])
for entry in materials:
    material=bpy.data.materials.get(entry['name'])
    shader=material.node_tree.nodes.get('Principled BSDF') if material and material.use_nodes else None
    if shader:
        entry['color']=[round(v,6) for v in shader.inputs['Base Color'].default_value[:3]]
        entry['roughness']=round(shader.inputs['Roughness'].default_value,6)
        entry['metallic']=round(shader.inputs['Metallic'].default_value,6)
        entry['emission']=round(shader.inputs['Emission Strength'].default_value,6)
names=[m['name'] for m in materials]
refined=Path(bpy.data.filepath).stem=='Vanthex_Refinery'
result={'generator':'Blender '+bpy.app.version_string,'edition':'Refinery' if refined else 'Industrial','units':'centimetres','materials':materials,'meshes':[]}
graph=bpy.context.evaluated_depsgraph_get()
for collection in bpy.data.collections:
    if 'vanthex_asset_name' not in collection:continue
    origin=Vector(collection['vanthex_origin'])
    triangles=[]
    for obj in collection.all_objects:
        if obj.type not in {'MESH','FONT','CURVE'}:continue
        for mat in obj.data.materials:
            if mat.name not in names:raise ValueError('Add material to scene palette before export: '+mat.name)
        evaluated=obj.evaluated_get(graph)
        mesh=evaluated.to_mesh()
        mesh.calc_loop_triangles()
        normal_matrix=obj.matrix_world.to_3x3().inverted().transposed()
        for tri in mesh.loop_triangles:
            corners=[]
            for li in tri.loops:
                point=obj.matrix_world@mesh.vertices[mesh.loops[li].vertex_index].co-origin
                normal=(normal_matrix@mesh.corner_normals[li].vector).normalized()
                corners.append([round(v,6) for v in (*point,*normal)])
            triangles.append({'m':names.index(obj.data.materials[tri.material_index].name),'v':corners})
        evaluated.to_mesh_clear()
    result['meshes'].append({'name':collection['vanthex_asset_name'],'triangles':triangles})
if len(result['meshes'])<12:raise ValueError('Not a complete VANTHEX authoring scene')
(root/('ArtSource/Refinery' if refined else 'ArtSource/Industrial')/'industrial.json').write_text(json.dumps(result,separators=(',',':')))
print('VANTHEX_BLENDER_EDITED_EXPORT_OK',len(result['meshes']))
