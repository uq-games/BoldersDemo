"""Add dry architecture along the outer edge of each volcano station.

The arena builder calls this after placing the interactive equipment.
"""

import unreal as ue


INFERNO = '/Game/Inferno_World_Free/Meshes/Decorations'
GENERATED = ue.Name('VolcanoCombatGenerated')
ARCHITECTURE = ue.Name('VolcanoFoundryArchitecture')
ARENA_MAP = 'L_VolcanoCombat'
ARENA_FLOOR = 'WalkableGround'

# (label, source, local center, installed dimensions in cm, bottom over the floor,
#  own material key, lay flat). Each rear assembly is attached to a single fixture.
REAR_ASSEMBLIES = {
    'West': (
        ('Cooling Return', '/Engine/BasicShapes/Cube', (-223, 139), (29, 110, 8), 1, 'iron', False),
        ('Pump Seat', 'SM_BaseSmall_001', (-223, 227), (136, 83, 29), -2, 'stone', False),
        ('Pump Wheel', 'SM_Gear_001', (-223, 226), (79, 69, 13), 25, 'iron', True),
    ),
    'East': (
        ('Firebox Return', '/Engine/BasicShapes/Cube', (252, 246), (39, 48, 9), 1, 'iron', False),
        ('Firebox Retaining Stone', 'SM_ColumnBigBroken_002', (252, 276), (150, 86, 43), -7, 'stone', False),
    ),
    'Southern': (
        ('Flue Return', '/Engine/BasicShapes/Cube', (252, 246), (39, 48, 8), 1, 'iron', False),
        ('Flue Saddle', 'SM_Pedestal_001', (252, 281), (131, 82, 39), -2, 'stone', False),
        ('Flue Baffle', 'SM_CirclePlatformSmall_001', (252, 282), (112, 74, 21), 34, 'iron', False),
    ),
}


def measure_floor(world, station_point, center, yaw, local, dimensions, expected_floor, equipment):
    x, y = local
    width, depth, _ = dimensions
    heights = []
    support_points = [(x, y)]
    for side in (-1, 1):
        for back in (-1, 1):
            support_points.append((x + side * width * .40, y + back * depth * .40))
    for point in support_points:
        wx, wy = station_point(center, yaw, point)
        hit = ue.SystemLibrary.line_trace_single(
            world, ue.Vector(wx, wy, expected_floor + 95), ue.Vector(wx, wy, expected_floor - 24),
            ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, equipment, ue.DrawDebugTrace.NONE, True)
        if hit is None:
            raise RuntimeError(f'No floor at ({wx:.0f}, {wy:.0f}).')
        impact = hit.to_tuple()
        actor = impact[9]
        if not actor or not actor.actor_has_tag(ARENA_FLOOR):
            raise RuntimeError(f'{actor} intercepts the floor at ({wx:.0f}, {wy:.0f}).')
        heights.append(impact[5].z)
    if abs(heights[0] - expected_floor) > 8 or max(heights) - min(heights) > 8:
        raise RuntimeError(f'Uneven support at {local}: {[round(z, 2) for z in heights]}.')
    return heights[0]


def fit_part(actor, mesh, material, world_center, yaw, dimensions, bottom, lay_flat):
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    for slot in range(component.get_num_materials()):
        component.set_material(slot, material)
    _, extent = actor.get_actor_bounds(False)
    extent_xyz = (extent.x, extent.y, extent.z)

    width, depth, height = dimensions
    pitch = roll = 0
    scaled_dimensions = dimensions
    if lay_flat:
        thin_axis = min(range(3), key=extent_xyz.__getitem__)
        if thin_axis == 0:
            pitch = 90
            scaled_dimensions = (height, depth, width)
        elif thin_axis == 1:
            roll = 90
            scaled_dimensions = (width, height, depth)

    if min(extent_xyz) <= .01:
        raise RuntimeError(f'{mesh.get_name()} has no usable mesh bounds: {extent_xyz}.')
    actor.set_actor_scale3d(ue.Vector(*(size / (2 * extent) for size, extent in zip(scaled_dimensions, extent_xyz))))
    actor.set_actor_rotation(ue.Rotator(pitch=pitch, yaw=yaw, roll=roll), True)
    center, extent = actor.get_actor_bounds(False)
    offset = ue.Vector(world_center[0], world_center[1], bottom + extent.z) - center
    actor.set_actor_location(actor.get_actor_location() + offset, False, True)
    return (round(extent.x * 2, 2), round(extent.y * 2, 2), round(extent.z * 2, 2))


def author(world, stations, materials_path, floor_z, station_point):
    if world.get_name() != ARENA_MAP:
        raise RuntimeError(f'Choose {ARENA_MAP} before generating the foundry perimeter.')
    actor_system = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = actor_system.get_all_level_actors()
    existing = [actor for actor in actors if actor.actor_has_tag(ARCHITECTURE)]
    equipment = [actor for actor in actors if isinstance(actor, ue.VolcanoHazard)]
    palette = {'iron': ue.load_asset(f'{materials_path}/M_FoundryIron'),
               'stone': ue.load_asset(f'{materials_path}/M_FoundryBasalt')}
    if not all(palette.values()):
        raise RuntimeError('Generate the foundry iron and basalt materials first.')

    prepared = []
    for station, center, yaw, _ in stations:
        for label, source, local, dimensions, lift, finish, flat in REAR_ASSEMBLIES[station]:
            path = source if source.startswith('/') else f'{INFERNO}/{source}'
            mesh = ue.load_asset(path)
            if not isinstance(mesh, ue.StaticMesh):
                raise RuntimeError(f'Missing foundry source mesh {path}.')
            support = measure_floor(world, station_point, center, yaw, local, dimensions, floor_z, equipment)
            xy = station_point(center, yaw, local)
            prepared.append((station, label, mesh, xy, yaw, dimensions, support + lift, palette[finish], flat))

    additions = []
    try:
        for station, label, mesh, xy, yaw, dimensions, bottom, material, flat in prepared:
            actor = actor_system.spawn_actor_from_class(ue.StaticMeshActor, ue.Vector(*xy, bottom))
            if not actor:
                raise RuntimeError(f'Could not place {station} {label}.')
            additions.append(actor)
            actor.set_actor_label(f'{station} {label}')
            actor.set_folder_path(ue.Name(f'Volcano Combat/Foundry Stations/{station}/Architecture'))
            actor.set_editor_property('tags', [GENERATED, ARCHITECTURE])
            actual = fit_part(actor, mesh, material, xy, yaw, dimensions, bottom, flat)
            if actual[2] > 62:
                raise RuntimeError(f'{station} {label} is too tall: {actual}.')
            ue.log(f'FOUNDRY_ARCHITECTURE {station} {label} xy={xy} bottom={bottom:.1f} size={actual}')
    except Exception:
        for actor in additions:
            actor_system.destroy_actor(actor)
        raise

    for actor in existing:
        actor_system.destroy_actor(actor)
    ue.log(f'FOUNDRY_ARCHITECTURE_READY pieces={len(additions)}')
