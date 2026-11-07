"""Generate original modular Roman ornaments for the Crossroads CTF art pilot.

Run with Blender 5.2 in background mode. All dimensions below are metres and
all exported meshes keep their attachment point at the origin, facing -Y.
"""

import json
import math
import random
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved" / "CTFArtPilot"


def material_spec(base, metallic, roughness):
    return {
        "base_color": base,
        "metallic": metallic,
        "roughness": roughness,
        "emissive_color": (0.0, 0.0, 0.0),
        "emissive_strength": 0.0,
    }


PALETTE = {
    "PilotTravertine": material_spec((0.68, 0.56, 0.40), 0.0, 0.82),
    "PilotShadowStone": material_spec((0.48, 0.39, 0.28), 0.0, 0.89),
    "PilotMortar": material_spec((0.28, 0.22, 0.15), 0.0, 0.95),
    "PilotBronze": material_spec((0.47, 0.31, 0.105), 0.68, 0.45),
    "PilotRed": material_spec((0.36, 0.054, 0.034), 0.0, 0.88),
    "PilotBlue": material_spec((0.038, 0.132, 0.245), 0.0, 0.88),
    "PilotLinen": material_spec((0.72, 0.60, 0.36), 0.0, 0.92),
}


def make_materials():
    materials = {}
    for name, spec in PALETTE.items():
        material = bpy.data.materials.new(name)
        material.diffuse_color = (*spec["base_color"], 1.0)
        material.use_nodes = True
        shader = material.node_tree.nodes.get("Principled BSDF")
        shader.inputs["Base Color"].default_value = material.diffuse_color
        shader.inputs["Metallic"].default_value = spec["metallic"]
        shader.inputs["Roughness"].default_value = spec["roughness"]
        materials[name] = material
    return materials


def chamfered_rectangle(x0, x1, z0, z1, cut, seed=None):
    """A closed X/Z contour. Small unequal chips break up the ashlar edges."""
    rng = random.Random(seed) if seed is not None else None
    cuts = [cut * rng.uniform(0.60, 1.25) if rng else cut for _ in range(8)]
    a, b, c, d, e, f, g, h = cuts
    return [(x0 + a, z0), (x1 - b, z0), (x1, z0 + c), (x1, z1 - d),
            (x1 - e, z1), (x0 + f, z1), (x0, z1 - g), (x0, z0 + h)]


class Asset:
    def __init__(self, name, materials):
        self.name = name
        self.materials = materials
        self.parts = []
        self.collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(self.collection)
        self.pivot = bpy.data.objects.new(f"{name}_PlacementOrigin", None)
        self.pivot.empty_display_type = "ARROWS"
        self.pivot.empty_display_size = 0.22
        self.collection.objects.link(self.pivot)
        self.groups = {}
        for name in ("Structure", "Mouldings", "Ornaments"):
            group = bpy.data.collections.new(f"{self.name}_{name}")
            self.collection.children.link(group)
            self.groups[name] = group

    def mesh(self, name, verts, faces, material, group, bevel=0.0, smooth=False):
        mesh = bpy.data.meshes.new(f"{self.name}_{name}")
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(mesh)
        bm.free()
        mesh.update()
        mesh.materials.append(self.materials[material])

        # Metre-scale planar UVs. Face orientation selects a projection so the
        # stone keeps the same texture scale on faces, returns, and edge mouldings.
        uv_data = mesh.uv_layers.new(name="UVMap").uv
        for face in mesh.polygons:
            normal_axis = max(range(3), key=lambda axis: abs(face.normal[axis]))
            u_axis, v_axis = [(1, 2), (0, 2), (0, 1)][normal_axis]
            for loop_index in face.loop_indices:
                co = mesh.vertices[mesh.loops[loop_index].vertex_index].co
                uv_data[loop_index].vector = (co[u_axis], co[v_axis])
            face.use_smooth = smooth

        obj = bpy.data.objects.new(name, mesh)
        self.groups[group].objects.link(obj)
        obj.parent = self.pivot
        if bevel:
            modifier = obj.modifiers.new("Soft stone edges", "BEVEL")
            modifier.width = bevel
            modifier.segments = 2
            modifier.limit_method = "ANGLE"
            modifier.angle_limit = math.radians(24)
        self.parts.append(obj)
        return obj

    def extrude(self, name, contour, y_front, y_back, material, group, bevel=0.0):
        count = len(contour)
        verts = [(x, y, z) for y in (y_front, y_back) for x, z in contour]
        faces = [tuple(range(count)), tuple(reversed(range(count, 2 * count)))]
        for i in range(count):
            j = (i + 1) % count
            faces.append((i, j, j + count, i + count))
        return self.mesh(name, verts, faces, material, group, bevel)

    def stone(self, name, x0, x1, z0, z1, y_front, y_back, material="PilotTravertine", group="Structure", cut=0.014, seed=None):
        contour = chamfered_rectangle(x0, x1, z0, z1, cut, seed)
        return self.extrude(name, contour, y_front, y_back, material, group, min(0.004, cut / 2))

    def disc(self, name, x, z, radius, y_front, y_back, material, sides=32):
        contour = [(x + radius * math.cos(i * math.tau / sides), z + radius * math.sin(i * math.tau / sides))
                   for i in range(sides)]
        return self.extrude(name, contour, y_front, y_back, material, "Ornaments", 0.002)

    def export(self):
        """Export an evaluated, single mesh without flattening the editable source."""
        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        clones = []
        for part in self.parts:
            data = bpy.data.meshes.new_from_object(part.evaluated_get(depsgraph), depsgraph=depsgraph)
            copy = bpy.data.objects.new(f"Export_{part.name}", data)
            bpy.context.scene.collection.objects.link(copy)
            copy.matrix_world = part.matrix_world.copy()
            clones.append(copy)
        bpy.ops.object.select_all(action="DESELECT")
        for clone in clones:
            clone.select_set(True)
        bpy.context.view_layer.objects.active = clones[0]
        bpy.ops.object.join()
        combined = bpy.context.view_layer.objects.active
        combined.name = self.name
        combined.data.name = self.name
        combined.data.calc_loop_triangles()
        coords = [combined.matrix_world @ v.co for v in combined.data.vertices]
        stats = {
            "bounds_min_m": [round(min(p[i] for p in coords), 5) for i in range(3)],
            "bounds_max_m": [round(max(p[i] for p in coords), 5) for i in range(3)],
            "triangles": len(combined.data.loop_triangles),
            "material_slots": [slot.material.name for slot in combined.material_slots],
        }
        bpy.ops.export_scene.fbx(
            filepath=str(OUTPUT / f"{self.name}.fbx"), use_selection=True,
            object_types={"MESH"}, apply_unit_scale=True, global_scale=1.0,
            apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
            mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False,
        )
        bpy.data.objects.remove(combined, do_unlink=True)
        print(f"CTF_PILOT_ASSET {self.name} {json.dumps(stats)}", flush=True)
        return stats


def fluted_pilaster(asset, x, suffix):
    """A tapered engaged shaft with real fluted cross sections."""
    steps = 56
    rings = []
    for z, taper in ((-0.91, 1.0), (-0.50, 0.98), (0.54, 0.89), (0.78, 0.87)):
        ring = []
        for i in range(steps + 1):
            angle = math.pi * i / steps
            flute = 1.0 - 0.16 * (0.5 + 0.5 * math.cos(angle * 16.0))
            ring.append((x + 0.151 * taper * math.cos(angle), -0.113 - 0.070 * taper * math.sin(angle) * flute, z))
        rings.append(ring)
    count = steps + 1
    faces = [tuple(reversed(range(count))), tuple(range(count * 3, count * 4))]
    for row in range(3):
        for i in range(count):
            j = (i + 1) % count
            faces.append((row * count + i, row * count + j, (row + 1) * count + j, (row + 1) * count + i))
    asset.mesh(f"Fluted_shaft_{suffix}", [p for ring in rings for p in ring], faces, "PilotTravertine", "Mouldings", smooth=True)
    for i, (width, lo, hi, front) in enumerate(((0.48, -1.12, -1.00, -0.176), (0.40, -1.00, -0.91, -0.183),
                                                (0.35, 0.77, 0.86, -0.174), (0.48, 0.86, 0.94, -0.188))):
        asset.stone(f"Pilaster_{suffix}_{i}", x - width / 2, x + width / 2, lo, hi, front, -0.103,
                    group="Mouldings", cut=0.014)


def facade(materials):
    asset = Asset("SM_CTFRomanFacade", materials)
    asset.stone("Backing", -1.925, 1.925, -1.20, 1.20, -0.074, 0.012, "PilotMortar", cut=0.022)
    for row in range(5):
        z0 = -1.15 + row * 0.45
        boundaries = [-1.90, -1.29, -0.63, 0.06, 0.71, 1.34, 1.90] if row % 2 == 0 else \
            [-1.90, -1.56, -0.95, -0.29, 0.39, 1.06, 1.69, 1.90]
        for col, (x0, x1) in enumerate(zip(boundaries, boundaries[1:])):
            asset.stone(f"Ashlar_{row}_{col}", x0 + 0.004, x1 - 0.004, z0 + 0.006, z0 + 0.438,
                        -0.117 - 0.003 * ((row + col * 3) % 4), -0.072,
                        "PilotShadowStone" if (row + col) % 8 == 0 else "PilotTravertine", seed=row * 21 + col)

    # A shallow blind arch with separate voussoirs, a marked keystone and a
    # recessed bronze patera. It reads as wall relief and cannot suggest a path.
    spring = -0.04
    opening = 0.77
    outer = 0.98
    niche = [(-opening, -0.96), (opening, -0.96)]
    niche += [(opening * math.cos(i * math.pi / 36), spring + opening * math.sin(i * math.pi / 36)) for i in range(37)]
    asset.extrude("Blind_arch_inset", niche, -0.131, -0.116, "PilotShadowStone", "Structure", 0.003)
    for i in range(13):
        start = math.pi * i / 13 + 0.006
        end = math.pi * (i + 1) / 13 - 0.006
        outer_radius = outer + (0.040 if i == 6 else 0.0)
        contour = [(outer_radius * math.cos(a), spring + outer_radius * math.sin(a))
                   for a in [start + (end - start) * k / 4 for k in range(5)]]
        contour += [(opening * math.cos(a), spring + opening * math.sin(a))
                    for a in [end - (end - start) * k / 4 for k in range(5)]]
        asset.extrude(f"Voussoir_{i:02d}", contour, -0.190 if i == 6 else -0.178, -0.114,
                      "PilotTravertine", "Mouldings", 0.004)
    for sign in (-1, 1):
        a, b = sorted((sign * opening, sign * outer))
        asset.stone(f"Jamb_{sign}", a, b, -0.97, spring - 0.008, -0.172, -0.114, group="Mouldings")
        fluted_pilaster(asset, sign * 1.48, "L" if sign < 0 else "R")
    for i, (lo, hi, front) in enumerate(((0.96, 1.035, -0.162), (1.035, 1.10, -0.176), (1.10, 1.195, -0.188),
                                          (-1.195, -1.105, -0.179))):
        asset.stone(f"Moulding_{i}", -1.918, 1.918, lo, hi, front, -0.072, group="Mouldings", cut=0.01)
    for i in range(22):
        x = (i - 10.5) * 0.166
        asset.stone(f"Dentil_{i:02d}", x - 0.047, x + 0.047, 1.052, 1.116, -0.187, -0.147,
                    group="Mouldings", cut=0.005)
    asset.disc("Patera_backing", 0, -0.26, 0.252, -0.159, -0.129, "PilotMortar")
    asset.disc("Patera_raised_rim", 0, -0.26, 0.233, -0.178, -0.158, "PilotBronze", 48)
    asset.disc("Patera_centre", 0, -0.26, 0.157, -0.184, -0.175, "PilotShadowStone", 40)
    for i in range(12):
        angle = math.tau * i / 12
        asset.disc(f"Patera_stud_{i}", 0.198 * math.cos(angle), -0.26 + 0.198 * math.sin(angle),
                   0.013, -0.189, -0.174, "PilotLinen", 10)
    return asset


def banner(materials, team, pigment):
    asset = Asset(f"SM_CTFRomanBanner_{team}", materials)
    # Bronze crossbar and wall brackets remain within a shallow mounted profile.
    asset.stone("Mounting_stone", -0.48, 0.48, 2.035, 2.20, -0.035, 0.010, group="Structure", cut=0.017)
    asset.stone("Hanging_crossbar", -0.535, 0.535, 2.063, 2.116, -0.134, -0.038, "PilotBronze", "Mouldings", 0.016)
    for sign in (-1, 1):
        asset.disc(f"Crossbar_rosette_{sign}", sign * 0.5, 2.09, 0.04, -0.156, -0.125, "PilotBronze", 18)
        asset.disc(f"Wall_pin_{sign}", sign * 0.40, 2.16, 0.022, -0.046, -0.030, "PilotBronze", 14)
    for x in (-0.33, -0.11, 0.11, 0.33):
        asset.stone(f"Sewn_loop_{x}", x - 0.027, x + 0.027, 1.965, 2.083, -0.118, -0.080,
                    pigment, "Mouldings", 0.008)

    def fabric(u, v, offset=0):
        # Shallow, immobile fabric folds maintain readable silhouettes and work
        # well against the base walls. A pointed hem keeps the Roman standard shape.
        centre = 1.0 - abs(2.0 * u - 1.0)
        width = 0.84 * (1.0 - 0.035 * v)
        x = (u - 0.5) * width
        z = 2.00 - v * (1.82 + 0.18 * centre)
        y = -0.094 + 0.018 * math.sin(u * math.tau * 2.7 + v * 0.8) * (0.28 + 0.72 * v)
        return (x, y + offset, z)

    def patch(name, u0, u1, v0, v1, nu, nv, slot, offset):
        verts = [fabric(u0 + (u1-u0)*i/nu, v0 + (v1-v0)*j/nv, offset)
                 for j in range(nv + 1) for i in range(nu + 1)]
        faces = [(j*(nu+1)+i, (j+1)*(nu+1)+i, (j+1)*(nu+1)+i+1, j*(nu+1)+i+1)
                 for j in range(nv) for i in range(nu)]
        return asset.mesh(name, verts, faces, slot, "Ornaments", smooth=True)

    cloth = patch("Woven_cloth", 0, 1, 0, 1, 28, 28, pigment, 0.0)
    thick = cloth.modifiers.new("Woven backing", "SOLIDIFY")
    thick.thickness = 0.006
    thick.offset = -1.0
    for i, (u0, u1, v0, v1) in enumerate(((0.035, 0.065, 0.025, 0.983), (0.935, 0.965, 0.025, 0.983),
                                           (0.035, 0.965, 0.025, 0.049), (0.035, 0.965, 0.960, 0.983))):
        nu, nv = (2, 28) if i < 2 else (28, 2)
        patch(f"Woven_border_{i}", u0, u1, v0, v1, nu, nv, "PilotLinen", -0.003)
    for i in range(19):
        u = 0.066 + i * 0.048
        patch(f"Hem_stitch_{i}", u, u + 0.013, 0.983, 0.998, 2, 2, "PilotLinen", -0.004)

    # Original embroidered laurel and diamond standard, matching both factions.
    def embroidery(name, uv_points, subdivisions=4):
        # Tessellate the appliqué in fabric coordinates so it follows the folds.
        verts, faces = [], []
        for index in range(1, len(uv_points) - 1):
            a, b, c = uv_points[0], uv_points[index], uv_points[index + 1]
            grid = {}
            for i in range(subdivisions + 1):
                for j in range(subdivisions + 1 - i):
                    grid[i, j] = len(verts)
                    u = a[0] + (i * (b[0] - a[0]) + j * (c[0] - a[0])) / subdivisions
                    v = a[1] + (i * (b[1] - a[1]) + j * (c[1] - a[1])) / subdivisions
                    verts.append(fabric(u, v, -0.005))
            for i in range(subdivisions):
                for j in range(subdivisions - i):
                    faces.append((grid[i, j], grid[i + 1, j], grid[i, j + 1]))
                    if i + j < subdivisions - 1:
                        faces.append((grid[i + 1, j], grid[i + 1, j + 1], grid[i, j + 1]))
        asset.mesh(name, verts, faces, "PilotLinen", "Ornaments")

    embroidery("Diamond_badge", [(0.50, 0.253), (0.64, 0.389), (0.50, 0.527), (0.36, 0.389)], 10)
    for sign in (-1, 1):
        for i in range(9):
            t = i / 8
            u = 0.50 + sign * (0.055 + 0.22 * math.sin(t * math.pi * 0.75))
            v = 0.72 - 0.34 * t
            embroidery(f"Laurel_leaf_{sign}_{i}", [(u - sign*0.026, v + 0.019), (u - sign*0.018, v - 0.003),
                                                    (u + sign*0.043, v - 0.022), (u + sign*0.028, v + 0.004)])
    patch("Embroidered_lower_rule", 0.31, 0.69, 0.781, 0.792, 12, 2, "PilotLinen", -0.005)
    return asset


def victory_finial(materials):
    asset = Asset("SM_CTFVictoryFinial", materials)

    def pedestal(name, x_radius, y_radius, lo, hi, material):
        count = 12
        verts = [(x_radius * math.cos(math.tau * i / count), y_radius * math.sin(math.tau * i / count), z)
                 for z in (lo, hi) for i in range(count)]
        faces = [tuple(reversed(range(count))), tuple(range(count, 2 * count))]
        for i in range(count):
            j = (i + 1) % count
            faces.append((i, j, j + count, i + count))
        asset.mesh(name, verts, faces, material, "Structure", 0.006)

    pedestal("Finial_seat", 0.254, 0.176, -0.44, -0.37, "PilotShadowStone")
    pedestal("Cast_bronze_foot", 0.220, 0.150, -0.37, -0.32, "PilotBronze")
    pedestal("Cast_bronze_socket", 0.128, 0.102, -0.32, -0.23, "PilotBronze")
    asset.stone("Medal_support", -0.063, 0.063, -0.255, -0.12, -0.061, 0.061, "PilotBronze", "Structure", 0.009)
    asset.disc("Victory_medal_body", 0.0, 0.018, 0.198, -0.066, 0.066, "PilotBronze", 48)
    for sign in (-1, 1):
        front, back = sorted((sign * 0.078, sign * 0.061))
        asset.disc(f"Medal_recess_{sign}", 0.0, 0.018, 0.162, front, back, "PilotShadowStone", 40)
        front, back = sorted((sign * 0.090, sign * 0.077))
        asset.disc(f"Sun_embossing_{sign}", 0.0, 0.018, 0.052, front, back, "PilotBronze", 28)
        for i in range(10):
            angle = i * math.tau / 10
            cos_a, sin_a = math.cos(angle), math.sin(angle)
            contour = [(r * cos_a - tangent * sin_a, 0.018 + r * sin_a + tangent * cos_a)
                       for r, tangent in ((0.062, -0.010), (0.139, 0.0), (0.062, 0.010))]
            asset.extrude(f"Sun_ray_{sign}_{i}", contour, front, back, "PilotBronze", "Ornaments", 0.0015)

    for side in (-1, 1):
        segments = 20
        sides = 9
        verts = []
        for segment in range(segments + 1):
            t = segment / segments
            angle = -1.20 + 2.30 * t
            radius = 0.019 - 0.009 * t
            for i in range(sides):
                around = i * math.tau / sides
                radial = 0.320 + radius * math.cos(around)
                verts.append((side * radial * math.cos(angle), radius * math.sin(around), 0.018 + radial * math.sin(angle)))
        faces = [tuple(reversed(range(sides))), tuple(range(segments*sides, (segments+1)*sides))]
        for row in range(segments):
            for i in range(sides):
                j = (i + 1) % sides
                faces.append((row*sides+i, row*sides+j, (row+1)*sides+j, (row+1)*sides+i))
        asset.mesh(f"Laurel_branch_{side}", verts, faces, "PilotBronze", "Mouldings", smooth=True)
        for i in range(9):
            angle = -1.01 + i * 0.245
            x0 = side * 0.320 * math.cos(angle)
            z0 = 0.018 + 0.320 * math.sin(angle)
            for branch in (-1, 1):
                length = 0.084 - i * 0.002
                radial = branch * 0.70
                along = 0.71
                dx = side * length * (radial * math.cos(angle) - along * math.sin(angle))
                dz = length * (radial * math.sin(angle) + along * math.cos(angle))
                wx, wz = -0.21 * dz, 0.21 * dx
                cx, cz = x0 + 0.48 * dx, z0 + 0.48 * dz
                leaf = [(x0, 0.0, z0), (cx + wx, 0.006, cz + wz), (x0 + dx, 0.0, z0 + dz),
                        (cx - wx, -0.006, cz - wz), (cx, -0.017, cz), (cx, 0.017, cz)]
                faces = [(i0, (i0 + 1) % 4, ridge) for ridge in (4, 5) for i0 in range(4)]
                asset.mesh(f"Laurel_leaf_{side}_{i}_{branch}", leaf, faces, "PilotBronze", "Ornaments", 0.0015)
    return asset


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    materials = make_materials()
    assets = [facade(materials), banner(materials, "Alpha", "PilotBlue"),
              banner(materials, "Bravo", "PilotRed"), victory_finial(materials)]
    stats = {asset.name: asset.export() for asset in assets}

    # Spread the editable groups out for browsing, while their local pivots and
    # separate exported FBXs remain positioned at their attachment points.
    assets[0].pivot.location = (0, 0, 1.20)
    assets[1].pivot.location = (2.9, 0, 0)
    assets[2].pivot.location = (4.25, 0, 0)
    assets[3].pivot.location = (5.48, 0, 0.44)
    bpy.ops.object.select_all(action="DESELECT")
    assets[0].pivot.select_set(True)
    bpy.context.view_layer.objects.active = assets[0].pivot
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                space = area.spaces.active
                space.shading.color_type = "MATERIAL"
                space.region_3d.view_location = (1.9, 0.0, 1.12)
                space.region_3d.view_rotation = Vector((-0.07, 1.0, -0.07)).to_track_quat("-Z", "Y")
                space.region_3d.view_distance = 9.8
                space.region_3d.view_perspective = "ORTHO"
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "CTFArtPilot.blend"))
    (OUTPUT / "palette.json").write_text(json.dumps({"materials": PALETTE, "assets": stats}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
