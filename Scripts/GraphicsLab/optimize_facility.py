import unreal as u,json
MAP='/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.load_level(MAP)
shadow_details=[];rooms={}
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    label=a.get_actor_label()
    if label in ('FacilityStyle_SM_LabWallCassette','FacilityStyle_SM_LabPilaster'):
        # Existing opaque wall backing already casts the structural shadow.
        # Thousands of 5cm decorative skins do not need to cast it a second time.
        for c in a.get_components_by_class(u.StaticMeshComponent):c.set_cast_shadow(False)
        shadow_details.append(label)
    if label.startswith('Facility_Worklight_'):
        rooms.setdefault(label,[]).append(a)
for label,lights in rooms.items():
    lights.sort(key=lambda a:(a.get_actor_location().x,a.get_actor_location().y))
    for i,a in enumerate(lights):a.get_component_by_class(u.RectLightComponent).set_cast_shadows(i%2==0)
assert levels.save_current_level()
with open('H:/Vanthex/Saved/GraphicsLab/Facility/optimization.json','w') as f:json.dump({'decor_groups_without_redundant_shadow':shadow_details,'room_lights':sum(len(l) for l in rooms.values()),'room_shadow_lights':sum((len(l)+1)//2 for l in rooms.values()),'reason':'Structural wall backing retains shadow; one shadow source per fixture pair.'},f,indent=2)
