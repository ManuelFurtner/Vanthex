"""Import original Roman art into a separate baked Crossroads CTF preview."""

import hashlib
import json
from pathlib import Path
import re

import unreal as u


SOURCE = "/Game/Vanthex/Maps/CrossroadsCTF"
LAB = "/Game/Vanthex/Experiments/GraphicsLab"
DESTINATION = LAB + "/Maps/RomanPilotCrossroadsCTF"
ASSETS = "/Game/Vanthex/Art/CrossroadsRoman"
TAG = u.Name("VanthexRomanArtPilot")
OUTPUT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "CTFArtPilot"
CONTENT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_content_dir()))
SOURCE_FILE = CONTENT / (SOURCE.removeprefix("/Game/") + ".umap")
PREVIEW_FILE = CONTENT / (DESTINATION.removeprefix("/Game/") + ".umap")
REPORT = OUTPUT / "unreal_preview.json"
EXPECTED_SIZE = {
    "SM_CTFRomanFacade": ((372, 398), (9, 27), (225, 250)),
    "SM_CTFRomanBanner_Alpha": ((70, 116), (7, 26), (199, 245)),
    "SM_CTFRomanBanner_Bravo": ((70, 116), (7, 26), (199, 245)),
}
FINIAL = "SM_CTFVictoryFinial"
LOCAL_ONLY_PREFIXES = (LAB + "/", "/Game/Vanthex/Art/IndustrialFinish/",
                       "/Game/Vanthex/Imported/", "/Game/Vanthex/Materials/Aegis/",
                       "/Game/FPS_Weapon_Bundle/", "/Game/FreeWeaponSFX/")

levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
assets = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()


def xyz(vector):
    return tuple(round(value, 4) for value in (vector.x, vector.y, vector.z))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pilot_art_hashes():
    folder = CONTENT / ASSETS.removeprefix("/Game/")
    return {path.relative_to(CONTENT).as_posix(): digest(path)
            for path in sorted(folder.rglob("*.uasset"))}


def assert_portable(path, own_package=None):
    data = path.read_bytes()
    for encoding in ("ascii", "utf-16-le"):
        package_names = data.replace(own_package.encode(encoding), b"") if own_package else data
        for prefix in LOCAL_ONLY_PREFIXES:
            assert prefix.encode(encoding) not in package_names, f"{path.name} references {prefix}"


def pose(transform):
    rot = transform.rotation.rotator()
    return (xyz(transform.translation), xyz(transform.scale3d),
            tuple(round(value, 4) for value in (rot.pitch, rot.yaw, rot.roll)))


def collision_signature():
    rows = []
    for actor in actors.get_all_level_actors():
        for component in actor.get_components_by_class(u.PrimitiveComponent):
            collision = component.get_collision_enabled()
            if collision == u.CollisionEnabled.NO_COLLISION:
                continue
            instances = ()
            if isinstance(component, u.InstancedStaticMeshComponent):
                instances = tuple(pose(component.get_instance_transform(index, world_space=True))
                                  for index in range(component.get_instance_count()))
            mesh = component.static_mesh if isinstance(component, u.StaticMeshComponent) else None
            rows.append((actor.get_actor_label(), component.get_name(), component.get_class().get_name(),
                         str(collision), str(component.get_collision_profile_name()),
                         str(component.get_editor_property("component_tags")), pose(component.get_world_transform()),
                         mesh.get_path_name() if mesh else "", instances))
    return sorted(rows)


def collision_probes():
    world = editor.get_editor_world()
    results = []
    for x in range(-2400, 2401, 300):
        for y in range(-4200, 4201, 300):
            point = u.Vector(x, y, 100)
            hit = u.SystemLibrary.line_trace_single(
                world, point, u.Vector(x, y, -70), u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                False, [], u.DrawDebugTrace.NONE, True)
            slide = u.SystemLibrary.capsule_trace_single(
                world, point, u.Vector(x + 100, y, 100), 34, 88, u.TraceTypeQuery.TRACE_TYPE_QUERY1,
                False, [], u.DrawDebugTrace.NONE, True)
            for trace in (hit, slide):
                row = trace.to_tuple() if trace else None
                # A penetrating sweep has no unique impact point. Compare its blocked distance.
                results.append((bool(row[0]), bool(row[1]), round(row[3], 3)) if row else None)
    return results


def compare_probes(source, preview):
    def hits(results):
        return sum(bool(result and result[0]) for result in results)

    source_hits, preview_hits = hits(source), hits(preview)
    if source_hits:
        assert source == preview, "Crossroads traces differed between comparable editor worlds"
    return {"samples": len(source), "source_blocking": source_hits, "preview_blocking": preview_hits,
            "result": "equal" if source_hits else "source_scene_unavailable"}


def make_material(slot, parameters):
    name = "M_" + slot
    folder = ASSETS + "/Materials"
    path = folder + "/" + name
    material = u.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, folder, u.Material, u.MaterialFactoryNew())
    assert material, path
    nodes = u.MaterialEditingLibrary
    nodes.delete_all_material_expressions(material)
    color = nodes.create_material_expression(material, u.MaterialExpressionConstant3Vector)
    color.constant = u.LinearColor(*parameters["base_color"], 1.0)
    nodes.connect_material_property(color, "", u.MaterialProperty.MP_BASE_COLOR)
    for key, prop in (("metallic", u.MaterialProperty.MP_METALLIC),
                      ("roughness", u.MaterialProperty.MP_ROUGHNESS)):
        value = nodes.create_material_expression(material, u.MaterialExpressionConstant)
        value.r = parameters[key]
        nodes.connect_material_property(value, "", prop)
    strength = parameters["emissive_strength"]
    if strength > 0:
        glow = nodes.create_material_expression(material, u.MaterialExpressionConstant3Vector)
        glow.constant = u.LinearColor(*(channel * strength for channel in parameters["emissive_color"]), 1.0)
        nodes.connect_material_property(glow, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    nodes.recompile_material(material)
    assert assets.save_loaded_asset(material), path
    return material


def import_mesh(name, materials, size_limits):
    folder = ASSETS + "/Meshes"
    task = u.AssetImportTask()
    task.filename = str(OUTPUT / (name + ".fbx"))
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    options = u.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    task.options = options
    tools.import_asset_tasks([task])
    mesh = next((obj for obj in task.get_objects() if isinstance(obj, u.StaticMesh)), None)
    assert mesh and mesh.get_path_name() == f"{folder}/{name}.{name}", f"FBX import failed for {name}"
    info = mesh_info(mesh)
    assert all(lo <= length <= hi for length, (lo, hi) in zip(info["size_cm"], size_limits)), (name, info)
    slots = info["slots"]
    assert slots and len(set(slots)) == len(slots) and set(slots) <= materials.keys(), (name, slots)
    for index, slot in enumerate(slots):
        mesh.set_material(index, materials[slot])
    assert assets.save_loaded_asset(mesh), name
    return mesh, info


def mesh_info(mesh):
    bounds = mesh.get_bounds()
    return {"size_cm": xyz(bounds.box_extent * 2.0),
            "slots": [str(slot.material_slot_name) for slot in mesh.static_materials],
            "min_cm": xyz(bounds.origin - bounds.box_extent),
            "max_cm": xyz(bounds.origin + bounds.box_extent)}


def add_prop(label, mesh, position, yaw=0.0, scale=(1, 1, 1)):
    actor = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(*position), u.Rotator(yaw=yaw))
    assert actor, label
    actor.set_actor_label(label)
    actor.set_folder_path("CROSSROADS / ROMAN ART PILOT")
    actor.tags = [TAG]
    actor.set_actor_scale3d(u.Vector(*scale))
    actor.set_actor_enable_collision(False)
    component = actor.static_mesh_component
    component.set_collision_profile_name("NoCollision")
    component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_static_mesh(mesh)
    component.set_editor_property("cast_shadow", True)


def mount_offsets(bounds):
    """FBX can flip Blender's Y axis. Find the thin back edge in local space."""
    min_y, max_y = bounds["min_cm"][1], bounds["max_cm"][1]
    front_is_negative_y = abs(min_y) > abs(max_y)
    back_offset = max_y if front_is_negative_y else -min_y
    assert abs(back_offset) < 5.0, bounds
    return back_offset, 0.0 if front_is_negative_y else 180.0


def build_preview():
    source_hash = digest(SOURCE_FILE)
    assert SOURCE_FILE.stat().st_size > 1000, "Hydrate CrossroadsCTF.umap via Git LFS before importing"
    if REPORT.exists() and assets.does_asset_exist(DESTINATION):
        previous = json.loads(REPORT.read_text(encoding="utf-8"))
        assert previous["source_sha256"] == source_hash, "Pilot was copied from a different Crossroads revision"
    palette = json.loads((OUTPUT / "palette.json").read_text(encoding="utf-8"))["materials"]
    for name in EXPECTED_SIZE:
        assert (OUTPUT / (name + ".fbx")).is_file(), f"Missing Blender FBX for {name}"
    mesh_sizes = EXPECTED_SIZE.copy()
    if (OUTPUT / (FINIAL + ".fbx")).is_file():
        mesh_sizes[FINIAL] = ((18, 92), (18, 92), (48, 92))

    assert levels.load_level(SOURCE), SOURCE
    before = collision_signature()
    assert before, "Source collision components missing"
    if assets.does_asset_exist(DESTINATION):
        assert levels.load_level(DESTINATION), DESTINATION
    else:
        assert levels.new_level_from_template(DESTINATION, SOURCE), DESTINATION
    assert editor.get_editor_world().get_path_name().split(".")[0] == DESTINATION
    assert collision_signature() == before, "Candidate collision differs from Crossroads before placement"

    materials = {slot: make_material(slot, value) for slot, value in palette.items()}
    meshes = {name: import_mesh(name, materials, limits) for name, limits in mesh_sizes.items()}
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            assert actors.destroy_actor(actor), actor.get_actor_label()

    # Art stays over continuous masonry from VanthexCTFMap.cpp. The inner
    # faces at X=±1100 flank the courtyard and stop well before the routes.
    facade, facade_bounds = meshes["SM_CTFRomanFacade"]
    back, flip = mount_offsets(facade_bounds)
    facade_face = 1098.0 - back
    for end, y in (("South", -1450), ("North", 1450)):
        add_prop(f"RomanPilot_{end}WestRelief", facade, (-facade_face, y, 180), 90.0 + flip)
        add_prop(f"RomanPilot_{end}EastRelief", facade, (facade_face, y, 180), -90.0 + flip)

    # Use the same relief on the two 500 × 210 cm solid forum walls. A
    # reduced depth and height keep the profiles within the original cover.
    low_scale = (1, 0.65, 0.69)
    low_back = back * low_scale[1]
    for end, side in (("South", -1), ("North", 1)):
        inward_yaw = 180.0 if side < 0 else 0.0
        add_prop(f"RomanPilot_{end}ForumInside", facade, (0, side * (573 - low_back), 108),
                 inward_yaw + flip, low_scale)
        add_prop(f"RomanPilot_{end}ForumOutside", facade, (0, side * (727 + low_back), 108),
                 inward_yaw + flip + 180.0, low_scale)

    # Heraldry is fixed to the rear masonry beside each existing portico.
    # Z is the bottom of the wall-hung standard, above spawn-height sightlines.
    for team, side, yaw in (("Alpha", -1, 180.0), ("Bravo", 1, 0.0)):
        banner, bounds = meshes[f"SM_CTFRomanBanner_{team}"]
        back, flip = mount_offsets(bounds)
        assert abs(bounds["min_cm"][2]) < 3.0, bounds
        wall_face = side * (4357.5 - back)
        for flank, x in (("West", -430), ("East", 430)):
            add_prop(f"RomanPilot_{team}{flank}Standard", banner, (x, wall_face, 225), yaw + flip)

    finial = meshes.get(FINIAL)
    original_orbs = [actor for actor in actors.get_all_level_actors()
                     if actor.get_actor_label().startswith("Roman_VictoryOrb_")]
    if finial:
        assert len(original_orbs) == 1, "Cannot identify the original Victory Orb"
        mesh, bounds = finial
        assert all(-46 <= p <= 46 for p in bounds["min_cm"] + bounds["max_cm"]), bounds
        add_prop("RomanPilot_VictoryFinial", mesh, (0, 0, 790))
    for actor in original_orbs:
        component = actor.static_mesh_component
        assert component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
        component.set_visibility(finial is None)

    assert collision_signature() == before, "Pilot changed collision before saving"
    assert levels.save_current_level(), DESTINATION
    verify_preview(source_hash, before)


def verify_preview(source_hash, before):
    assert levels.load_level(SOURCE), SOURCE
    probes_before = collision_probes()
    assert levels.load_level(DESTINATION), DESTINATION
    assert collision_signature() == before, "Saved pilot collision differs from Crossroads"
    probes = compare_probes(probes_before, collision_probes())
    props = [actor for actor in actors.get_all_level_actors() if actor.actor_has_tag(TAG)]
    names = sorted(actor.get_actor_label() for actor in props)
    assert len(props) == 12 + ("RomanPilot_VictoryFinial" in names), "Pilot props missing after reload"
    meshes = {}
    for actor in props:
        component = actor.static_mesh_component
        mesh = component.static_mesh
        assert mesh and mesh.get_path_name().startswith(ASSETS + "/Meshes/")
        assert component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
        assert not component.get_editor_property("can_ever_affect_navigation")
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            assert material and material.get_path_name().startswith(ASSETS + "/Materials/")
        meshes[mesh.get_name()] = mesh_info(mesh)
    assert set(EXPECTED_SIZE) <= meshes.keys()
    assert_portable(PREVIEW_FILE, DESTINATION)
    art_hashes = pilot_art_hashes()
    for path in art_hashes:
        assert_portable(CONTENT / path)
    source_after = digest(SOURCE_FILE)
    assert source_hash == source_after, "Production Crossroads was modified"
    report = {"result": "PASS", "map": DESTINATION, "source_sha256": source_hash,
              "source_sha256_after": source_after, "preview_sha256": digest(PREVIEW_FILE),
              "art_sha256": art_hashes, "collision_components": len(before),
              "editor_probes": probes, "collision_equal": True, "props": names, "meshes": meshes}
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    u.log(f"CTF ART PILOT PASS {REPORT}")


def pilot_placements():
    placements = {}
    for actor in actors.get_all_level_actors():
        if not actor.actor_has_tag(TAG):
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh.get_path_name()
        assert mesh.startswith(ASSETS + "/Meshes/")
        assert component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
        assert not component.get_editor_property("can_ever_affect_navigation")
        location, scale, (pitch, yaw, roll) = pose(actor.get_actor_transform())
        assert pitch == roll == 0.0
        label = actor.get_actor_label()
        assert label not in placements, label
        placements[label] = (mesh, location, yaw, scale)
    return placements


def victory_orb():
    matches = [actor for actor in actors.get_all_level_actors()
               if actor.get_actor_label().startswith("Roman_VictoryOrb_")]
    assert len(matches) == 1, "Cannot identify the original Victory Orb"
    component = matches[0].static_mesh_component
    assert component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
    return component


def apply_preview():
    """Copy verified art placements into the original baked level."""
    preview = json.loads(REPORT.read_text(encoding="utf-8"))
    assert preview["result"] == "PASS" and preview["map"] == DESTINATION
    source_hash = digest(SOURCE_FILE)
    assert source_hash == preview["source_sha256"], "Crossroads changed since the preview was built"
    assert digest(PREVIEW_FILE) == preview["preview_sha256"], "Preview map changed after verification"
    assert preview["art_sha256"] and pilot_art_hashes() == preview["art_sha256"], "Pilot art assets changed"
    match = re.search(r'(?:^|\s)-CTFArtGameplayReport=(?:"([^"]+)"|(\S+))',
                      u.SystemLibrary.get_command_line(), re.IGNORECASE)
    assert match, "Pass -CTFArtGameplayReport with the isolated preview CTFTest.txt path"
    gameplay = Path(match[1] or match[2])
    assert gameplay.name == "CTFTest.txt" and gameplay.is_file(), gameplay
    run_log = gameplay.parent.parent / "Unreal.log"
    newest_preview_file = max(PREVIEW_FILE.stat().st_mtime,
                              *(CONTENT.joinpath(path).stat().st_mtime for path in preview["art_sha256"]))
    assert gameplay.stat().st_mtime >= newest_preview_file and run_log.stat().st_mtime >= newest_preview_file
    assert gameplay.read_text(encoding="utf-8-sig").startswith("OVERALL PASS "), "CTF gameplay test failed"
    assert f"Load map complete {DESTINATION}" in run_log.read_text(encoding="utf-8-sig", errors="replace"), run_log

    assert levels.load_level(SOURCE), SOURCE
    before = collision_signature()
    probes = collision_probes()
    assert before and levels.load_level(DESTINATION), DESTINATION
    assert collision_signature() == before, "Preview collision changed"
    compare_probes(probes, collision_probes())
    placements = pilot_placements()
    assert sorted(placements) == preview["props"], "Preview placements differ from verified report"
    orb_visible = victory_orb().get_editor_property("visible")

    assert levels.load_level(SOURCE), SOURCE
    assert editor.get_editor_world().get_path_name().split(".")[0] == SOURCE
    assert not pilot_placements(), "Crossroads already contains pilot art"
    for label, (mesh_path, location, yaw, scale) in placements.items():
        mesh = u.load_asset(mesh_path)
        assert mesh, mesh_path
        add_prop(label, mesh, location, yaw, scale)
    victory_orb().set_visibility(orb_visible)
    assert pilot_placements() == placements, "Copied placements differ from verified preview"
    assert collision_signature() == before, "Copied pilot art changed Crossroads collision"

    backup = OUTPUT / f"CrossroadsCTF-before-{source_hash[:12]}.umap"
    original = SOURCE_FILE.read_bytes()
    assert hashlib.sha256(original).hexdigest() == source_hash
    if not backup.exists():
        backup.write_bytes(original)
    assert digest(backup) == source_hash, "Crossroads backup is invalid"

    try:
        assert levels.save_current_level(), SOURCE
        saved_hash = digest(SOURCE_FILE)
        assert saved_hash != source_hash, "Saving the placements did not update Crossroads"
        assert_portable(SOURCE_FILE)
        assert levels.load_level(DESTINATION) and levels.load_level(SOURCE), "Could not reopen promoted Crossroads"
        assert collision_signature() == before, "Promoted map collision changed"
        compare_probes(probes, collision_probes())
        assert pilot_placements() == placements, "Promoted Crossroads changed verified placements"
        assert victory_orb().get_editor_property("visible") == orb_visible, "Victory Orb visibility changed"
    except Exception:
        if not SOURCE_FILE.is_file() or digest(SOURCE_FILE) != source_hash:
            SOURCE_FILE.write_bytes(original)
        raise

    applied_report = OUTPUT / "unreal_applied.json"
    applied_report.write_text(json.dumps({"result": "PASS", "map": SOURCE,
        "source_sha256": source_hash, "applied_sha256": saved_hash, "backup": str(backup),
        "collision_equal": True, "props": sorted(placements)}, indent=2) + "\n", encoding="utf-8")
    u.log(f"CTF ART PILOT APPLIED {applied_report}")


switches = u.SystemLibrary.get_command_line().lower().split()
if "-ctfartapply" in switches:
    apply_preview()
elif "-ctfartverify" in switches:
    assert levels.load_level(SOURCE), SOURCE
    verify_preview(digest(SOURCE_FILE), collision_signature())
else:
    build_preview()
