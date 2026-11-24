"""Place the four interactive route signs on the Volcano Combat island."""
import json
import math
from pathlib import Path

import unreal as u

MAP = "/Game/BoldersDemo/Assets/Maps/L_VolcanoCombat"
FOLDER = "Volcano Combat/Island/Way Signs"
PLACEMENTS = (
    ("Entry Way Sign", u.VolcanoWaySignKind.ENTRY, (-900, 3020, 1558), (-985, 2125)),
    ("Casting Road Way Sign", u.VolcanoWaySignKind.CASTING_ROAD, (-1535, 1690, 1558), (-2040, -740)),
    ("Core Crossing Way Sign", u.VolcanoWaySignKind.CORE, (-2470, -1500, 1558), (-810, -1380)),
    ("Cooling Basin Way Sign", u.VolcanoWaySignKind.COOLING, (2500, -2650, 1808), (2650, -2965)),
)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == "L_VolcanoCombat", MAP
types = [u.ObjectTypeQuery.OBJECT_TYPE_QUERY1]
require_aligned_art = any(actor.get_actor_label() == "Foundry Walkable Surfaces" for actor in actors.get_all_level_actors())
existing = {actor.get_actor_label(): actor for actor in actors.get_all_level_actors()
            if isinstance(actor, u.VolcanoWaySign) and str(actor.get_folder_path()) == FOLDER}

def find_ground(origin):
    x, y, z = origin
    offsets = ((0, 0), (-65, 0), (65, 0), (0, -65), (0, 65), (-100, -75), (100, 75))
    footprint = ((0, 0), (-24, 0), (24, 0), (0, -24), (0, 24))

    def sample(px, py, complex_trace):
        hit = u.SystemLibrary.line_trace_single_for_objects(world, u.Vector(px, py, z + 72),
            u.Vector(px, py, z - 92), types, complex_trace, [], u.DrawDebugTrace.NONE, True)
        if not hit:
            return None
        values = hit.to_tuple()
        if values[9] and values[9].actor_has_tag("WalkableGround") and values[7].z > (.55 if complex_trace else .89):
            return values[5].z
        return None

    for ox, oy in offsets:
        px, py = x + ox, y + oy
        physical = [sample(px + fx, py + fy, False) for fx, fy in footprint]
        if None in physical or max(physical) - min(physical) > 3:
            continue

        visible = [sample(px + fx, py + fy, True) for fx, fy in footprint]
        if (all(height is not None and abs(height - expected) < 18
                for height, expected in zip(visible, physical)) and max(visible) - min(visible) <= 4):
            # Recess the footing slightly so surface joints cannot reveal a gap beneath it.
            return u.Vector(px, py, min(visible) - 1)
        if not require_aligned_art:
            return u.Vector(px, py, min(physical) - 1)
    raise RuntimeError(f"No supported footing near {origin}")

def place_way_signs(save=True):
    survey = [(label, kind, find_ground(origin), destination) for label, kind, origin, destination in PLACEMENTS]
    report = {}
    for label, kind, position, destination in survey:
        yaw = math.degrees(math.atan2(destination[1] - position.y, destination[0] - position.x))
        rotation = u.Rotator(yaw=yaw)
        sign = existing.get(label) or actors.spawn_actor_from_class(u.VolcanoWaySign, position, rotation)
        assert sign, label
        sign.set_actor_location(position, False, True)
        sign.set_actor_rotation(rotation, True)
        sign.set_actor_label(label)
        sign.set_folder_path(u.Name(FOLDER))
        sign.set_editor_property("kind", kind)
        report[label] = [position.x, position.y, position.z, rotation.yaw]
    if save:
        assert levels.save_current_level()
    saved = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "FoundryHudRedesign"
    saved.mkdir(exist_ok=True)
    (saved / "WaySigns.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    u.log("FOUNDRY_WAY_SIGNS_PLACED")


if __name__ == "__main__":
    place_way_signs()
