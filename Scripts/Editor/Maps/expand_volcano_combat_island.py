"""Extend the Volcano Combat map with traversable routes around the original main island."""
import json
import os
from pathlib import Path
import time

import unreal as ue


MAP = '/Game/BoldersDemo/Assets/Maps/L_VolcanoCombat'
ART = '/Game/BoldersDemo/Assets/Environment/VolcanoCombat'
TAG = ue.Name('FoundryIslandExpansion')
WALKABLE = ue.Name('WalkableGround')
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
LEVELS = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
EDITOR = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
RAMP_PLACEMENT = {
    'West': ((-2350, -1380, 1558), 0, (1, 1, 1)),
    'North': ((385, 1610, 1558), -90, (1, 1, 1)),
    'East': ((4470, 625, 1558), 180, (1, 1, 1)),
}
ENTRY = ((-805, 3150), (-435, 3150))
GUARDS = (((-985, 2125), (-1540, 1760)), ((-2040, -740), (-2780, -1320), (-1860, -1120)))

# All positions are on the main island, with low routes available on both sides of the star.
REGION_PROBES = {
    'entry': (-805, 3150, 1560), 'gate': (-990, 2130, 1560), 'west_lane': (-2050, -760, 1560),
    'west_ramp_lower': (-2500, -1380, 1560), 'west_ramp_upper': (-810, -1380, 1810),
    'north_ramp_lower': (385, 1640, 1560), 'north_ramp_upper': (385, 390, 1810),
    'east_ramp_lower': (4400, 625, 1580), 'east_ramp_upper': (3230, 625, 1810),
    'outer_east': (3920, 3270, 1560), 'outer_ridge': (2630, 5040, 1560),
    'outer_west': (-3070, -1540, 1560), 'boss': (1962, -1769, 1810),
}
PROBE_PATHS = (
    ('entry', 'gate'), ('gate', 'west_lane'), ('west_lane', 'west_ramp_lower'),
    ('west_ramp_lower', 'west_ramp_upper'), ('west_ramp_upper', 'boss'),
    ('north_ramp_lower', 'north_ramp_upper'), ('east_ramp_lower', 'east_ramp_upper'),
    ('east_ramp_lower', 'outer_east'), ('outer_east', 'outer_ridge'),
    ('west_ramp_lower', 'outer_west'), ('boss', 'outer_east'),
)


def import_crossings():
    source = Path(os.getenv('BOLDERS_ISLAND_GEOMETRY', 'H:/UE5/AssetSources/BoldersDemo/VolcanoCombat/IslandCrossings'))
    library = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    mesh_editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    stone = ue.load_asset(ART + '/Materials/M_FoundryArenaStone')
    coping = ue.load_asset(ART + '/Materials/M_FoundryBasalt')
    flag = 'Interchange.FeatureFlags.Import.FBX'
    previous = ue.SystemLibrary.get_console_variable_bool_value(flag)
    ue.SystemLibrary.execute_console_command(None, flag + ' 0')
    loaded = {}
    try:
        for side in RAMP_PLACEMENT:
            name = 'SM_FoundryIslandRamp_' + side
            path = ART + '/Meshes/' + name
            options = ue.FbxImportUI()
            options.set_editor_properties(dict(automated_import_should_detect_type=False,
                mesh_type_to_import=ue.FBXImportType.FBXIT_STATIC_MESH, import_as_skeletal=False,
                import_materials=False, import_textures=False))
            options.static_mesh_import_data.set_editor_properties(dict(auto_generate_collision=False,
                combine_meshes=True, convert_scene=True, force_front_x_axis=False, convert_scene_unit=True,
                normal_import_method=ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS))
            task = ue.AssetImportTask()
            task.set_editor_properties(dict(filename=str(source / (name + '.fbx')), destination_path=ART+'/Meshes',
                destination_name=name, automated=True, options=options, factory=ue.FbxFactory(),
                replace_existing=True, replace_existing_settings=True, save=True))
            tools.import_asset_tasks([task])
            assert task.imported_object_paths, name
            mesh = ue.load_asset(path)
            bounds = mesh.get_bounding_box()
            assert bounds.min.x > -1 and bounds.max.x > 1350 and abs(bounds.max.z - 256) < 2, (name, bounds)
            for i, slot in enumerate(mesh.get_editor_property('static_materials')):
                label = str(slot.get_editor_property('imported_material_slot_name'))
                mesh.set_material(i, coping if 'Coping' in label else stone)
            mesh_editor.set_collision_complexity(mesh, ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            assert library.save_loaded_asset(mesh, False)
            loaded[side] = mesh
    finally:
        ue.SystemLibrary.execute_console_command(None, flag + (' 1' if previous else ' 0'))
    return loaded


def wire_level(meshes):
    world = EDITOR.get_editor_world()
    assert world.get_name() == 'L_VolcanoCombat', world
    actors = ACTORS.get_all_level_actors()
    island = next(a for a in actors if a.get_name() == 'SM_Platform_003_1068')
    encounter = next(a for a in actors if a.get_class().get_name() == 'VolcanoEncounter')
    boss = encounter.get_editor_property('boss')
    assert boss and boss.get_actor_scale3d() == ue.Vector(1, 1, 1)
    boss.set_editor_properties(dict(chase_radius=14500, engagement_radius=17500))
    character_asset = '/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe'
    boss.get_component_by_class(ue.SkeletalMeshComponent).set_skeletal_mesh_asset(
        ue.load_asset(character_asset + '/SK_StatueBoss_Mixamo'))
    boss.get_component_by_class(ue.StaticMeshComponent).set_static_mesh(
        ue.load_asset(character_asset + '/SM_StatueBoss_MixamoAxe'))
    boss.get_component_by_class(ue.SkeletalMeshComponent).set_relative_rotation(ue.Rotator(), False, True)
    for actor in actors:
        if actor.actor_has_tag(TAG):
            ACTORS.destroy_actor(actor)
        elif isinstance(actor, ue.StaticMeshActor):
            mesh = actor.static_mesh_component.static_mesh
            if mesh and any(key in mesh.get_name() for key in ('Platform', 'Stairs')):
                xy = actor.get_actor_location()
                if abs(xy.x - 1000) < 7300 and abs(xy.y) < 7500:
                    actor.set_editor_property('tags', list(set(actor.tags) | {WALKABLE}))
    assert island.actor_has_tag(WALKABLE)

    navigation = next(a for a in actors if isinstance(a, ue.NavMeshBoundsVolume))
    navigation.set_actor_label('Main Island Navigation')
    navigation.set_actor_location(ue.Vector(1080, -50, 1480), False, True)
    navigation.set_actor_scale3d(ue.Vector(79, 76, 17))
    nav_system = ue.NavigationSystemV1.get_navigation_system(world)
    assert nav_system
    nav_system.on_navigation_bounds_updated(navigation)

    def place(cls, label, position, yaw, folder):
        actor = ACTORS.spawn_actor_from_class(cls, ue.Vector(*position), ue.Rotator(yaw=yaw))
        actor.set_actor_label(label)
        actor.set_folder_path(ue.Name('Volcano Combat/Island/' + folder))
        actor.set_editor_property('tags', [TAG])
        return actor

    for side, (xyz, yaw, scale) in RAMP_PLACEMENT.items():
        ramp = place(ue.StaticMeshActor, side + ' Basalt Crossing', xyz, yaw, 'Crossings')
        ramp.set_actor_scale3d(ue.Vector(*scale))
        ramp.set_editor_property('tags', [TAG, WALKABLE])
        ramp.static_mesh_component.set_static_mesh(meshes[side])
        ramp.static_mesh_component.set_collision_profile_name('BlockAll')

    def transfer(point, xy):
        hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(*xy, 2120), ue.Vector(*xy, 1050),
            ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], ue.DrawDebugTrace.NONE, True)
        assert hit, xy
        hit_data = hit.to_tuple()
        assert hit_data[7].z > .81 and hit_data[9].actor_has_tag(WALKABLE), (point.get_actor_label(), xy, hit_data[9].get_name())
        point.set_actor_location(hit_data[5] + ue.Vector(0, 0, 103), False, True)

    starts = sorted([a for a in actors if a.actor_has_tag(ue.Name('WarriorSpawn'))], key=lambda a: a.get_name())
    assert len(starts) == len(ENTRY)
    for point, xy in zip(starts, ENTRY):
        transfer(point, xy)
        point.set_folder_path(ue.Name('Volcano Combat/Island/Entry'))
    for wave_key, positions in zip(('first_wave_spawns', 'second_wave_spawns'), GUARDS):
        points = encounter.get_editor_property(wave_key)
        assert len(points) == len(positions)
        for point, xy in zip(points, positions):
            transfer(point, xy)
            point.set_folder_path(ue.Name('Volcano Combat/Island/Guard Posts'))
    encounter.set_editor_properties(dict(arena_fall_z=620.0, progress_by_region=True))

    hazard_class = ue.load_class(None, '/Script/BoldersDemo.VolcanoHazard')
    for label, kind, pos, radius, delay in (
        ('West Broken Vent', ue.VolcanoHazardKind.ERUPTION, (-2290, 120, 1558), 140, 8.3),
        ('Cooling Runoff', ue.VolcanoHazardKind.PUDDLE, (4130, 1110, 1558), 155, 0),
        ('Supply Barrel', ue.VolcanoHazardKind.EXPLOSIVE_BARREL, (-1270, 1110, 1558), 266, 0),
    ):
        hazard = place(hazard_class, label, pos, 0, 'Outer Hazards')
        hazard.set_editor_properties(dict(kind=kind, radius=radius, cycle_offset=delay))

    # The cast grate sits on the same stone construction used by the original foundry installations.
    pedestal = ue.load_asset('/Game/Inferno_World_Free/Meshes/Decorations/SM_Pedestal_001')
    bounds = pedestal.get_bounding_box()
    size = bounds.max - bounds.min
    scale = ue.Vector(187 / size.x, 157 / size.y, 51 / size.z)
    center = (bounds.min + bounds.max) * .5
    pedestal_origin = (-2290 - center.x * scale.x, 120 - center.y * scale.y, 1558 - bounds.max.z * scale.z)
    footing = place(ue.StaticMeshActor, 'West Vent Stone Curb', pedestal_origin, 0, 'Outer Hazards')
    footing.set_actor_scale3d(scale)
    footing.static_mesh_component.set_static_mesh(pedestal)
    footing.static_mesh_component.set_material(0, ue.load_asset(ART + '/Materials/M_FoundryBasalt'))
    footing.static_mesh_component.set_collision_profile_name('NoCollision')
    footing.static_mesh_component.set_editor_property('can_ever_affect_navigation', False)

    EDITOR.set_level_viewport_camera_info(ue.Vector(1130, 7210, 12175), ue.Rotator(pitch=-59, yaw=-90))
    nav_system.on_navigation_bounds_updated(navigation)
    return world


def verify_paths(world):
    points, connections = {}, []
    for name, values in REGION_PROBES.items():
        point = ue.NavigationSystemV1.project_point_to_navigation(world, ue.Vector(*values), None, None,
            ue.Vector(180, 180, 130))
        points[name] = point
    for a, b in PROBE_PATHS:
        start, end = points[a], points[b]
        path = ue.NavigationSystemV1.find_path_to_location_synchronously(world, start, end) if start and end else None
        connections.append({'from': a, 'to': b, 'connected': bool(path and path.is_valid() and not path.is_partial()),
                            'length_cm': path.get_path_length() if path and path.is_valid() else None})
    data = {'points': {k: list(p.to_tuple()) if p else None for k, p in points.items()}, 'routes': connections}
    folder = Path(ue.Paths.project_saved_dir()) / 'IslandBossRework'
    (folder / 'navigation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    return connections


def run():
    world = wire_level(import_crossings())
    started = time.monotonic()
    requested = None
    last_blocked = started
    def wait_for_navigation(_):
        nonlocal requested, last_blocked
        now = time.monotonic()
        if ue.NavigationSystemV1.is_navigation_being_built_or_locked(world):
            last_blocked = now
            if now - started < 95:
                return
            ue.unregister_slate_post_tick_callback(handle)
            raise RuntimeError('The island navigation rebuild timed out.')
        if requested is None:
            ue.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
            requested = now
            return
        if now - max(requested, last_blocked) < 1.8:
            return
        ue.unregister_slate_post_tick_callback(handle)
        paths = verify_paths(world)
        failures = [(p['from'], p['to']) for p in paths if not p['connected']]
        if failures:
            ue.log_error(f'FOUNDRY_ISLAND_NAV_GAPS {failures}')
            if os.getenv('BOLDERS_ISLAND_EXIT_AFTER_SAVE') == '1':
                ue.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
            return
        assert LEVELS.save_current_level()
        print('FOUNDRY_ISLAND_NAV_READY', len(paths))
        if os.getenv('BOLDERS_ISLAND_EXIT_AFTER_SAVE') == '1':
            ue.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
    handle = ue.register_slate_post_tick_callback(wait_for_navigation)


if __name__ == '__main__':
    run()

