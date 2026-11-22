"""Shared Unreal asset and material helpers for the elemental attack scripts."""

import unreal as u


E = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/BoldersDemo"
EFFECTS = ROOT + "/Assets/Combat/Effects/Elemental"
COMBAT = ROOT + "/Assets/Combat"


def load(path):
    asset = E.load_asset(path)
    assert asset, path
    return asset


def save(asset):
    assert E.save_loaded_asset(asset, False), asset.get_name()
    return asset


def make(name, folder, cls, factory):
    path = f"{folder}/{name}"
    return load(path) if E.does_asset_exist(path) else TOOLS.create_asset(name, folder, cls, factory)


def material(name, shader, color, time="burst", blend=u.BlendMode.BLEND_ADDITIVE, particle=False, local=False):
    asset = make("M_" + name, EFFECTS, u.Material, u.MaterialFactoryNew())
    asset.set_editor_property("two_sided", True)
    asset.set_editor_property("blend_mode", blend)
    asset.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    M.delete_all_material_expressions(asset)
    node = M.create_material_expression(asset, u.MaterialExpressionCustom, 160, 0)
    if local:
        world = M.create_material_expression(asset, u.MaterialExpressionWorldPosition, -810, 0)
        position = M.create_material_expression(asset, u.MaterialExpressionTransformPosition, -500, 0)
        positions = u.MaterialPositionTransformSource
        position.set_editor_property("transform_source_type", positions.TRANSFORMPOSSOURCE_WORLD)
        position.set_editor_property("transform_type", positions.TRANSFORMPOSSOURCE_LOCAL)
        assert M.connect_material_expressions(world, "", position, "Input")
        inputs = {"LocalPosition": position}
    else:
        inputs = {"UV": M.create_material_expression(asset, u.MaterialExpressionTextureCoordinate, -500, 0)}
    if particle:
        asset.set_editor_property("used_with_niagara_sprites", True)
        asset.set_editor_property("used_with_niagara_ribbons", True)
        inputs["ParticleAlpha"] = M.create_material_expression(asset, u.MaterialExpressionParticleColor, -500, 380)
    if time != "static":
        inputs["Clock"] = M.create_material_expression(asset, u.MaterialExpressionTime, -500, 140)
    if time == "burst":
        for index, (parameter, default) in enumerate((("StartTime", 0.), ("Lifetime", .45))):
            scalar = M.create_material_expression(asset, u.MaterialExpressionScalarParameter, -500, 250 + index * 100)
            scalar.set_editor_property("parameter_name", parameter)
            scalar.set_editor_property("default_value", default)
            inputs[parameter] = scalar
    definitions = []
    for key in inputs:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", key)
        definitions.append(pin)
    node.set_editor_property("inputs", definitions)
    rgba = blend == u.BlendMode.BLEND_TRANSLUCENT
    node.set_editor_property("output_type", u.CustomMaterialOutputType.CMOT_FLOAT4 if rgba
                             else u.CustomMaterialOutputType.CMOT_FLOAT3)
    tint = f"float3({color[0]},{color[1]},{color[2]})"
    header = "float t=saturate((Clock-StartTime)/max(.03,Lifetime));" if time == "burst" else ""
    node.set_editor_property("code", header + shader.replace("TINT", tint))
    for key, source in inputs.items():
        assert M.connect_material_expressions(source, "A" if key == "ParticleAlpha" else "", node, key)
    assert M.connect_material_property(node, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    if rgba:
        alpha = M.create_material_expression(asset, u.MaterialExpressionComponentMask, 405, 120)
        alpha.set_editor_property("r", False)
        alpha.set_editor_property("g", False)
        alpha.set_editor_property("b", False)
        alpha.set_editor_property("a", True)
        assert M.connect_material_expressions(node, "", alpha, "")
        assert M.connect_material_property(alpha, "", u.MaterialProperty.MP_OPACITY)
    M.recompile_material(asset)
    return save(asset)
