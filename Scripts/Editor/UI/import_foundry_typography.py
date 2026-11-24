"""Import GPT Image lettering and its measured UV rectangles without changing pixels."""
import json
from pathlib import Path

import unreal as u


ROOT = "/Game/BoldersDemo/Assets/UI/FoundryTypography"
SOURCE = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / "ArtSources/UI/FoundryTypography"
ASSETS = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()


def load_atlas(sheet):
    name = f"T_FoundryText_{sheet}"
    path = f"{ROOT}/{name}"
    task = u.AssetImportTask()
    task.set_editor_properties(dict(automated=True, filename=(SOURCE / f"{sheet}.png").as_posix(),
                                    destination_path=ROOT, destination_name=name, replace_existing=True, save=False))
    TOOLS.import_asset_tasks([task])
    texture = ASSETS.load_asset(path)
    assert isinstance(texture, u.Texture2D) and task.imported_object_paths, name
    texture.set_editor_properties(dict(
        lod_group=u.TextureGroup.TEXTUREGROUP_UI,
        compression_settings=u.TextureCompressionSettings.TC_EDITOR_ICON,
        mip_gen_settings=u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        power_of_two_mode=u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION,
        resize_during_build_x=1024, resize_during_build_y=1024, never_stream=True,
        address_x=u.TextureAddress.TA_CLAMP, address_y=u.TextureAddress.TA_CLAMP,
    ))
    assert ASSETS.save_loaded_asset(texture), name
    return texture


def import_typography():
    definitions = json.loads((SOURCE / "Atlas.json").read_text(encoding="utf-8"))
    atlases = {sheet: load_atlas(sheet) for sheet in dict.fromkeys(row["sheet"] for row in definitions.values())}
    sprites = {}
    for label, row in definitions.items():
        sprite = u.FoundryTextSprite()
        sprite.set_editor_properties(dict(atlas=atlases[row["sheet"]], uv=u.Vector4(*row["uv"]), aspect=row["aspect"]))
        sprites[label] = sprite
    name = "DA_FoundryTypography"
    path = f"{ROOT}/{name}"
    if ASSETS.does_asset_exist(path):
        library = ASSETS.load_asset(path)
    else:
        factory = u.DataAssetFactory()
        asset_class = u.load_class(None, "/Script/BoldersDemo.FoundryTypography")
        assert asset_class, "Build BoldersDemoEditor before importing typography"
        factory.set_editor_property("data_asset_class", asset_class)
        library = TOOLS.create_asset(name, ROOT, None, factory)
    assert library, path
    library.set_editor_property("sprites", sprites)
    assert ASSETS.save_loaded_asset(library), path
    for role, role_text in (("Warrior", "近战剑盾"), ("Archer", "远程弓箭"), ("Mage", "远程魔法")):
        definition = ASSETS.load_asset(f"/Game/BoldersDemo/Assets/UI/ClassSelection/DA_Class_{role}")
        if definition and str(definition.get_editor_property("role_text")) != role_text:
            definition.set_editor_property("role_text", role_text)
            assert ASSETS.save_loaded_asset(definition), role
    u.log(f"FOUNDRY_TYPOGRAPHY_IMPORTED {len(sprites)} sprites")


import_typography()
