"""Build concept VFX meshes, materials and loadout overrides."""

import math
from pathlib import Path

import unreal as u

from elemental_assets import COMBAT, E, EFFECTS, TOOLS, load, material, save


FIRE = (1., .25, .035)
WATER = (.035, .52, .75)


class Geometry:
    def __init__(self):
        self.points = []
        self.faces = []

    def vertex(self, x, y, z, u_coord, v_coord):
        self.points.append((x, y, z, u_coord, v_coord))
        return len(self.points)

    def tube(self, rings, sides, section):
        first = len(self.points) + 1
        for row, ring in enumerate(rings):
            for col in range(sides + 1):
                f = col / sides
                x, y, z, v = section(ring, f * math.tau)
                self.vertex(x, y, z, f, v)
                if row and col:
                    a = first + row * (sides + 1) + col
                    self.faces.append((a, a - 1, a - sides - 2, a - sides - 1))

    def mesh(self, name, default_material):
        folder = Path(u.Paths.project_saved_dir()) / "CombatConceptVFX" / "MeshSource"
        folder.mkdir(parents=True, exist_ok=True)
        source = (folder / (name + ".obj")).resolve()
        lines = [f"o {name}"]
        lines += [f"v {x:.4f} {-y:.4f} {z:.4f}" for x, y, z, _, _ in self.points]
        lines += [f"vt {v:.4f} {w:.4f}" for _, _, _, v, w in self.points]
        lines += ["f " + " ".join(f"{i}/{i}" for i in face) for face in self.faces]
        payload = "\n".join(lines) + "\n"
        if E.does_asset_exist(f"{EFFECTS}/{name}") and source.is_file() and source.read_text(encoding="ascii") == payload:
            mesh = load(f"{EFFECTS}/{name}")
            if mesh.get_material(0) != default_material:
                mesh.set_material(0, default_material)
                save(mesh)
            return mesh
        source.write_text(payload, encoding="ascii")
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = EFFECTS
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.factory = u.FbxFactory()
        opts = u.FbxImportUI()
        opts.set_editor_property("automated_import_should_detect_type", False)
        opts.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
        opts.import_materials = False
        opts.import_textures = False
        opts.static_mesh_import_data.combine_meshes = True
        opts.static_mesh_import_data.auto_generate_collision = False
        task.options = opts
        TOOLS.import_asset_tasks([task])
        mesh = next((a for a in task.get_objects() if isinstance(a, u.StaticMesh)), None)
        assert mesh, (name, task.imported_object_paths)
        mesh.set_material(0, default_material)
        u.log(f"CONCEPT_MESH {name} {mesh.get_bounding_box()}")
        return save(mesh)


def mat(name, shader, color, time="burst", solid=False):
    return material("Concept_" + name, shader, color, time, u.BlendMode.BLEND_OPAQUE if solid else u.BlendMode.BLEND_ADDITIVE)


def update_loadout(role, changes):
    asset=load(f"{COMBAT}/DA_{role}AttackLoadout")
    modules=list(asset.get_editor_property("modules"))
    found=set()
    for module in modules:
        name=str(module.get_editor_property("id"))
        if name not in changes:
            continue
        found.add(name)
        for property_name,value in changes[name].items():
            module.set_editor_property(property_name,value)
    assert found == set(changes), (role,found)
    asset.set_editor_property("modules",modules)
    save(asset)
    for module in asset.get_editor_property("modules"):
        name = str(module.get_editor_property("id"))
        for key, expected in changes.get(name, {}).items():
            assert module.get_editor_property(key) == expected, (role, name, key)
    u.log(f"CONCEPT_LOADOUT {role} {sorted(found)}")
