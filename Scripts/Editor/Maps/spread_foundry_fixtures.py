"""Space the core arena's foundry fixtures and fit their supporting details to the stone floor."""
import json
from itertools import combinations
from math import atan2, cos, degrees, radians, sin
from pathlib import Path

import unreal as ue


MAP = 'L_VolcanoCombat'
FLOOR_HEIGHT = 1808.19
FLOOR_TAG = ue.Name('WalkableGround')
FOLDER = 'Volcano Combat/Foundry Fixtures'
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)

# Centers and headings in world space. The outer route's three fixtures already stand alone.
POSITIONS = {
    'West Vent': (420, -1330, 90),
    'West Lightning Orb': (570, -2020, 90),
    'West Pool': (965, -2130, -16),
    'West Barrel': (1350, -680, 24),
    'East Vent': (3120, -1600, -90),
    'East Lightning Orb': (3060, -710, -90),
    'East Pool': (3415, -515, 29),
    'East Barrel': (2110, -280, -12),
    'Southern Vent': (1400, -2670, 180),
    'Southern Lightning Orb': (2150, -2880, 180),
    'Southern Pool': (2535, -3020, -20),
    'Southern Barrel': (2990, -2300, 45),
}

SUPPORTS = {
    'West Lightning Orb': ('West Cooling Return', 'West Pump Seat', 'West Pump Wheel'),
    'East Vent': ('East Firebox Return', 'East Firebox Retaining Stone'),
    'Southern Vent': ('Southern Flue Return', 'Southern Flue Saddle', 'Southern Flue Baffle'),
}


def trace(world, position, end, ignored):
    return ue.SystemLibrary.line_trace_single(
        world, position, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
        ignored, ue.DrawDebugTrace.NONE, True)


def support(world, x, y, ignored):
    hit = trace(world, ue.Vector(x, y, FLOOR_HEIGHT + 120),
                ue.Vector(x, y, FLOOR_HEIGHT - 24), ignored)
    if not hit:
        raise RuntimeError(f'No arena floor under ({x:.0f}, {y:.0f}).')
    data = hit.to_tuple()
    if not data[9].actor_has_tag(FLOOR_TAG) or data[7].z < .96:
        raise RuntimeError(f'{data[9].get_actor_label()} blocks ({x:.0f}, {y:.0f}).')
    return data[5].z


def validate_footing(world, name, placement, ignored, hazard):
    x, y, yaw = placement
    if name.endswith('Pool'):
        radius = hazard.get_effect_radius()
        width, depth = radius * 1.175 + 5, radius * .795 + 5
    else:
        width, depth = (64, 54) if name.endswith('Vent') else (57, 57)
    c, s = cos(radians(yaw)), sin(radians(yaw))
    samples = [(0, 0), (width, 0), (-width, 0), (0, depth), (0, -depth)]
    samples.extend((i * .74 * width, j * .74 * depth) for i in (-1, 1) for j in (-1, 1))
    floors = [support(world, x + dx * c - dy * s, y + dx * s + dy * c, ignored)
              for dx, dy in samples]
    if max(floors) - min(floors) > 6:
        raise RuntimeError(f'{name} needs a level footing.')
    return ue.Vector(x, y, floors[0] + 2.0)


def reposition(world):
    if world.get_name() != MAP:
        raise RuntimeError(f'Open {MAP} before placing the foundry fixtures.')
    by_name = {actor.get_actor_label(): actor for actor in ACTORS.get_all_level_actors()}
    hazards = [actor for actor in by_name.values() if isinstance(actor, ue.VolcanoHazard)]
    details = [actor for name, actor in by_name.items()
               if 'Service Apron' in name or ' Conductor ' in name
               or any(name in parts for parts in SUPPORTS.values())]
    ignored = hazards + details
    centers = {name: validate_footing(world, name, placement, ignored, by_name[name])
               for name, placement in POSITIONS.items()}
    spacing = dict(centers)
    spacing.update((actor.get_actor_label(), actor.get_actor_location()) for actor in hazards
                   if actor.get_actor_label() not in centers)
    nearest = min(((b - a).length(), key_a, key_b)
                  for (key_a, a), (key_b, b) in combinations(spacing.items(), 2))
    if nearest[0] < 380:
        raise RuntimeError(f'Foundry fixtures are too close: {nearest}.')
    links = []
    for side in ('West', 'East', 'Southern'):
        orb = side + ' Lightning Orb'
        pool = side + ' Pool'
        start, end = centers[orb], centers[pool]
        reach = by_name[orb].get_effect_radius() + .65 * by_name[pool].get_effect_radius()
        distance = (end - start).length()
        if distance > reach or trace(world, start + ue.Vector(0, 0, 46),
                                     end + ue.Vector(0, 0, 35), ignored):
            raise RuntimeError(f'{side} conductor is outside clear reach.')
        links.append({'side': side, 'distance_cm': round(distance, 1), 'reach_cm': round(reach, 1)})

    for name, position in centers.items():
        actor = by_name[name]
        delta = position - actor.get_actor_location()
        actor.set_actor_location(position, False, True)
        actor.set_actor_rotation(ue.Rotator(yaw=POSITIONS[name][2]), True)
        area = name.split()[0]
        actor.set_folder_path(ue.Name(f'{FOLDER}/{area}'))
        for detail in SUPPORTS.get(name, ()):
            part = by_name.get(detail)
            if part:
                part.set_actor_location(part.get_actor_location() + delta, False, True)
                part.set_folder_path(ue.Name(f'{FOLDER}/{area}/Details'))

    for side in ('West', 'East', 'Southern'):
        pool = side + ' Pool'
        start, end = centers[side + ' Lightning Orb'], centers[pool]
        radius = by_name[pool].get_effect_radius()
        apron = by_name.get(side + ' Service Apron') or by_name[side + ' Basin Footing']
        apron.set_actor_label(side + ' Basin Footing')
        apron.set_actor_location(end - ue.Vector(0, 0, .15), False, True)
        apron.set_actor_rotation(ue.Rotator(yaw=POSITIONS[pool][2]), True)
        apron.set_actor_scale3d(ue.Vector((2.35 * radius + 16) / 100, (1.59 * radius + 16) / 100, 1))
        apron.set_folder_path(ue.Name(f'{FOLDER}/{side}/Details'))

        offset = end - start
        distance = offset.length()
        step = offset / distance
        first, last = 54., distance - (radius + 23)
        heading = degrees(atan2(offset.y, offset.x))
        length = (last - first) / 3
        for index in range(3):
            strip = by_name[f'{side} Conductor {index + 1}']
            strip.set_actor_location(start + step * (first + (index + .5) * length) +
                                     ue.Vector(0, 0, 1.6), False, True)
            strip.set_actor_rotation(ue.Rotator(yaw=heading), True)
            strip.set_actor_scale3d(ue.Vector((length - 4) / 100, .085, .029))
            strip.set_folder_path(ue.Name(f'{FOLDER}/{side}/Details'))

    return {'positions': {name: position.to_tuple() for name, position in spacing.items()},
            'min_spacing_cm': round(nearest[0], 1), 'nearest_pair': nearest[1:], 'conductor_links': links}


def run():
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    result = reposition(world)
    navigation = ue.NavigationSystemV1
    anchors = [actor for actor in ACTORS.get_all_level_actors() if actor.actor_has_tag('WarriorSpawn')]
    assert anchors, 'Missing island entrance start.'
    extent = ue.Vector(125, 125, 130)
    entrance = navigation.project_point_to_navigation(world, anchors[0].get_actor_location(), None, None, extent)
    assert entrance, 'The entrance is off the navigation mesh.'
    for name, coordinates in result['positions'].items():
        target = navigation.project_point_to_navigation(world, ue.Vector(*coordinates), None, None, extent)
        if not target:
            raise RuntimeError(f'No navigable approach to {name}.')
        path = navigation.find_path_to_location_synchronously(world, entrance, target)
        if not path or not path.is_valid() or path.is_partial():
            raise RuntimeError(f'No complete route from the entrance to {name}.')
    assert levels.save_current_level(), 'Could not save the foundry fixture layout.'
    location = Path(ue.Paths.project_saved_dir()) / 'FoundryHazards'
    location.mkdir(exist_ok=True)
    (location / 'layout.json').write_text(json.dumps(result, indent=2), encoding='utf8')
    print('FOUNDRY_LAYOUT_OK', len(result['positions']), result['min_spacing_cm'], result['conductor_links'])


if __name__ == '__main__':
    run()

