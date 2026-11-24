"""Import the four GPT Image plaques and build their shared world material."""
from pathlib import Path

import unreal as u


ROOT = "/Game/BoldersDemo/Assets/Environment/VolcanoCombat/WaySigns"
SOURCE = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / "ArtSources/Environment/VolcanoCombat/WaySigns"
VARIANTS = ("Entry", "CastingRoad", "Core", "Cooling")
ASSETS = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
MATERIALS = u.MaterialEditingLibrary


def import_texture(variant):
    name = f"T_WaySign_{variant}"
    path = f"{ROOT}/{name}"
    existing = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if existing:
        return existing
    task = u.AssetImportTask()
    task.set_editor_properties(dict(automated=True, filename=(SOURCE / f"{variant}.png").as_posix(),
        destination_path=ROOT, destination_name=name, replace_existing=False, save=False))
    TOOLS.import_asset_tasks([task])
    texture = ASSETS.load_asset(path)
    assert isinstance(texture, u.Texture2D) and task.imported_object_paths, path
    texture.set_editor_properties(dict(power_of_two_mode=u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION,
        resize_during_build_x=1024, resize_during_build_y=512, max_texture_size=1024,
        mip_gen_settings=u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE))
    assert ASSETS.save_loaded_asset(texture), path
    return texture


def create_material(default_texture):
    name = "M_FoundryWaySign"
    path = f"{ROOT}/{name}"
    if ASSETS.does_asset_exist(path):
        return ASSETS.load_asset(path)
    material = TOOLS.create_asset(name, ROOT, u.Material, u.MaterialFactoryNew())
    assert material, path
    material.set_editor_properties(dict(blend_mode=u.BlendMode.BLEND_MASKED,
        shading_model=u.MaterialShadingModel.MSM_UNLIT, two_sided=True, opacity_mask_clip_value=.57))
    art = MATERIALS.create_material_expression(material, u.MaterialExpressionTextureSampleParameter2D, -340, 0)
    art.set_editor_properties(dict(parameter_name="PlaqueArt", texture=default_texture))
    assert MATERIALS.connect_material_property(art, "RGB", u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert MATERIALS.connect_material_property(art, "A", u.MaterialProperty.MP_OPACITY_MASK)
    MATERIALS.recompile_material(material)
    assert ASSETS.save_loaded_asset(material), path
    return material


def create_instance(variant, parent, texture):
    name = f"MI_WaySign_{variant}"
    path = f"{ROOT}/{name}"
    if ASSETS.does_asset_exist(path):
        return
    instance = TOOLS.create_asset(name, ROOT, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    assert instance, path
    MATERIALS.set_material_instance_parent(instance, parent)
    assert MATERIALS.set_material_instance_texture_parameter_value(instance, "PlaqueArt", texture)
    MATERIALS.update_material_instance(instance)
    assert ASSETS.save_loaded_asset(instance), path


textures = {variant: import_texture(variant) for variant in VARIANTS}
parent = create_material(textures["Entry"])
for variant, texture in textures.items():
    create_instance(variant, parent, texture)
u.log("FOUNDRY_WAY_SIGN_ART_IMPORTED")
