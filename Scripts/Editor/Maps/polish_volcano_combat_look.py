"""Tune the existing island lighting and ambience from its fixed gameplay camera."""
import unreal as u


FOLDER = "Volcano Combat/Lighting/Final Polish"
LANDMARK_LIGHTS = (
    ("Entry Copper Light", (-1050, 3150, 1735), (1.0, .62, .32), 3.2, 660),
    ("Casting Road Furnace Light", (-2020, -930, 1800), (1.0, .59, .31), 3.4, 840),
    ("Core Forge Light", (1860, -1820, 2110), (1.0, .67, .39), 3.6, 820),
    ("Cooling Trough Bounce", (2600, -2880, 1980), (.28, .72, 1.0), 2.6, 680),
)


def tune_island():
    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    level = u.get_editor_subsystem(u.LevelEditorSubsystem)
    world = editor.get_editor_world()
    assert world and world.get_name() == "L_VolcanoCombat", "Open L_VolcanoCombat before running the polish script"
    placed = {actor.get_actor_label(): actor for actor in actors.get_all_level_actors()}

    grade = placed["Arena Color and Exposure"]
    settings = grade.get_editor_property("settings")
    settings.set_editor_properties(dict(
        override_auto_exposure_bias=True, auto_exposure_bias=-.14,
        override_color_gain=True, color_gain=u.Vector4(.94, 1.011, 1.085, 1.0),
        override_color_saturation=True, color_saturation=u.Vector4(.85, .92, .97, .96),
        override_bloom_intensity=True, bloom_intensity=.23,
    ))
    grade.set_editor_property("settings", settings)
    sun = max((actor.get_component_by_class(u.DirectionalLightComponent)
               for actor in placed.values() if isinstance(actor, u.DirectionalLight)),
              key=lambda component: component.get_editor_property("intensity"))
    sun.set_light_color(u.LinearColor(1.0, .96, .91, 1.0))

    for name, location, color, intensity, radius in LANDMARK_LIGHTS:
        light = placed.get(name) or actors.spawn_actor_from_class(u.PointLight, u.Vector(*location))
        assert isinstance(light, u.PointLight), name
        light.set_actor_label(name)
        light.set_folder_path(u.Name(FOLDER))
        light.set_actor_location(u.Vector(*location), False, True)
        component = light.get_component_by_class(u.PointLightComponent)
        component.set_editor_properties(dict(
            mobility=u.ComponentMobility.MOVABLE, cast_shadows=False,
            use_inverse_squared_falloff=False, indirect_lighting_intensity=0.0,
        ))
        component.set_light_color(u.LinearColor(*color, 1.0))
        component.set_intensity(intensity)
        component.set_attenuation_radius(radius)

    furnace = placed["Furnace Air"].get_component_by_class(u.AudioComponent)
    furnace.set_editor_property("volume_multiplier", .56)
    assert level.save_current_level()
    u.log("VOLCANO_COMBAT_FINAL_LOOK_SAVED")


tune_island()
