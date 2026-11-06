"""Read-only saved-map comparison against pre-change collision reference."""
import unreal as u,json,re,os,hashlib
ROOT='H:/Vanthex';OUT=ROOT+'/Saved/GraphicsLab/Facility'
production='FacilityProduction' in u.SystemLibrary.get_command_line()
MAP='/Game/Vanthex/Maps/GridV4' if production else '/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(MAP)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
def V(p):return u.Vector(*p)
def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
rows=[];decoration_collisions=[];experiment_refs=[];missing_visual_meshes=[]
for a in actors:
    for c in a.get_components_by_class(u.PrimitiveComponent):
        if c.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION:
            t=c.get_world_transform();r=t.rotation.rotator()
            rows.append([a.get_actor_label(),c.get_name(),str(c.get_collision_enabled()),str(c.get_collision_profile_name()),[xyz(t.translation),xyz(t.scale3d),[round(r.pitch,3),round(r.yaw,3),round(r.roll,3)]],str(c.get_editor_property('component_tags'))])
            if a.get_actor_label().startswith(('FacilityStyle_','GraphicsLab_')):decoration_collisions.append(a.get_actor_label())
    for c in a.get_components_by_class(u.StaticMeshComponent):
        if a.get_actor_label().startswith(('FacilityStyle_','GraphicsLab_')) and not c.static_mesh:missing_visual_meshes.append(a.get_actor_label())
        objects=[c.static_mesh]+[c.get_material(i) for i in range(c.get_num_materials())]
        experiment_refs.extend(o.get_path_name() for o in objects if o and '/Experiments/GraphicsLab/' in o.get_path_name())
rooms=[]
text=open(ROOT+'/Source/Vanthex/VanthexGridV4Layout.h').read().split('inline const TArray<FConnection>')[0]
for m in re.finditer(r'\{(\d+),TEXT\("([^"]+)"\),\{([^}]+)\},\{([^}]+)\}',text):rooms.append((int(m[1]),[float(n) for n in m[3].split(',')],[float(n) for n in m[4].split(',')]))
queries=[];initial_overlaps=[]
for rid,p,size in rooms:
    x,y,z=p;w,h,_=size
    for xx in range(int(x-w/2+50),int(x+w/2),150):
        for yy in range(int(y-h/2+50),int(y+h/2),150):
            for start,end,capsule in [([xx,yy,z+80],[xx,yy,z-80],False),([xx,yy,z+100],[xx+100,yy,z+100],True)]:
                hit=u.SystemLibrary.capsule_trace_single(world,V(start),V(end),34,88,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True) if capsule else u.SystemLibrary.line_trace_single(world,V(start),V(end),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
                d=hit.to_tuple() if hit else None
                initial_overlaps.append(bool(d[1]) if d else False)
                queries.append([rid,start,end,capsule,bool(d[0]),round(d[3],3),xyz(d[5])] if d else [rid,start,end,capsule,False])
ref=json.load(open(OUT+'/collision_reference.json'))
with open(OUT+'/query_differences.json','w') as f:json.dump([{'before':b,'after':a} for b,a in zip(ref['queries'],queries) if b!=a],f,indent=2)
differences=[(i,b,a) for i,(b,a) in enumerate(zip(ref['queries'],queries)) if b!=a]
overlap_contacts=[];unexplained=[]
for i,b,a in differences:
    # A penetrating capsule has no unique first-contact point. Only classify this
    # exception after independently confirming an initial overlap in the source.
    if b[:6]==a[:6] and b[3] and b[4] and b[5]==0 and initial_overlaps[i] and not production:
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Vanthex/Maps/GridV4')
        original_world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        h=u.SystemLibrary.capsule_trace_single(original_world,V(b[1]),V(b[2]),34,88,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
        d=h.to_tuple() if h else None
        if d and d[0] and d[1] and d[3]==0:overlap_contacts.append({'index':i,'before':b,'after':a,'source_initial_overlap_confirmed':True});continue
    elif b[:6]==a[:6] and b[3] and b[4] and b[5]==0 and initial_overlaps[i] and production:
        # The saved candidate report established this exact start point before promotion.
        candidate=json.load(open(OUT+'/verification_candidate.json'))
        if any(entry['index']==i and entry['source_initial_overlap_confirmed'] for entry in candidate.get('initial_overlap_contact_variations',[])):
            overlap_contacts.append({'index':i,'before':b,'after':a,'source_initial_overlap_confirmed':True});continue
    unexplained.append({'index':i,'before':b,'after':a})
manifest=json.load(open(OUT+'/art_manifest.json'))
missing_asset_files=[]
for path in manifest['assets']:
    package=path.split('.')[0]
    filename=ROOT+'/Content/'+package[len('/Game/'):]+'.uasset'
    if not os.path.isfile(filename) or os.path.getsize(filename)==0:missing_asset_files.append(filename)
report={'map':MAP,'actors':len(actors),'rooms':len(rooms),'collision_components':len(rows),'queries':len(queries),'components_equal':sorted(rows)==ref['components'],'queries_exactly_equal':queries==ref['queries'],'queries_equal':not unexplained,'initial_overlap_contact_variations':overlap_contacts,'unexplained_differences':unexplained,'new_decor_collision_errors':decoration_collisions,'missing_visual_meshes':missing_visual_meshes,'missing_asset_files':missing_asset_files,'experiment_asset_references':sorted(set(experiment_refs))}
suffix='production' if production else 'candidate'
with open(OUT+'/verification_'+suffix+'.json','w') as f:json.dump(report,f,indent=2)
assert report['components_equal'] and report['queries_equal'] and not decoration_collisions,report
assert not experiment_refs,'Candidate still depends on experimental assets'
assert not missing_visual_meshes,'Saved visual meshes are missing'
assert not missing_asset_files,'Production art assets were not persisted to disk'
u.log('FACILITY SAVED VERIFICATION PASS '+json.dumps(report))
