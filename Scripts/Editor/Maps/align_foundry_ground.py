"""Align the visible Volcano Combat surfaces with the terrain used by movement.

Run with L_VolcanoCombat open and the GeometryScripting plugin enabled.
The project copies keep the existing simple collision and navigation heights.
"""
import unreal as u

ART = "/Game/BoldersDemo/Assets/Environment/VolcanoCombat"
MESHES = ART + "/Meshes"
CORE_SOURCE = "/Game/Inferno_World_Free/Meshes/Buildings/SM_PlatformStar_001"
CORE_NAME = MESHES + "/SM_FoundryCorePlatform"
SHELF_NAME = MESHES + "/SM_FoundryWalkableSurfaces"
SHELF_LABEL = "Foundry Walkable Surfaces"
CORE_LIFT = 49.88
LIB = u.EditorAssetLibrary
MESH_API = u.GeometryScript_AssetUtils
MESH_EDITOR = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LEVELS = u.get_editor_subsystem(u.LevelEditorSubsystem)


def copy_core_art():
    source = LIB.load_asset(CORE_SOURCE)
    result = LIB.load_asset(CORE_NAME) if LIB.does_asset_exist(CORE_NAME) else LIB.duplicate_asset(CORE_SOURCE, CORE_NAME)
    assert source and result, CORE_NAME
    lod_count = MESH_EDITOR.get_lod_count(source)
    for index in range(lod_count):
        mesh = u.DynamicMesh()
        read_lod = u.GeometryScriptMeshReadLOD(lod_type=u.GeometryScriptLODType.SOURCE_MODEL, lod_index=index)
        _, outcome = MESH_API.copy_mesh_from_static_mesh_v2(source, mesh, u.GeometryScriptCopyMeshFromAssetOptions(),
                                                            read_lod, False)
        assert outcome == u.GeometryScriptOutcomePins.SUCCESS, index
        u.GeometryScript_MeshTransforms.translate_mesh(mesh, u.Vector(0, 0, CORE_LIFT))
        options = u.GeometryScriptCopyMeshToAssetOptions()
        _, outcome = MESH_API.copy_mesh_to_static_mesh(mesh, result, options, u.GeometryScriptMeshWriteLOD(lod_index=index), False)
        assert outcome == u.GeometryScriptOutcomePins.SUCCESS, index
    assert LIB.save_loaded_asset(result, False)
    return result


def playable_collision_triangles(component):
    hull = u.DynamicMesh()
    _, _, outcome = u.GeometryScript_SceneUtils.copy_collision_meshes_from_object(component, hull, True)
    assert outcome == u.GeometryScriptOutcomePins.SUCCESS, component
    query = u.GeometryScript_MeshQueries
    triangles = []
    for index in range(hull.get_triangle_count()):
        points = [item for item in query.get_triangle_positions(hull, index) if isinstance(item, u.Vector)]
        assert len(points) == 3, index
        a, b, c = points
        face = (c - a).cross(b - a)
        normal = face * (1. / max(face.length(), 1e-6))
        if normal.z > .6:
            triangles.append((points, normal))
    assert len(triangles) >= 6, component
    return triangles


def lower_terrace_material():
    name = "M_FoundryLowerTerrace"
    path = ART + "/Materials/" + name
    if LIB.does_asset_exist(path):
        return LIB.load_asset(path)

    tools = u.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, ART + "/Materials", u.Material, u.MaterialFactoryNew())
    edit = u.MaterialEditingLibrary
    world = edit.create_material_expression(mat, u.MaterialExpressionWorldPosition, -420, 0)
    shade = edit.create_material_expression(mat, u.MaterialExpressionCustom, -180, 0)
    position = u.CustomInput()
    position.set_editor_property("input_name", "World")
    shade.set_editor_properties(dict(inputs=[position], output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,
        code="""
float2 p = World.xy / float2(245., 170.);
float y = floor(p.y);
p.x += frac(y * .41421);
float2 cell = floor(p);
float2 edge = .5 - abs(frac(p) - .5);
float hash = frac(sin(dot(cell, float2(23.31, 51.13))) * 12651.52);
float wear = sin(World.x * .091 + World.y * .127) * sin(World.y * .072 - World.x * .097);
float rock = smoothstep(.006, .024, min(edge.x, edge.y) + wear * .003);
float shoulder = smoothstep(.018, .071, min(edge.x, edge.y));
float3 basalt = float3(.103, .093, .087) * (.88 + .22 * hash + .09 * shoulder + .022 * wear);
return lerp(float3(.069, .060, .055), basalt, rock);
"""))
    roughness = edit.create_material_expression(mat, u.MaterialExpressionConstant, -165, 130)
    roughness.set_editor_property("r", .88)
    assert edit.connect_material_expressions(world, "", shade, "World")
    assert edit.connect_material_property(shade, "", u.MaterialProperty.MP_BASE_COLOR)
    assert edit.connect_material_property(roughness, "", u.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(mat)
    assert LIB.save_loaded_asset(mat, False)
    return mat


def append_surface(mesh, faces, offset, slot):
    vertices, normals, triangles = [], [], []
    for points, normal in faces:
        start = len(vertices)
        triangles.append(u.IntVector(start, start + 1, start + 2))
        for point in points:
            vertices.append(point + u.Vector(0, 0, offset))
            normals.append(normal)
    uv = [u.Vector2D(p.x / 170., p.y / 170.) for p in vertices]
    data = u.GeometryScriptSimpleMeshBuffers(vertices=vertices, normals=normals, uv0=uv, triangles=triangles)
    u.GeometryScript_MeshEdits.append_buffers_to_mesh(mesh, data, material_id=slot)


def build_surface_asset(outer, core, stone):
    mesh = u.DynamicMesh()
    # Place the lower shelf against the collision top, including its sloping edges.
    append_surface(mesh, playable_collision_triangles(outer), -.55, 0)
    # The recessed core backing fills gaps beyond the modeled stone tiles.
    append_surface(mesh, playable_collision_triangles(core), -14., 1)
    materials = [lower_terrace_material(), stone]
    if LIB.does_asset_exist(SHELF_NAME):
        result = LIB.load_asset(SHELF_NAME)
        options = u.GeometryScriptCopyMeshToAssetOptions()
        options.set_editor_properties(dict(replace_materials=True, new_materials=materials))
        _, outcome = MESH_API.copy_mesh_to_static_mesh(mesh, result, options, u.GeometryScriptMeshWriteLOD(), False)
    else:
        options = u.GeometryScriptCreateNewStaticMeshAssetOptions()
        result, outcome = u.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, SHELF_NAME, options)
        for slot, material in enumerate(materials):
            result.set_material(slot, material)
    assert outcome == u.GeometryScriptOutcomePins.SUCCESS
    # Expose the visual surfaces to complex traces for ground indicators.
    # Capsules and navmesh still use the original simple collision.
    MESH_EDITOR.remove_collisions(result)
    MESH_EDITOR.set_collision_complexity(result, u.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    assert LIB.save_loaded_asset(result, False)
    return result


def align(build_assets=True, save=True):
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_name() == "L_VolcanoCombat"
    actors = {actor.get_actor_label(): actor for actor in ACTORS.get_all_level_actors()}
    outer = actors["SM_Platform_003"].static_mesh_component
    core = actors["Walkable Star Arena"].static_mesh_component
    stone = core.get_material(0)
    surface_mesh = build_surface_asset(outer, core, stone) if build_assets else LIB.load_asset(SHELF_NAME)

    core_art = copy_core_art() if build_assets else LIB.load_asset(CORE_NAME)
    assert core_art and surface_mesh
    core.set_static_mesh(core_art)
    core.set_material(0, stone)
    surface = actors.get(SHELF_LABEL) or ACTORS.spawn_actor_from_class(u.StaticMeshActor, u.Vector())
    surface.set_actor_location(u.Vector(), False, True)
    surface.set_actor_label(SHELF_LABEL)
    surface.set_folder_path(u.Name("Volcano Combat/Island"))
    surface.set_editor_property("tags", [u.Name("WalkableGround")])
    component = surface.static_mesh_component
    component.set_static_mesh(surface_mesh)
    component.set_collision_profile_name("BlockAll")
    component.set_collision_enabled(u.CollisionEnabled.QUERY_ONLY)
    component.set_editor_property("can_ever_affect_navigation", False)
    if save:
        assert LEVELS.save_current_level()
    u.log("FOUNDRY_TERRAIN_ALIGNED")


if __name__ == "__main__":
    align()
