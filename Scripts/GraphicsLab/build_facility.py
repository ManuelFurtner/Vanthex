"""Extend the approved PowerLab finish to GRID V4, without rebuilding gameplay geometry."""
import unreal as u
import json, os, re, hashlib, math
ROOT='H:/Vanthex';OUT=ROOT+'/Saved/GraphicsLab/Facility'
os.makedirs(OUT,exist_ok=True)
BASE='/Game/Vanthex/Experiments/GraphicsLab/Revision4'
DEST='/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
SOURCE=ROOT+'/Content/Vanthex/Maps/GridV4.umap'
source_hash=hashlib.sha256(open(SOURCE,'rb').read()).hexdigest()
actors=u.get_editor_subsystem(u.EditorActorSubsystem);levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
def V(p):return u.Vector(*p)
def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
def tr(t):
    r=t.rotation.rotator();return [xyz(t.translation),xyz(t.scale3d),[round(r.pitch,3),round(r.yaw,3),round(r.roll,3)]]
def signature():
    out=[]
    for a in actors.get_all_level_actors():
        for c in a.get_components_by_class(u.PrimitiveComponent):
            if c.get_collision_enabled()!=u.CollisionEnabled.NO_COLLISION:
                out.append([a.get_actor_label(),c.get_name(),str(c.get_collision_enabled()),str(c.get_collision_profile_name()),tr(c.get_world_transform()),str(c.get_editor_property('component_tags'))])
    return sorted(out)
text=open(ROOT+'/Source/Vanthex/VanthexGridV4Layout.h').read().split('inline const TArray<FConnection>')[0]
rooms=[]
for m in re.finditer(r'\{(\d+),TEXT\("([^"]+)"\),\{([^}]+)\},\{([^}]+)\}',text):
    rooms.append({'id':int(m[1]),'name':m[2],'p':[float(n) for n in m[3].split(',')],'size':[float(n) for n in m[4].split(',')]})
assert len(rooms)==25
def room_for(p,margin=0):
    for r in rooms:
        x,y,z=r['p'];w,h,tall=r['size']
        if abs(p.x-x)<=w/2+margin and abs(p.y-y)<=h/2+margin and z-25<=p.z<=z+min(tall,420)+25:return r
    return None
def protected(p):return 2080<=p.x<=3920 and 2380<=p.y<=3620 and -30<=p.z<=445
def probes(world):
    out=[]
    for r in rooms:
        x,y,z=r['p'];w,h,_=r['size']
        for xx in range(int(x-w/2+50),int(x+w/2),150):
            for yy in range(int(y-h/2+50),int(y+h/2),150):
                for start,end,capsule in [((xx,yy,z+80),(xx,yy,z-80),False),((xx,yy,z+100),(xx+100,yy,z+100),True)]:
                    args=(world,V(start),V(end))
                    hit=u.SystemLibrary.capsule_trace_single(*args,34,88,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True) if capsule else u.SystemLibrary.line_trace_single(*args,u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
                    d=hit.to_tuple() if hit else None
                    out.append([r['id'],start,end,capsule,bool(d[0]),round(d[3],3),xyz(d[5])] if d else [r['id'],start,end,capsule,False])
    return out
assert levels.load_level('/Game/Vanthex/Maps/GridV4')
before=signature();before_queries=probes(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world())
assert not u.EditorAssetLibrary.does_asset_exist(DEST),'Candidate already exists; refusing overwrite'
assert levels.new_level_from_template(DEST,'/Game/Vanthex/Experiments/GraphicsLab/Maps/PowerLab_V4_GridV4')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert signature()==before
original=list(actors.get_all_level_actors())
template=next(a for a in original if a.get_actor_label()=='GridV4_SM_ConcreteWall')
groups={};counts={r['id']:{'name':r['name'],'new_instances':0,'new_lights':0} for r in rooms}
paint=u.load_asset(BASE+'/Materials/M_LabPaint');steel=u.load_asset(BASE+'/Materials/M_LabSteel');floor=u.load_asset(BASE+'/Materials/M_LabConcrete')
mats={key:u.load_asset(BASE+'/Materials/M_'+key) for key in ['LabPaint','LabSteel','LabRubber','LabAmber']}
def group(name,override=None):
    key=(name,override.get_path_name() if override else '')
    if key in groups:return groups[key]
    path=BASE+'/Meshes/'+name if name.startswith('SM_Lab') else '/Game/Meshes/'+name
    mesh=u.load_asset(path);assert mesh,path
    a=actors.duplicate_actor(template,world);a.set_actor_label('FacilityStyle_'+name+('_surface' if override else ''));a.set_folder_path('GRID V4 / INDUSTRIAL FINISH / INSTANCES');a.tags=[u.Name('VanthexIndustrialFinish')]
    c=a.get_component_by_class(u.InstancedStaticMeshComponent);c.clear_instances();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    for i,slot in enumerate(mesh.static_materials):
        keymat=str(slot.material_slot_name)
        if override:c.set_material(i,override)
        elif keymat in mats:c.set_material(i,mats[keymat])
    groups[key]=c;return c
def add(name,p,size,rot=None,override=None,count=True):
    c=group(name,override);m=c.static_mesh;b=m.get_bounds();rot=rot or u.Rotator(pitch=0,yaw=0,roll=0)
    scale=V((size[0]/(b.box_extent.x*2),size[1]/(b.box_extent.y*2),size[2]/(b.box_extent.z*2)))
    q=rot.quaternion();offset=u.MathLibrary.quat_rotate_vector(q,V((b.origin.x*scale.x,b.origin.y*scale.y,b.origin.z*scale.z)))
    t=u.Transform();t.translation=V(p)-offset;t.rotation=q;t.scale3d=scale
    c.add_instance(t,world_space=True)
    r=room_for(V(p),20)
    if count and r:counts[r['id']]['new_instances']+=1
    return c
def geom(c,t):
    b=c.static_mesh.get_bounds();s=t.scale3d
    p=u.MathLibrary.transform_location(t,b.origin)
    return p,[b.box_extent.x*s.x*2,b.box_extent.y*s.y*2,b.box_extent.z*s.z*2],t.rotation.rotator()
# Apply the approved matte concrete to existing floor surfaces, preserving holes and ramps.
wall_count=0;roof_cells={};upper_floors=0;boilers=0
for a in original:
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
        if not c.static_mesh:continue
        name=c.static_mesh.get_name();remove=[]
        if name=='SM_ConcreteFloor_01':
            c.set_material(0,floor)
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True);p,size,rot=geom(c,t)
            if protected(p):continue
            if name=='SM_ConcreteWall':
                # Two thin skins remain attached to each original wall face. Original
                # backing geometry and all collision remain, including inclined stair walls.
                n=u.MathLibrary.quat_rotate_vector(t.rotation,V((0,1,0)))
                for side in (-1,1):
                    pos=p+n*(size[1]/2+2.7)*side
                    rotation=u.MathLibrary.compose_rotators(u.Rotator(pitch=0,yaw=180 if side<0 else 0,roll=0),rot)
                    add('SM_LabWallCassette',xyz(pos),(size[0],5.0,size[2]),rotation)
                wall_count+=1
            elif name=='SM_Platform_01' and abs(rot.pitch)<.1 and abs(rot.roll)<.1 and abs(p.z-592)<1:
                add('SM_ConcreteFloor_01',xyz(p),size,rot,floor);remove.append(i);upper_floors+=1
            elif name=='SM_FactoryFloorLarge01' and abs(rot.pitch)<.1 and abs(rot.roll)<.1 and abs(size[0]-100)<.1 and abs(size[1]-100)<.1:
                roof=round(p.z-size[2]/2,2);roof_cells.setdefault(roof,set()).add((round((p.x-50)/100),round((p.y-50)/100)));remove.append(i)
            elif name=='SM_Boiler_01':
                r=room_for(p)
                if r and r['id'] in (11,13):
                    add('SM_LabPowerCabinet',xyz(p),size,u.Rotator(pitch=0,yaw=180,roll=0));remove.append(i);boilers+=1
            elif name=='SM_CeilingVentilation01':
                # Replace stretched 30cm domestic vent grilles with honest ceiling services.
                r=room_for(p)
                if r and r['id'] not in (3,17,18):remove.append(i)
            elif name=='SM_LampSet_01':
                # Correct fixture proportions/underside in corridors as well as rooms.
                r=room_for(p)
                if r and r['id'] not in (3,8,17,18) and p.z-r['p'][2]<450:remove.append(i)
                else:
                    add('SM_LampSet_01',xyz(p),(28,144,12.7),u.Rotator(pitch=0,yaw=rot.yaw,roll=180));remove.append(i)
        for i in reversed(remove):c.remove_instance(i)
        if name=='SM_FactoryFloorLarge01':c.set_material(0,paint)
# Pack only existing ceiling cells. Apertures, stairs and upper open Core spaces stay open.
roof_modules=0
for z,cells in roof_cells.items():
    used=set()
    for x,y in sorted(cells,key=lambda p:(p[1],p[0])):
        if (x,y) in used:continue
        w=1
        while w<3 and (x+w,y) in cells and (x+w,y) not in used:w+=1
        h=1
        while h<3 and all((x+dx,y+h) in cells and (x+dx,y+h) not in used for dx in range(w)):h+=1
        used.update((x+dx,y+dy) for dx in range(w) for dy in range(h))
        add('SM_LabCeilingCassette',((x+w/2)*100,(y+h/2)*100,z-3),(w*100,h*100,6));roof_modules+=1
# Preserve distinctive server/research/freight/Core machinery. Add services above head height.
def ceiling_at(x,y,z):
    h=u.SystemLibrary.line_trace_single(world,V((x,y,z+360)),V((x,y,z+450)),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE,True)
    if not h:return None
    d=h.to_tuple()
    return d[5].z if d[0] and abs(d[5].z-(z+420))<5 else None
new_lights=[]
for r in rooms:
    rid=r['id'];x,y,z=r['p'];w,h,tall=r['size']
    if rid in (3,8,17,18):continue
    # Remove room fills only; corridor/stair lights and all gameplay actors remain.
    for a in original:
        if isinstance(a,u.PointLight):
            p=a.get_actor_location()
            if abs(p.x-x)<w/2 and abs(p.y-y)<h/2 and z+250<p.z<z+350:actors.destroy_actor(a)
    for side in (-1,1):
        yy=y+side*h*.32
        start=x-w/2+30;end=x+w/2-30
        xx=start
        while xx<end-1:
            length=min(200,end-xx);cx=xx+length/2
            if ceiling_at(cx,yy,z) is not None:
                add('SM_LabCableTray',(cx,yy,z+374),(length,36,10))
                if int((xx-start)/200)%2==0:add('SM_LabPilaster',(cx,yy,z+398),(4,4,44))
            xx+=length
    positions=[(x-w*.25,y),(x+w*.25,y)]
    if w*h>=1800000:positions=[(x+sx*w*.25,y+sy*h*.22) for sx in (-1,1) for sy in (-1,1)]
    for xx,yy in positions:
        if ceiling_at(xx,yy,z) is None:continue
        add('SM_LampSet_01',(xx,yy,z+398),(28,144,12.7),u.Rotator(pitch=0,yaw=90,roll=180))
        a=actors.spawn_actor_from_class(u.RectLight,V((xx,yy,z+389)),u.Rotator(pitch=-90,yaw=0,roll=0));a.set_actor_label('Facility_Worklight_%02d'%rid);a.set_folder_path('GRID V4 / INDUSTRIAL FINISH / LIGHTS')
        c=a.get_component_by_class(u.RectLightComponent);c.set_mobility(u.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(180);c.set_editor_property('source_width',120);c.set_editor_property('source_height',25);c.set_editor_property('attenuation_radius',min(1050,max(w,h)*.75));c.set_editor_property('use_temperature',True);c.set_editor_property('temperature',4600 if rid in (2,4,9,19) else 4300);c.set_cast_shadows(True)
        new_lights.append(a);counts[rid]['new_lights']+=1
for c in groups.values():c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
after=signature();after_queries=probes(world)
assert before==after,'Gameplay collision components changed'
assert before_queries==after_queries,'Gameplay collision query changed'
assert levels.save_current_level()
assert source_hash==hashlib.sha256(open(SOURCE,'rb').read()).hexdigest(),'Production changed before approval gate'
report={'candidate':DEST,'original_sha256':source_hash,'collision_components':len(before),'collision_queries':len(before_queries),'signatures_equal':before==after,'queries_equal':before_queries==after_queries,'original_walls_clad':wall_count,'ceiling_modules':roof_modules,'upper_floor_tiles':upper_floors,'boilers_replaced':boilers,'new_instanced_groups':len(groups),'new_visual_instances':sum(c.get_instance_count() for c in groups.values()),'new_lights':len(new_lights),'room_details':counts}
with open(OUT+'/generation.json','w') as f:json.dump(report,f,indent=2)
with open(OUT+'/collision_reference.json','w') as f:json.dump({'components':before,'queries':before_queries},f)
u.log('FACILITY STYLE COMPLETE '+json.dumps(report))
