"""Copy approved finish assets into a stable production art folder; keep PoC intact."""
import unreal as u, json
SRC='/Game/Vanthex/Experiments/GraphicsLab/Revision4'
DST='/Game/Vanthex/Art/IndustrialFinish'
MAP='/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.load_level(MAP)
mapping={}
paths=u.EditorAssetLibrary.list_assets(SRC,recursive=True,include_folder=False)
for path in paths:
    short=path.split('.')[0];dest=DST+short[len(SRC):]
    copy=u.load_asset(dest) if u.EditorAssetLibrary.does_asset_exist(dest) else u.EditorAssetLibrary.duplicate_asset(short,dest)
    assert copy,dest
    if isinstance(copy,u.Material):
        copy.set_editor_property('used_with_instanced_static_meshes',True)
        u.MaterialEditingLibrary.recompile_material(copy)
    assert u.EditorAssetLibrary.save_loaded_asset(copy,only_if_is_dirty=False),dest
    mapping[short]=copy
components=0
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(u.StaticMeshComponent):
        if not c.static_mesh:continue
        mesh_path=c.static_mesh.get_path_name().split('.')[0]
        if mesh_path in mapping:c.set_static_mesh(mapping[mesh_path]);components+=1
        for i in range(c.get_num_materials()):
            m=c.get_material(i)
            if m and m.get_path_name().split('.')[0] in mapping:c.set_material(i,mapping[m.get_path_name().split('.')[0]])
        if a.get_actor_label().startswith(('FacilityStyle_','GraphicsLab_')):
            c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
assert levels.save_current_level()
with open('H:/Vanthex/Saved/GraphicsLab/Facility/art_manifest.json','w') as f:json.dump({'assets':[m.get_path_name() for m in mapping.values()],'mesh_components_updated':components},f,indent=2)
u.log('FACILITY STABLE ART COMPLETE')
