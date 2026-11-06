import unreal as u, json
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Vanthex/Experiments/GraphicsLab/Maps/PowerLab_V2_GridV4')
data=[]
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label().startswith('GraphicsLab'):
        for c in a.get_components_by_class(u.LocalLightComponent):data.append({'actor':a.get_actor_label(),'intensity':c.intensity,'units':str(c.intensity_units),'position':str(a.get_actor_location())})
        if 'PowerCabinet' in a.get_actor_label():data.append({'actor':a.get_actor_label(),'rotation':str(a.get_actor_rotation()),'slots':[str(s.material_slot_name) for s in a.static_mesh_component.static_mesh.static_materials]})
data.append({'rotator':str(u.Rotator(0,180,0))})
with open('H:/Vanthex/Saved/GraphicsLab/correction_inspection.json','w') as f:json.dump(data,f,indent=2)
