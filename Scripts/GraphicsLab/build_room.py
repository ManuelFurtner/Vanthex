"""Deterministic visual-only GRID V4 experiment; never saves production packages."""
import unreal as u
import os, json, math, hashlib, re
ROOT='H:/Vanthex'
OUT=ROOT+'/Saved/GraphicsLab'
BASE='/Game/Vanthex/Experiments/GraphicsLab'
revision=int(re.search(r'GraphicsLabRevision=(\d+)',u.SystemLibrary.get_command_line()).group(1))
DEST=BASE+'/Maps/PowerLab_V%d_GridV4'%revision
if revision>=2: BASE+='/Revision%d'%revision
SOURCE=ROOT+'/Content/Vanthex/Maps/GridV4.umap'
source_hash=hashlib.sha256(open(SOURCE,'rb').read()).hexdigest()
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
def v(p): return u.Vector(*p)
def xyz(p):return [round(p.x,4),round(p.y,4),round(p.z,4)]
def transform(t):
    r=t.rotation.rotator()
    return [xyz(t.translation),xyz(t.scale3d),[round(r.pitch,4),round(r.yaw,4),round(r.roll,4)]]
def signature():
    rows=[]
    for a in actors.get_all_level_actors():
        for c in a.get_components_by_class(u.PrimitiveComponent):
            if c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION:continue
            rows.append([a.get_actor_label(),c.get_name(),c.get_class().get_name(),str(c.get_collision_enabled()),str(c.get_collision_profile_name()),transform(c.get_world_transform()),str(c.get_editor_property('component_tags'))])
    return sorted(rows,key=lambda x:(x[0],x[1]))
assert levels.load_level('/Game/Vanthex/Maps/GridV4')
before=signature()
# UE's template operation duplicates the existing map under a new package.
assert not u.EditorAssetLibrary.does_asset_exist(DEST), 'Refusing to overwrite an existing experiment revision'
assert levels.new_level_from_template(DEST,'/Game/Vanthex/Maps/GridV4')
assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(DEST)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
def probes():
    results=[]
    # Dense floor rays and horizontal visibility probes in/around the 18x12m room.
    rays=[]
    for x in range(2150,3900,100):
        for y in range(2450,3600,100): rays.append(((x,y,80),(x,y,-80)))
    for z in (70,165,290):
        for x in range(2150,3900,100): rays.append(((x,2350,z),(x,3650,z)))
        for y in range(2450,3600,100): rays.append(((2050,y,z),(3950,y,z)))
    for start,end in rays:
        hit=u.SystemLibrary.line_trace_single(world,v(start),v(end),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,True)
        if hit:
            d=hit.to_tuple()
            results.append([start,end,bool(d[0]),round(float(d[3]),3),xyz(d[5])])
        else:results.append([start,end,False])
    return results
probe_before=probes()
# Load/import only original generated mesh modules into the experiment folder.
for name in ['SM_LabPilaster','SM_LabWallCassette','SM_LabCableTray','SM_LabCeilingCassette','SM_LabPowerCabinet']:
    path=BASE+'/Meshes/'+name
    if not u.EditorAssetLibrary.does_asset_exist(path):
        task=u.AssetImportTask();task.filename=OUT+'/Modules/'+name+'.fbx';task.destination_path=BASE+'/Meshes';task.automated=True;task.save=True;task.replace_existing=False
        options=u.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
        options.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
        options.static_mesh_import_data.set_editor_property('combine_meshes',True)
        task.options=options
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert u.load_asset(path), path
# Physically based, non-emissive metal/paint materials for original modules.
def material(name,color,metallic,roughness,emissive=0):
    path=BASE+'/Materials/'+name
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(m)
    col=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector)
    col.constant=u.LinearColor(*color,1)
    u.MaterialEditingLibrary.connect_material_property(col,'',u.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(u.MaterialProperty.MP_METALLIC,metallic),(u.MaterialProperty.MP_ROUGHNESS,roughness)]:
        c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant);c.r=value;u.MaterialEditingLibrary.connect_material_property(c,'',prop)
    if revision>=2 and name in ('M_LabPaint','M_LabSteel'):
        tex=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample)
        tex.texture=u.load_asset('/Game/Textures/T_MetalDentsDetail01_N');tex.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL
        uv=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureCoordinate);uv.u_tiling=3;uv.v_tiling=3
        u.MaterialEditingLibrary.connect_material_expressions(uv,'',tex,'UVs')
        mul=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionMultiply);mul.set_editor_property('const_b',.12)
        u.MaterialEditingLibrary.connect_material_expressions(tex,'RGB',mul,'A')
        flat=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector);flat.constant=u.LinearColor(0,0,.88,1)
        add=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionAdd)
        u.MaterialEditingLibrary.connect_material_expressions(mul,'',add,'A');u.MaterialEditingLibrary.connect_material_expressions(flat,'',add,'B')
        u.MaterialEditingLibrary.connect_material_property(add,'',u.MaterialProperty.MP_NORMAL)
    if emissive:
        e=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector);e.constant=u.LinearColor(*(c*emissive for c in color),1);u.MaterialEditingLibrary.connect_material_property(e,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m)
    return m
mats={
 'LabSteel':material('M_LabSteel',(.065,.083,.095),.82,.39),
 'LabPaint':material('M_LabPaint',(.21,.26,.26),.35,.56),
 'LabRubber':material('M_LabRubber',(.013,.017,.02),0,.88),
 'LabAmber':material('M_LabAmber',(.65,.3,.035),.15,.58),
 'Light':material('M_LabPractical',(.75,.84,1),0,.4,3),
}
floor_material=None
if revision>=2:
    floor_material=material('M_LabConcrete',(.2,.2,.2),0,.8)
    for texture,sampler,prop,output in [('T_FactoryConcreteFloor01_BC',u.MaterialSamplerType.SAMPLERTYPE_COLOR,u.MaterialProperty.MP_BASE_COLOR,'RGB'),('T_FactoryConcreteFloor01_N',u.MaterialSamplerType.SAMPLERTYPE_NORMAL,u.MaterialProperty.MP_NORMAL,'RGB'),('T_FactoryConcreteFloor01_ORM',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,u.MaterialProperty.MP_ROUGHNESS,'G')]:
        t=u.MaterialEditingLibrary.create_material_expression(floor_material,u.MaterialExpressionTextureSample);t.texture=u.load_asset('/Game/Textures/'+texture);t.sampler_type=sampler
        assert t.texture,texture
        u.MaterialEditingLibrary.connect_material_property(t,output,prop)
    u.MaterialEditingLibrary.recompile_material(floor_material);u.EditorAssetLibrary.save_loaded_asset(floor_material)
created=[]
def mesh(name,p,scale=1,yaw=0,center=True,label=None,material_override=None):
    path=BASE+'/Meshes/'+name if name.startswith('SM_Lab') else '/Game/Meshes/'+name
    m=u.load_asset(path);assert m,path
    a=actors.spawn_actor_from_class(u.StaticMeshActor,v(p),u.Rotator(pitch=0,yaw=yaw,roll=0));a.set_actor_label('GraphicsLab_'+(label or name));a.set_folder_path('GRAPHICS LAB / POWER / V%d'%revision)
    c=a.static_mesh_component;c.set_static_mesh(m);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);c.set_can_ever_affect_navigation(False) if hasattr(c,'set_can_ever_affect_navigation') else None
    c.set_editor_property('cast_shadow',True)
    a.set_actor_scale3d(v((scale,)*3 if isinstance(scale,(float,int)) else scale))
    if center:
        b=m.get_bounds();s=a.get_actor_scale3d();off=v((b.origin.x*s.x,b.origin.y*s.y,b.origin.z*s.z));off=u.MathLibrary.rotate_angle_axis(off,yaw,v((0,0,1)))
        a.set_actor_location(v(p)-off,False,False)
    if name.startswith('SM_Lab'):
        for i,slot in enumerate(m.static_materials):
            key=str(slot.material_slot_name)
            if key in mats:c.set_material(i,mats[key])
    if material_override:
        for i in range(c.get_num_materials()):c.set_material(i,material_override)
    created.append(a)
    return a
def fit(name,p,size,yaw=0,**kw):
    m=u.load_asset((BASE+'/Meshes/' if name.startswith('SM_Lab') else '/Game/Meshes/')+name);e=m.get_bounds().box_extent
    return mesh(name,p,(size[0]/(e.x*2),size[1]/(e.y*2),size[2]/(e.z*2)),yaw,**kw)
def inside(p):return 2100<=p.x<=3900 and 2400<=p.y<=3600 and -20<=p.z<=440
removed={}
# Replace selected local ISM instances only; all blocking collision is separate.
replace_names=['SM_Boiler_01','SM_LampSet_01','SM_CeilingVentilation01','SM_Pipe_round_long']
for a in actors.get_all_level_actors():
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
        if not c.static_mesh or c.static_mesh.get_name() not in replace_names:continue
        assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
        ids=[i for i in range(c.get_instance_count()) if inside(c.get_instance_transform(i,world_space=True).translation)]
        for i in reversed(ids):c.remove_instance(i)
        if ids:removed[a.get_actor_label()]=len(ids)
    if isinstance(a,u.PointLight) and inside(a.get_actor_location()):actors.destroy_actor(a)
# Six native-scale textured concrete floor tiles, 2mm above the old visual surface.
for x in (2400,3000,3600):
    for y in (2700,3300):mesh('SM_ConcreteFloor_01',(x,y,.2),1,center=False,material_override=floor_material)
# Modular ceiling and wall skins. New parts stay inside existing structural surfaces.
for x in range(2250,3900,300):
    for y in range(2550,3600,300):fit('SM_LabCeilingCassette',(x,y,417),(300,300,6))
for x in range(2250,3900,300):
    for z in (100,300):fit('SM_LabWallCassette',(x,3585,z),(298,5,198))
for x in (2105,2400,2700,3000,3300,3600,3895):fit('SM_LabPilaster',(x,3584,210),(16,8,420))
# Add equally thin panels only at existing solid walls; leave all door openings clear.
for x,width in ((2600,396),(3550,696)):
    for z in (100,300):fit('SM_LabWallCassette',(x,2415,z),(width,5,198),180)
for y in (3300,3500):
    for z in (100,300):fit('SM_LabWallCassette',(2115,y,z),(198,5,198),90)
for y in (2500,2700,3300,3500):
    for z in (100,300):fit('SM_LabWallCassette',(3885,y,z),(198,5,198),-90)
# Matched envelopes for the three existing full-height cover blocks.
for i,(x,y,yaw) in enumerate(((2270,3420,0),(3720,3420,0),(3720,2580,180))):
    fit('SM_LabPowerCabinet',(x,y,150),(250,210,300),yaw+(180 if revision>=2 else 0))
    # Native-scale engine assembly grounded on the enclosure roof, below ceiling.
    engine=u.load_asset('/Game/Meshes/SM_EngineModifyPivot');b=engine.get_bounds();s=.9
    mesh('SM_EngineModifyPivot',(x,y,300+b.box_extent.z*s),s,yaw)
    # Front instrumentation uses an authentic textured control box.
    front=-1 if yaw==0 else 1
    mesh('SM_ElectricalPanel_02',(x,y+front*106,222),.48,yaw)
    # Visible power feed continuing upwards from each unit.
    for dx in (-70,70):
        if revision>=2:
            a=fit('SM_Pipe_round_long',(x+dx,y,350),(10,100,10),0)
            # Rotate about the requested geometric center, not the mesh pivot.
            a.set_actor_rotation(u.Rotator(pitch=0,yaw=0,roll=90),False)
            e=a.static_mesh_component.static_mesh.get_bounds().origin;s=a.get_actor_scale3d()
            off=u.MathLibrary.rotate_angle_axis(v((e.x*s.x,e.y*s.y,e.z*s.z)),90,v((1,0,0)))
            a.set_actor_location(v((x+dx,y,350))-off,False,False)
        else:fit('SM_Pipe_round_long',(x+dx,y,357),(10,100,10),0)
# Ceiling services: modular cable ladder and parallel pipe runs.
for y in (2600,3425):
    for x in range(2200,3900,200):fit('SM_LabCableTray',(x,y,374),(200,36,10))
    for x in (2200,2600,3000,3400,3800):
        fit('SM_LabPilaster',(x,y,398),(4,4,44))
for x in (2180,3830):
    for y in (2500,2700,2900,3100,3300,3500):
        mesh('SM_Pipe_round_long',(x,y,342),1 if revision>=4 else .6,center=True)
    if revision>=4:
        for y in (2600,3000,3400):fit('SM_LabPilaster',(x,y,390),(4,4,54))
# Four light fittings at honest proportions; rectangular sources face down.
for x in (2550,3350):
    for y in (2700,3300):
        fixture=mesh('SM_LampSet_01',(x,y,398),1,90)
        if revision>=4:
            fixture.set_actor_rotation(u.Rotator(pitch=0,yaw=90,roll=180),False)
            fixture.set_actor_location(v((x,y,398+fixture.static_mesh_component.static_mesh.get_bounds().origin.z)),False,False)
        a=actors.spawn_actor_from_class(u.RectLight,v((x,y,389)),u.Rotator(pitch=-90,yaw=0,roll=0));a.set_actor_label('GraphicsLab_Worklight');a.set_folder_path('GRAPHICS LAB / LIGHTS')
        c=a.get_component_by_class(u.RectLightComponent);c.set_mobility(u.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(180 if revision>=3 else 3200);c.set_editor_property('source_width',120);c.set_editor_property('source_height',25);c.set_editor_property('attenuation_radius',1050);c.set_editor_property('use_temperature',True);c.set_editor_property('temperature',4300);c.set_cast_shadows(True)
        created.append(a)
# Low-power service light washes the cabinets; no unmotivated room-sized ambient lamp.
for x,y in ((2270,3420),(3720,3420),(3720,2580)):
    a=actors.spawn_actor_from_class(u.PointLight,v((x,y-150 if y>3000 else y+150,290)));a.set_actor_label('GraphicsLab_ServiceBounce');a.set_folder_path('GRAPHICS LAB / LIGHTS')
    c=a.point_light_component;c.set_mobility(u.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(12 if revision>=3 else 450);c.set_editor_property('attenuation_radius',360);c.set_light_color(u.LinearColor(.75,.85,1,1));c.set_cast_shadows(False);created.append(a)
if revision>=4:
    # High-level wall controls remain above standing capsule height.
    for x in (3050,3370):mesh('SM_ElectricalPanel_01',(x,3570,225),.45,0)
    for i,(x,y) in enumerate(((2270,3420),(3720,3420),(3720,2580))):
        a=actors.spawn_actor_from_class(u.TextRenderActor,v((x,y-110 if y>3000 else y+110,270)),u.Rotator(pitch=0,yaw=-90 if y>3000 else 90,roll=0))
        a.set_actor_label('GraphicsLab_UnitIdentifier');a.set_folder_path('GRAPHICS LAB / LABELS')
        c=a.get_component_by_class(u.TextRenderComponent);c.set_text('PWR / %02d'%(i+1));c.set_world_size(14);c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);c.set_text_render_color(u.Color(215,225,220,255));c.set_collision_profile_name('NoCollision');created.append(a)
    # Flush painted perimeter marks identify existing cover; they add no collision.
    for x,y in ((2270,3420),(3720,3420),(3720,2580)):
        for side in (-1,1):
            a=actors.spawn_actor_from_class(u.StaticMeshActor,v((x+side*140,y,.35)))
            a.set_actor_label('GraphicsLab_FloorSafetyPaint');a.set_folder_path('GRAPHICS LAB / MARKINGS')
            c=a.static_mesh_component;c.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,mats['LabAmber']);c.set_collision_profile_name('NoCollision');c.set_cast_shadow(False)
            a.set_actor_scale3d(v((.04,2.35,.002)));created.append(a)
if revision==2:
    # The inherited map clamps exposure near EV1, unsuitable for lumen-calibrated lights.
    # Change only this cloned map's postprocess; production/config remain untouched.
    for a in actors.get_all_level_actors():
        if isinstance(a,u.PostProcessVolume):
            settings=a.get_editor_property('settings')
            settings.set_editor_property('override_auto_exposure_min_brightness',True)
            settings.set_editor_property('override_auto_exposure_max_brightness',True)
            settings.set_editor_property('auto_exposure_min_brightness',4.8)
            settings.set_editor_property('auto_exposure_max_brightness',4.8)
            a.set_editor_property('settings',settings)
for a in created:
    for c in a.get_components_by_class(u.StaticMeshComponent):
        c.set_collision_profile_name('NoCollision');c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
after=signature()
probe_after=probes()
if before!=after:
    with open(OUT+'/collision_diff_v%d.json'%revision,'w') as f:json.dump({'before':before,'after':after},f,indent=2)
assert before==after,'Collision components changed'
assert probe_before==probe_after,'Collision query results changed'
assert levels.save_current_level()
assert hashlib.sha256(open(SOURCE,'rb').read()).hexdigest()==source_hash,'Production map modified'
report={'revision':revision,'map':DEST,'production_sha256':source_hash,'collision_components':len(before),'collision_signature_equal':before==after,'collision_rays':len(probe_before),'collision_rays_equal':probe_before==probe_after,'new_actors':len(created),'removed_visual_instances':removed,'new_mesh_bounds':{n:xyz(u.load_asset(BASE+'/Meshes/'+n).get_bounds().box_extent) for n in ['SM_LabWallCassette','SM_LabPowerCabinet']}}
with open(OUT+'/validation_v%d.json'%revision,'w') as f:json.dump(report,f,indent=2)
with open(OUT+'/collision_v%d.json'%revision,'w') as f:json.dump({'components':before,'queries':probe_after},f)
u.log('GRAPHICSLAB BUILD COMPLETE '+json.dumps(report))
