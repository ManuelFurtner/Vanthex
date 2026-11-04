import unreal as u,json
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4')
rows=[]
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label().startswith('FacilityStyle_') or a.get_actor_label()=='GridV4_SM_ConcreteFloor_01':
        d={'actor':a.get_actor_label(),'location':str(a.get_actor_location()),'hidden':a.get_editor_property('hidden'),'editor_only':a.get_editor_property('is_editor_only_actor'),'components':[]}
        for c in a.get_components_by_class(u.StaticMeshComponent):
            e={'name':c.get_name(),'mesh':c.static_mesh.get_path_name() if c.static_mesh else None,'visible':c.is_visible(),'hidden':c.get_editor_property('hidden_in_game'),'transform':str(c.get_world_transform()),'materials':[str(c.get_material(i)) for i in range(c.get_num_materials())]}
            if isinstance(c,u.InstancedStaticMeshComponent):e['count']=c.get_instance_count();e['first']=str(c.get_instance_transform(0,world_space=True)) if c.get_instance_count() else None
            d['components'].append(e)
        rows.append(d)
with open('H:/Vanthex/Saved/GraphicsLab/Facility/render_inventory.json','w') as f:json.dump(rows,f,indent=2)
