"""Author the warrior's shared blade material and the five montage trail windows."""
import unreal as u

ROOT = "/Game/BoldersDemo/Assets/Characters/Warrior"
M = u.MaterialEditingLibrary
ANIM = u.AnimationLibrary
MATERIAL_PATH = ROOT + "/Effects/M_WarriorBladeTrail"
TRACK = "Blade VFX"

# Times relative to AttackHit: lead, follow-through, fade, tip width.
STYLES = {
    "Chop": (.115, .055, .115, 7.),
    "Slice_Horizontal": (.095, .105, .125, 7.),
    "Stab": (.085, .07, .08, 6.),
    "GreatswordSweep": (.13, .095, .17, 15.),
    "GreatswordChop": (.135, .085, .155, 14.),
}


def build_material():
    material = u.EditorAssetLibrary.load_asset(MATERIAL_PATH) if u.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH) else None
    if material is None:
        material = u.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_WarriorBladeTrail", ROOT + "/Effects", u.Material, u.MaterialFactoryNew())
    assert material, MATERIAL_PATH
    material.set_editor_property("two_sided", True)
    material.set_editor_property("enable_responsive_aa", True)
    material.set_editor_property("blend_mode", u.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    M.delete_all_material_expressions(material)
    uv = M.create_material_expression(material, u.MaterialExpressionTextureCoordinate, -440, -100)
    color = M.create_material_expression(material, u.MaterialExpressionVertexColor, -440, 60)
    tint = M.create_material_expression(material, u.MaterialExpressionVectorParameter, -440, 230)
    tint.set_editor_property("parameter_name", "TrailTint")
    tint.set_editor_property("default_value", u.LinearColor(1., .65, .29, 1.))
    shader = M.create_material_expression(material, u.MaterialExpressionCustom, -80, 0)
    shader.set_editor_property("description", "Blade profile / age / tip streak")
    bindings = [("UV", uv, ""), ("Life", color, "A"), ("Tint", tint, "RGB")]
    definitions = []
    for name, _, _ in bindings:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", name)
        definitions.append(pin)
    shader.set_editor_property("inputs", definitions)
    shader.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT3)
    shader.set_editor_property("code", """
        const float age = saturate(UV.y);
        const float heavy = saturate((Tint.r - Tint.b) * 2.65);
        if (UV.x >= 1.5)
        {
            const float cross = abs((UV.x - 2.5) * 2);
            const float tip = pow(saturate(1 - cross), 2);
            const float white = pow(saturate(1 - cross), 9);
            return Life * ((1.0 + .72 * heavy) * tip * Tint + 1.75 * white * lerp(Tint, 1.0, .72));
        }
        const float blade = saturate(UV.x);
        const float body = smoothstep(.17, .76, blade) * (1 - smoothstep(.89, 1., blade));
        const float filament = exp(-lerp(67.,36.,heavy) * abs(blade - .95));
        const float fine = exp(-79 * abs(blade - .76 - .043 * sin(12 * age)));
        return Life * (lerp(Tint, float3(.23,.46,.85), (1-heavy)*.58) * ((.48 + .57*heavy)*body + .23*heavy*fine)
            + (1.9 + .55*heavy) * lerp(Tint, 1.0, .69) * filament);
    """)
    for name, expr, output in bindings:
        assert M.connect_material_expressions(expr, output, shader, name), name
    assert M.connect_material_property(shader, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    M.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def author_montages(material):
    for name, (lead, follow, fade, tip_width) in STYLES.items():
        montage = u.load_asset(ROOT + "/Animations/Montages/AM_Warrior_" + name)
        assert montage, name
        events = ANIM.get_animation_notify_events(montage)
        hits = [e for e in events if str(e.get_editor_property("notify_name")) == "AttackHit"]
        assert len(hits) == 1, (name, len(hits))
        hit = ANIM.get_anim_notify_event_trigger_time(hits[0])
        start = max(.012, hit - lead)
        end = min(montage.get_play_length() - .012, hit + follow)
        if TRACK not in [str(track) for track in ANIM.get_animation_notify_track_names(montage)]:
            ANIM.add_animation_notify_track(montage, TRACK, u.LinearColor(1., .54, .15, 1.))
        ANIM.remove_animation_notify_events_by_name(montage, "Weapon Trail")
        notify = ANIM.add_animation_notify_state_event(montage, TRACK, start, end - start, u.AnimNotifyState_WeaponTrail)
        assert notify, name
        great = name.startswith("Greatsword")
        root = (68. if great else 34.) if name != "Stab" else 95.
        notify.set_editor_property("blade_root", u.Vector(0., 0., root))
        notify.set_editor_property("blade_tip", u.Vector(0., 0., 194. if great else 139.))
        notify.set_editor_property("fade_time", fade)
        notify.set_editor_property("tip_width", tip_width)
        notify.set_editor_property("tint", u.LinearColor(1., .59, .23, 1.) if great else u.LinearColor(.73, .88, 1., 1.))
        notify.set_editor_property("trail_material", material)
        assert u.EditorAssetLibrary.save_loaded_asset(montage, False), name
        u.log(f"BLADE_TRAIL_AUTHORED {name} {start:.3f}-{end:.3f} hit={hit:.3f} fade={fade:.3f}")


if __name__ == "__main__":
    author_montages(build_material())
