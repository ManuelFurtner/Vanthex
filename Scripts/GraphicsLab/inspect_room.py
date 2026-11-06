import unreal as u
import json, os
OUT = 'H:/Vanthex/Saved/GraphicsLab'
os.makedirs(OUT, exist_ok=True)
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Vanthex/Maps/GridV4')
def vec(v): return [round(v.x,3),round(v.y,3),round(v.z,3)]
def trans(t): return {'p':vec(t.translation),'s':vec(t.scale3d),'r':str(t.rotation.rotator())}
def inside(p): return 2080<=p.x<=3920 and 2380<=p.y<=3620 and -100<=p.z<=440
rows=[]
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    comps=[]
    for c in a.get_components_by_class(u.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh: continue
        instances=[]
        if isinstance(c,u.InstancedStaticMeshComponent):
            for i in range(c.get_instance_count()):
                t=c.get_instance_transform(i,world_space=True)
                if inside(t.translation): instances.append({'i':i,**trans(t)})
        elif inside(c.get_world_location()): instances=[trans(c.get_world_transform())]
        if instances: comps.append({'component':c.get_name(),'mesh':mesh.get_path_name(),'collision':str(c.get_collision_enabled()),'materials':[str(c.get_material(i).get_path_name()) if c.get_material(i) else None for i in range(c.get_num_materials())],'instances':instances})
    if comps or inside(a.get_actor_location()): rows.append({'label':a.get_actor_label(),'class':a.get_class().get_name(),'p':vec(a.get_actor_location()),'components':comps})
names=['SM_Engine','SM_EngineModifyPivot','SM_EngineWithContainer','SM_ContainerEngine','SM_TrainEngine01','SM_TrainEngineFan01','SM_Motor','SM_Boiler_01','SM_ElectricalPanel_01','SM_ElectricalPanel_02','SM_ElectricalPanel_03','SM_ConcreteWall','SM_ConcreteFloor_01','SM_MetalWall_02','SM_MetalWallPart_01','SM_MetalBeam01','SM_MetalBeam02','SM_Lamp01','SM_Lamp02','SM_Lamp03','SM_LampSet_01','SM_CeilingVentilation01','SM_AirVentilation01','SM_Pipe_round_long','SM_Pipe_round_corner1','SM_WireSetWall01','SM_WireSetTop01','SM_FloorDrainage01','SM_WaterContainer01','SM_DeskControl_01','SM_pipe_conditioning01_01','SM_Pipes_round_section1']
assets=[]
for name in names:
    m=u.load_asset('/Game/Meshes/'+name)
    if not m: continue
    b=m.get_bounds()
    assets.append({'name':name,'origin':vec(b.origin),'extent':vec(b.box_extent),'materials':[s.material_interface.get_path_name() if s.material_interface else None for s in m.static_materials]})
with open(OUT+'/inventory.json','w') as f: json.dump({'actors':rows,'assets':assets},f,indent=2)
u.log('GRAPHICSLAB INVENTORY COMPLETE')
