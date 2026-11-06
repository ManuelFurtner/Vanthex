"""Check whether the legacy connected-rooms diagnostic camera starts in geometry."""
import unreal as u,json,math
results=[]
for name in ['/Game/Vanthex/Maps/GridV4','/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4']:
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(name)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    for label,coordinates in [('12_connected_rooms',[-1800,-400,165]),('22_closed_service_boundary',[-700,-2700,-435])]:
        start=u.Vector(*coordinates)
        hit=u.SystemLibrary.sphere_trace_single(world,start,start+u.Vector(.1,0,0),1,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
        d=hit.to_tuple() if hit else None
        near=[]
        for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
            for c in a.get_components_by_class(u.StaticMeshComponent):
                if not c.static_mesh:continue
                b=c.static_mesh.get_bounds()
                transforms=[(i,c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())] if isinstance(c,u.InstancedStaticMeshComponent) else [(-1,c.get_world_transform())]
                for i,t in transforms:
                    p=u.MathLibrary.inverse_transform_location(t,start)-b.origin
                    e=b.box_extent;s=t.scale3d
                    dist=math.sqrt(sum((max(abs(v)-extent,0)*abs(scale))**2 for v,extent,scale in [(p.x,e.x,s.x),(p.y,e.y,s.y),(p.z,e.z,s.z)]))
                    if dist<30:near.append({'actor':a.get_actor_label(),'mesh':c.static_mesh.get_name(),'instance':i,'distance_to_bounds':dist,'location':str(t.translation),'rotation':str(t.rotation.rotator()),'scale':str(s),'local_offset':str(p),'bounds_extent':str(e)})
        results.append({'map':name,'view':label,'camera':coordinates,'blocking':bool(d[0]) if d else False,'initial_overlap':bool(d[1]) if d else False,'distance':d[3] if d else None,'details':str(d),'near_geometry':near})
with open('H:/Vanthex/Saved/GraphicsLab/Facility/capture_origin.json','w') as f:json.dump(results,f,indent=2)
u.log('CAPTURE ORIGIN '+json.dumps(results))
