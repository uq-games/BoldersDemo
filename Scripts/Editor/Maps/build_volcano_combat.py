"""Author the standalone Inferno combat map in Unreal Editor."""

import math
import os
from pathlib import Path
import runpy
import time

import unreal as ue


SOURCE = '/Game/Inferno_World_Free/Maps/Demonstration'
MAP = '/Game/BoldersDemo/Assets/Maps/L_VolcanoCombat'
MATERIALS = '/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Materials'
AUDIO = '/Game/BoldersDemo/Assets/Environment/VolcanoCombat/Audio'
GAME_MODE = '/Game/BoldersDemo/Blueprints/Shared/BP_BoldersGameMode'
GUARDIAN = '/Game/BoldersDemo/Blueprints/Characters/Enemy/BP_EnemyMushroom'
GENERATED = ue.Name('VolcanoCombatGenerated')
WALKABLE = ue.Name('WalkableGround')
FLOOR_Z = 1808.0
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
LEVELS = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
EDITOR = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)

# Each installation faces the fighting floor. The vent is on the outer side of the apron;
# the basin and barrel are reached from the arena, with a conductor from orb to basin.
STATIONS = (
    ('West', (960, -1145), 90, 1.5),
    ('East', (2805, -1110), -90, 5.0),
    ('Southern', (1930, -2640), 180, 8.5),
)
EQUIPMENT = (
    ('ERUPTION', 'Vent', (252, 190), 188),
    ('PUDDLE', 'Pool', (0, -78), 158),
    ('EXPLOSIVE_BARREL', 'Barrel', (233, -95), 345),
    ('LIGHTNING_ORB', 'Lightning Orb', (-223, 34), 340),
)


def spawn(cls, label, xy, height=FLOOR_Z, yaw=0, folder='Encounter'):
    location = ue.Vector(*xy, height)
    actor = ACTORS.spawn_actor_from_class(cls, location, ue.Rotator(yaw=yaw))
    assert actor, label
    actor.set_actor_location(location, False, True)
    actor.set_actor_label(label)
    actor.set_folder_path(ue.Name('Volcano Combat/' + folder))
    actor.set_editor_property('tags', [GENERATED])
    return actor


def station_point(center, yaw, local):
    angle = math.radians(yaw)
    x, y = local
    return (center[0] + x * math.cos(angle) - y * math.sin(angle),
            center[1] + x * math.sin(angle) + y * math.cos(angle))


def floor_detail(label, xy, height, yaw, folder, mesh, material, scale):
    actor = spawn(ue.StaticMeshActor, label, xy, height, yaw, folder)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)
    actor.set_actor_scale3d(ue.Vector(*scale))
    return actor


def build_foundry_stations():
    apron = ue.load_asset(f'{MATERIALS}/M_FoundryApron')
    iron = ue.load_asset(f'{MATERIALS}/M_FoundryIron')
    plane = ue.load_asset('/Engine/BasicShapes/Plane')
    cube = ue.load_asset('/Engine/BasicShapes/Cube')
    assert all((apron, iron, plane, cube)), 'Generate the foundry materials before building the level.'
    hazard_class = ue.load_class(None, '/Script/BoldersDemo.VolcanoHazard')
    for name, center, yaw, offset in STATIONS:
        folder = f'Foundry Stations/{name}'
        floor_detail(f'{name} Service Apron', station_point(center, yaw, (0, 12)), FLOOR_Z + 1.9,
                     yaw, folder, plane, apron, (6.60, 4.64, 1.0))
        for index, (start, end) in enumerate((((-223, 34), (-187, 34)), ((-187, 34), (-187, -78)),
                                             ((-187, -78), (-156, -78))), start=1):
            midpoint = tuple((a + b) * .5 for a, b in zip(start, end))
            heading = math.degrees(math.atan2(end[1] - start[1], end[0] - start[0]))
            floor_detail(f'{name} Conductor {index}', station_point(center, yaw, midpoint), FLOOR_Z + 4,
                         yaw + heading, folder, cube, iron, ((math.dist(start, end) + 4) * .01, .12, .035))
        for kind, label, local, radius in EQUIPMENT:
            xy = station_point(center, yaw, local)
            hazard = spawn(hazard_class, f'{name} {label}', xy, FLOOR_Z + 1.7, yaw, folder)
            hazard.set_editor_properties(dict(kind=getattr(ue.VolcanoHazardKind, kind), radius=radius,
                                              cycle_offset=offset if kind == 'ERUPTION' else 0.0))


def build_furnace_air():
    loop = ue.load_asset(f'{AUDIO}/SW_FoundryFurnaceAir_Loop')
    assert loop and loop.get_editor_property('looping')
    actor = spawn(ue.AmbientSound, 'Furnace Air', (3270, -1870), 2050, folder='Sound')
    sound = actor.get_component_by_class(ue.AudioComponent)
    sound.set_editor_properties(dict(sound=loop, volume_multiplier=.80, override_attenuation=True))
    attenuation = sound.get_editor_property('attenuation_overrides')
    attenuation.set_editor_properties(dict(attenuation_shape_extents=ue.Vector(1200, 0, 0),
                                           falloff_distance=5000.0))
    sound.set_editor_property('attenuation_overrides', attenuation)


def load_level():
    if not ue.EditorAssetLibrary.does_asset_exist(MAP):
        world = ue.EditorLoadingAndSavingUtils.new_map_from_template(SOURCE, False)
        assert world and ue.EditorLoadingAndSavingUtils.save_map(world, MAP), MAP
    assert LEVELS.load_level(MAP), MAP
    return EDITOR.get_editor_world()


def prepare_original(world):
    strip = (
        'SM_StatueKnight_002_1094', 'SM_Axe_001_1096', 'SM_PedestalBig_001_1098',
        'SM_Lid_002_1186', 'SM_ChestBig_001_1188', 'SM_GoldPile_002_1190', 'SM_GoldPileBig_001_1218',
        'SM_GoldPileSmall_001_1216', 'SM_GoldPileSmall_001_1220', 'SM_GoldPileSmall_001_1278',
        'SM_Brazier_004_1200', 'SM_Brazier_004_1202',
    )
    scene_lights = []
    basalt = ue.load_asset(f'{MATERIALS}/M_FoundryArenaStone')
    assert basalt, 'Generate the arena materials before building the level.'
    for existing in ACTORS.get_all_level_actors():
        if existing.get_name() in strip or existing.actor_has_tag(GENERATED):
            ACTORS.destroy_actor(existing)
        elif isinstance(existing, ue.PlayerStart):
            existing.set_actor_label('Arena Camera Start')
            existing.set_actor_location(ue.Vector(1830, -320, 2710), False, True)
            existing.set_actor_rotation(ue.Rotator(yaw=-90), True)
            existing.set_folder_path(ue.Name('Volcano Combat/Entry'))
        elif isinstance(existing, ue.DirectionalLight):
            scene_lights.append(existing.get_component_by_class(ue.DirectionalLightComponent))
        elif isinstance(existing, ue.StaticMeshActor):
            component = existing.static_mesh_component
            mesh = component.static_mesh
            if existing.get_name() == 'Plane_1622':
                component.set_collision_profile_name('NoCollision')
                component.set_editor_property('can_ever_affect_navigation', False)
            if existing.get_name() == 'SM_PlatformStar_001_1430':
                existing.set_editor_property('tags', [WALKABLE])
                existing.set_actor_label('Walkable Star Arena')
                component.set_material(0, basalt)
            if mesh and 'Cliff' in mesh.get_name():
                component.set_editor_property('can_ever_affect_navigation', False)

    scene_lights.sort(key=lambda light: light.get_editor_property('intensity'), reverse=True)
    for index, light in enumerate(scene_lights):
        light.set_editor_property('forward_shading_priority', len(scene_lights) - index)
    if scene_lights:
        scene_lights[0].set_light_color(ue.LinearColor(1.0, .89, .79, 1.0))

    settings = world.get_world_settings()
    settings.set_editor_property('default_game_mode', ue.EditorAssetLibrary.load_blueprint_class(GAME_MODE))


def build():
    world = load_level()
    prepare_original(world)

    grade = spawn(ue.PostProcessVolume, 'Arena Color and Exposure', (1800, -1400), folder='Lighting')
    grade.set_editor_properties(dict(unbound=True, priority=15.0))
    settings = grade.get_editor_property('settings')
    settings.set_editor_properties(dict(override_auto_exposure_bias=True, auto_exposure_bias=-.22,
                                        override_color_gain=True, color_gain=ue.Vector4(.98, 1.025, 1.10, 1.0),
                                        override_color_saturation=True, color_saturation=ue.Vector4(.91, .96, .98, .94)))
    grade.set_editor_property('settings', settings)
    outline = ue.load_asset('/Game/BoldersDemo/Assets/Shared/Materials/M_HoverOutline')
    if outline:
        grade.add_or_update_blendable(outline, 1.0)

    nav = spawn(ue.NavMeshBoundsVolume, 'Star Arena Navigation', (1880, -1520), 1700, folder='Navigation')
    nav.set_actor_scale3d(ue.Vector(31, 33, 8))
    center, extent = nav.get_actor_bounds(False)
    assert min(extent.x, extent.y) > 2600, (center, extent)
    navigation = ue.NavigationSystemV1.get_navigation_system(world)
    assert navigation
    navigation.on_navigation_bounds_updated(nav)

    for index, xy in enumerate(((1620, -455), (2010, -455)), 1):
        start = spawn(ue.TargetPoint, f'Hero Start {index}', xy, FLOOR_Z + 105, -90, 'Entry')
        start.set_editor_property('tags', [GENERATED, ue.Name('WarriorSpawn')])

    boss_class = ue.load_class(None, '/Script/BoldersDemo.StatueBossCharacter')
    boss = spawn(boss_class, 'Caldera Knight', (1962, -1769), FLOOR_Z + 150, 76)
    encounter_class = ue.load_class(None, '/Script/BoldersDemo.VolcanoEncounter')
    encounter = spawn(encounter_class, 'Volcano Encounter', (1800, -1480))
    defeat_cue = ue.load_asset(f'{AUDIO}/SW_FoundryPartyDefeat')
    assert defeat_cue
    encounter.set_editor_properties(dict(boss=boss, guardian_class=ue.EditorAssetLibrary.load_blueprint_class(GUARDIAN),
                                         party_defeat_cue=defeat_cue))
    waves = (((1570, -1100), (2220, -1090)), ((1400, -1530), (2350, -1510), (2540, -1920)))
    for index, locations in enumerate(waves, 1):
        points = [spawn(ue.TargetPoint, f'Wave {index} Guardian {i + 1}', xy, FLOOR_Z + 95, 90, 'Encounter/Spawns')
                  for i, xy in enumerate(locations)]
        encounter.set_editor_property(('first_wave_spawns', 'second_wave_spawns')[index - 1], points)

    build_foundry_stations()
    build_furnace_air()
    architecture = runpy.run_path(str(Path(__file__).with_name('volcano_foundry_architecture.py')))
    architecture['author'](world, STATIONS, MATERIALS, FLOOR_Z, station_point)
    fixtures = runpy.run_path(str(Path(__file__).with_name('spread_foundry_fixtures.py')))
    fixtures['reposition'](world)

    EDITOR.set_level_viewport_camera_info(ue.Vector(1900, 1630, 5780), ue.Rotator(pitch=-51, yaw=-90))
    started = time.monotonic()
    rebuild_requested_at = None

    def save_navigation(_):
        nonlocal rebuild_requested_at
        now = time.monotonic()
        if now - started > 45.0:
            ue.unregister_slate_post_tick_callback(handle)
            ue.log_error('VOLCANO_COMBAT_NAVIGATION_TIMEOUT')
            if os.getenv('BOLDERS_VOLCANO_EXIT_ON_SAVE') == '1':
                ue.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
            return
        if ue.NavigationSystemV1.is_navigation_being_built_or_locked(world):
            return
        if rebuild_requested_at is None:
            ue.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
            rebuild_requested_at = now
            return
        if now - rebuild_requested_at < 1.0:
            return
        ue.unregister_slate_post_tick_callback(handle)
        assert LEVELS.save_current_level()
        ue.log('VOLCANO_COMBAT_SAVED')
        if os.getenv('BOLDERS_VOLCANO_EXIT_ON_SAVE') == '1':
            ue.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')

    handle = ue.register_slate_post_tick_callback(save_navigation)


try:
    build()
except Exception:
    if os.getenv('BOLDERS_VOLCANO_EXIT_ON_SAVE') == '1':
        ue.SystemLibrary.execute_console_command(EDITOR.get_editor_world(), 'QUIT_EDITOR')
    raise
