"""Read-only reload verification of collision queries in source and experiment."""
import unreal as u
import json, hashlib, os, re
OUT='H:/Vanthex/Saved/GraphicsLab'
rev=int(re.search(r'GraphicsLabRevision=(\d+)',u.SystemLibrary.get_command_line()).group(1))
def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
def run(path):
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(path)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    rows=[]
    for a in actors:
        for c in a.get_components_by_class(u.PrimitiveComponent):
            if c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION:continue
            r=c.get_world_rotation()
            rows.append([a.get_actor_label(),c.get_name(),str(c.get_collision_profile_name()),str(c.get_collision_enabled()),xyz(c.get_world_location()),xyz(c.get_world_scale()),[round(r.pitch,3),round(r.yaw,3),round(r.roll,3)]])
    rays=[]
    for x in range(2150,3900,100):
        for y in range(2450,3600,100):rays.append(((x,y,80),(x,y,-80)))
    for z in (70,165,290):
        for x in range(2150,3900,100):rays.append(((x,2350,z),(x,3650,z)))
        for y in range(2450,3600,100):rays.append(((2050,y,z),(3950,y,z)))
    hits=[]
    for start,end in rays:
        h=u.SystemLibrary.line_trace_single(world,u.Vector(*start),u.Vector(*end),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
        d=h.to_tuple() if h else None
        hits.append([bool(d[0]),round(d[3],3),xyz(d[5])] if d else [False])
    capsules=[]
    for x in range(2200,3900,100):
        for y in (2600,3000,3400):
            h=u.SystemLibrary.capsule_trace_single(world,u.Vector(x,y,100),u.Vector(x+100,y,100),34,88,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
            d=h.to_tuple() if h else None
            capsules.append([bool(d[0]),round(d[3],3),xyz(d[5])] if d else [False])
    props=[a for a in actors if a.get_actor_label().startswith('GraphicsLab_')]
    return {'components':sorted(rows),'rays':hits,'capsules':capsules,'decor_actors':len(props),'actor_count':len(actors)}
source=run('/Game/Vanthex/Maps/GridV4')
lab=run('/Game/Vanthex/Experiments/GraphicsLab/Maps/PowerLab_V%d_GridV4'%rev)
report={'revision':rev,'source_actor_count':source['actor_count'],'experiment_actor_count':lab['actor_count'],'collision_component_count':len(source['components']),'rays':len(source['rays']),'capsules':len(source['capsules']),'collision_components_equal':source['components']==lab['components'],'ray_results_equal':source['rays']==lab['rays'],'capsule_results_equal':source['capsules']==lab['capsules'],'production_sha256':hashlib.sha256(open('H:/Vanthex/Content/Vanthex/Maps/GridV4.umap','rb').read()).hexdigest()}
with open(OUT+'/saved_verification_v%d.json'%rev,'w') as f:json.dump(report,f,indent=2)
assert report['collision_components_equal'] and report['ray_results_equal'] and report['capsule_results_equal'],json.dumps(report)
u.log('GRAPHICSLAB SAVED VERIFICATION PASS '+json.dumps(report))
