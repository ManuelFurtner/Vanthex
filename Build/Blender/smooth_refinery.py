"""Repair curved-surface shading only; keep authored vertices and modifiers."""
import bpy, math
from pathlib import Path
root=Path(__file__).resolve().parents[2]
changed=0
for obj in bpy.data.objects:
    if obj.type!='MESH' or len(obj.data.polygons)<10:continue
    mesh=obj.data;faces={}
    for polygon in mesh.polygons:
        polygon.use_smooth=True
        for edge in polygon.edge_keys:faces.setdefault(tuple(sorted(edge)),[]).append(polygon.normal.copy())
    for edge in mesh.edges:
        normals=faces.get(tuple(sorted(edge.vertices)),[])
        edge.use_edge_sharp=len(normals)!=2 or normals[0].dot(normals[1])<math.cos(math.radians(35))
    changed+=1
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
exec(compile((root/'Build/Blender/export_industrial.py').read_text(),'export_industrial.py','exec'),globals())
print('CURVED_NORMALS_REPAIRED',changed)
