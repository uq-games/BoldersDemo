"""Author the spatial cues and foundry materials for the standalone volcano arena."""

from pathlib import Path

import unreal as ue


ASSETS = "/Game/BoldersDemo/Assets/Environment/VolcanoCombat"
DESTINATION = ASSETS + "/Audio"
SOURCE = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / "ArtSources" / "Audio" / "VolcanoCombat"
CUES = (
    "SW_VolcanoEruption",
    "SW_VolcanoBarrelBlast",
    "SW_VolcanoOrbDischarge",
    "SW_VolcanoWaterSteam",
    "SW_VolcanoWaterFreeze",
    "SW_FoundryFurnaceAir_Loop",
    "SW_FoundryPartyDefeat",
)

POOL_SHADER = """
float2 p = UV * 2.0 - 1.0;
float2 extent = abs(p);
float wall = min(min(1.0 - extent.x, .62 - extent.y), (1.43 - extent.x - extent.y) * .707);
float nearEdge = exp(-max(0.0, wall - .009) * 40.0);
float ripple = sin(17.0 * p.x - 22.0 * p.y + sin(p.x * 9.0 + p.y * 6.0 + Clock * .42) * 1.32 - Clock * .8);
float crossRipple = sin(12.0 * p.x + 16.0 * p.y + Clock * .59);
float reflectedLight = exp(-2.2 * (p.x + .31) * (p.x + .31) - 5.6 * (p.y + .17) * (p.y + .17));
float reflection = smoothstep(.30, 1.27, ripple + .56 * crossRipple) * reflectedLight;
float3 water = float3(.018, .029, .031);
water += lerp(float3(.036, .022, .012), float3(.025, .040, .042), smoothstep(-.4, .4, p.y)) * nearEdge;
water += float3(.036, .047, .043) * reflection;
return float4(water, .91);
"""

APRON_SHADER = """
float2 p = abs(UV - .5) * 2.0;
float edge = max(max(p.x, p.y), (p.x + p.y) / 1.81);
float2 slab = abs(frac(UV * float2(5.0, 2.0)) - .5);
float recess = smoothstep(.455, .483, max(slab.x, slab.y));
float bevel = smoothstep(.85, .885, edge) * (1.0 - smoothstep(.94, .99, edge));
float bridle = exp(-abs(edge - .87) * 142.0);
float stone = sin(UV.x * 112.0 + UV.y * 32.0) * cos(UV.y * 160.0 - UV.x * 20.0);
float3 surface = float3(.093, .075, .064) + stone * .007 - recess * .023;
surface = lerp(surface, float3(.059, .054, .051), bevel);
surface += bridle * float3(.032, .023, .012);
return float4(surface, 1.0 - step(1.0, edge));
"""

ARENA_STONE_SHADER = """
float3 p = World * .017;
float grain = sin(p.x * 7.2 + p.y * 5.3 + p.z * 6.0) * sin(p.y * 8.3 - p.x * 3.8 + p.z * 2.4);
float variation = sin(p.x * .43 + p.z * .28) * sin(p.y * .52 - p.z * .33);
float facing = saturate(pow(abs(Normal.z), .65));
float3 walls = float3(.061, .057, .054);
float3 basalt = float3(.120, .115, .109);
return lerp(walls, basalt, facing) + grain * .0022 + variation * .008;
"""


def get_or_create_material(name):
    folder = ASSETS + "/Materials"
    path = f"{folder}/{name}"
    existing = ue.load_asset(path) if ue.EditorAssetLibrary.does_asset_exist(path) else None
    if existing:
        return existing, False
    material = ue.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, ue.Material, ue.MaterialFactoryNew())
    assert material, path
    return material, True


def constant_material(name, color, roughness, metallic):
    material, created = get_or_create_material(name)
    if not created:
        return
    materials = ue.MaterialEditingLibrary
    shade = materials.create_material_expression(material, ue.MaterialExpressionConstant3Vector, -260, -60)
    shade.set_editor_property("constant", ue.LinearColor(*color, 1.0))
    assert materials.connect_material_property(shade, "", ue.MaterialProperty.MP_BASE_COLOR)
    for y, value, channel in ((95, roughness, ue.MaterialProperty.MP_ROUGHNESS),
                               (190, metallic, ue.MaterialProperty.MP_METALLIC)):
        scalar = materials.create_material_expression(material, ue.MaterialExpressionConstant, -260, y)
        scalar.set_editor_property("r", value)
        assert materials.connect_material_property(scalar, "", channel)
    materials.recompile_material(material)
    assert ue.EditorAssetLibrary.save_loaded_asset(material, False), name


def build_foundry():
    constant_material("M_FoundryBasalt", (.079, .068, .064), .87, 0.0)
    constant_material("M_FoundryIron", (.121, .094, .065), .54, .61)
    build_arena_stone()
    apron, created = get_or_create_material("M_FoundryApron")
    if not created:
        return
    apron.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
    materials = ue.MaterialEditingLibrary
    uv = materials.create_material_expression(apron, ue.MaterialExpressionTextureCoordinate, -495, -30)
    custom = materials.create_material_expression(apron, ue.MaterialExpressionCustom, -267, -30)
    pin = ue.CustomInput()
    pin.set_editor_property("input_name", "UV")
    custom.set_editor_properties(dict(inputs=[pin], code=APRON_SHADER,
                                      output_type=ue.CustomMaterialOutputType.CMOT_FLOAT4))
    assert materials.connect_material_expressions(uv, "", custom, "UV")
    alpha = materials.create_material_expression(apron, ue.MaterialExpressionComponentMask, -47, 85)
    alpha.set_editor_properties(dict(r=False, g=False, b=False, a=True))
    assert materials.connect_material_expressions(custom, "", alpha, "")
    assert materials.connect_material_property(custom, "", ue.MaterialProperty.MP_BASE_COLOR)
    assert materials.connect_material_property(alpha, "", ue.MaterialProperty.MP_OPACITY_MASK)
    rough = materials.create_material_expression(apron, ue.MaterialExpressionConstant, -47, 195)
    rough.set_editor_property("r", .83)
    assert materials.connect_material_property(rough, "", ue.MaterialProperty.MP_ROUGHNESS)
    materials.recompile_material(apron)
    assert ue.EditorAssetLibrary.save_loaded_asset(apron, False)
    ue.log("VOLCANO_FOUNDRY_MATERIALS_SAVED")


def build_arena_stone():
    stone, created = get_or_create_material("M_FoundryArenaStone")
    materials = ue.MaterialEditingLibrary
    if not created:
        base = materials.get_material_property_input_node(stone, ue.MaterialProperty.MP_BASE_COLOR)
        if isinstance(base, ue.MaterialExpressionCustom) and base.get_editor_property("code") != ARENA_STONE_SHADER:
            base.set_editor_property("code", ARENA_STONE_SHADER)
            materials.recompile_material(stone)
            assert ue.EditorAssetLibrary.save_loaded_asset(stone, False)
        return
    inputs = (
        ("World", materials.create_material_expression(stone, ue.MaterialExpressionWorldPosition, -480, -70)),
        ("Normal", materials.create_material_expression(stone, ue.MaterialExpressionPixelNormalWS, -480, 70)),
    )
    custom = materials.create_material_expression(stone, ue.MaterialExpressionCustom, -200, -40)
    pins = []
    for key, _ in inputs:
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", key)
        pins.append(pin)
    custom.set_editor_properties(dict(inputs=pins, code=ARENA_STONE_SHADER,
                                      output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3))
    for key, expression in inputs:
        assert materials.connect_material_expressions(expression, "", custom, key)
    assert materials.connect_material_property(custom, "", ue.MaterialProperty.MP_BASE_COLOR)
    roughness = materials.create_material_expression(stone, ue.MaterialExpressionConstant, -85, 112)
    roughness.set_editor_property("r", .86)
    assert materials.connect_material_property(roughness, "", ue.MaterialProperty.MP_ROUGHNESS)
    materials.recompile_material(stone)
    assert ue.EditorAssetLibrary.save_loaded_asset(stone, False)
    ue.log("VOLCANO_ARENA_STONE_SAVED")


def build_water():
    name = "M_FoundryCoolingBasin"
    material, created = get_or_create_material(name)
    edit = ue.MaterialEditingLibrary
    if not created:
        shade = edit.get_material_property_input_node(material, ue.MaterialProperty.MP_EMISSIVE_COLOR)
        assert isinstance(shade, ue.MaterialExpressionCustom), name
        if shade.get_editor_property("code") != POOL_SHADER:
            shade.set_editor_property("code", POOL_SHADER)
            edit.recompile_material(material)
            assert ue.EditorAssetLibrary.save_loaded_asset(material, False), name
            ue.log("VOLCANO_WATER_MATERIAL_SAVED")
        return
    material.set_editor_properties(dict(
        blend_mode=ue.BlendMode.BLEND_TRANSLUCENT,
        shading_model=ue.MaterialShadingModel.MSM_UNLIT,
        two_sided=True,
    ))
    uv = edit.create_material_expression(material, ue.MaterialExpressionTextureCoordinate, -490, -90)
    clock = edit.create_material_expression(material, ue.MaterialExpressionTime, -490, 70)
    custom = edit.create_material_expression(material, ue.MaterialExpressionCustom, -240, -75)
    inputs = []
    for key in ("UV", "Clock"):
        pin = ue.CustomInput()
        pin.set_editor_property("input_name", key)
        inputs.append(pin)
    custom.set_editor_properties(dict(
        inputs=inputs, code=POOL_SHADER, output_type=ue.CustomMaterialOutputType.CMOT_FLOAT4,
    ))
    assert edit.connect_material_expressions(uv, "", custom, "UV")
    assert edit.connect_material_expressions(clock, "", custom, "Clock")
    mask = edit.create_material_expression(material, ue.MaterialExpressionComponentMask, -30, 155)
    mask.set_editor_properties(dict(r=False, g=False, b=False, a=True))
    assert edit.connect_material_expressions(custom, "", mask, "")
    assert edit.connect_material_property(custom, "", ue.MaterialProperty.MP_EMISSIVE_COLOR)
    assert edit.connect_material_property(mask, "", ue.MaterialProperty.MP_OPACITY)
    edit.recompile_material(material)
    assert ue.EditorAssetLibrary.save_loaded_asset(material, False), name
    ue.log("VOLCANO_WATER_MATERIAL_SAVED")


def import_sounds(names):
    tasks = []
    for name in names:
        source = SOURCE / f"{name}.wav"
        assert source.is_file(), source
        task = ue.AssetImportTask()
        task.set_editor_properties(dict(
            filename=str(source), destination_path=DESTINATION, automated=True,
            replace_existing=True, save=True,
        ))
        tasks.append(task)
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in names:
        sound = ue.load_asset(f"{DESTINATION}/{name}")
        assert isinstance(sound, ue.SoundWave), name
        if name == "SW_FoundryFurnaceAir_Loop":
            sound.set_editor_property("looping", True)
        assert ue.EditorAssetLibrary.save_loaded_asset(sound, False), name
    ue.log(f"VOLCANO_AUDIO_IMPORTED {len(names)}")


def run():
    import_sounds(CUES)
    build_water()
    build_foundry()


if __name__ == "__main__":
    run()
