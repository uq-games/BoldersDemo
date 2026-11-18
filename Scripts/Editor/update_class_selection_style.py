"""Import the card artwork and apply it to the existing Unreal class selection."""
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir())
SOURCE = ROOT / "ArtSources/UI/ClassSelection/Style"
ASSETS = "/Game/BoldersDemo/Assets/UI/ClassSelection"
STYLE = ASSETS + "/Style"
BLUEPRINTS = "/Game/BoldersDemo/Blueprints/UI/ClassSelection"
LIBRARY = unreal.EditorAssetLibrary


def import_asset(source, name):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / source)
    task.destination_path = STYLE
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(STYLE + "/" + name)
    if not asset:
        raise RuntimeError(f"Unable to import {source}")
    if isinstance(asset, unreal.Texture2D):
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        asset.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assert LIBRARY.save_loaded_asset(asset, False), name
    return asset


def tune_character_preview():
    preview = unreal.load_asset(BLUEPRINTS + "/BP_CharacterPreview")
    defaults = unreal.get_default_object(preview.generated_class())
    mesh = defaults.get_editor_property("character_mesh")
    mesh.set_editor_property("relative_rotation", unreal.Rotator(yaw=-161))
    camera = defaults.get_editor_property("capture")
    position = camera.get_editor_property("relative_location")
    camera.set_editor_property("relative_rotation", unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(0, 0, 93)))
    camera.set_editor_property("fov_angle", 24.5)
    assert LIBRARY.save_loaded_asset(preview, False)
    for role, yaw, fov in (("Warrior", 13.0, 0.0), ("Archer", 0.0, 0.0), ("Mage", 0.0, 2.8)):
        definition = unreal.load_asset(ASSETS + "/DA_Class_" + role)
        definition.set_editor_property("preview_yaw_offset", yaw)
        definition.set_editor_property("preview_fov_offset", fov)
        definition.set_editor_property("pedestal_scale", unreal.Vector(.38, .32, .35))
        assert LIBRARY.save_loaded_asset(definition, False)


def apply_captions(catalog):
    atlas = import_asset("ClassCaptions.png", "T_SelectionClassCaptions")
    catalog.set_editor_property("caption_atlas", atlas)
    assert LIBRARY.save_loaded_asset(catalog, False)

    # The crop centers follow the artwork while the size and baseline are shared.
    top, width, height = 209, 620, 289
    for role, left in (("Warrior", 65), ("Archer", 788), ("Mage", 1492)):
        definition = unreal.load_asset(ASSETS + "/DA_Class_" + role)
        definition.set_editor_property("caption_uv", unreal.Vector4(
            left / 2172, top / 724, (left + width) / 2172, (top + height) / 724))
        assert LIBRARY.save_loaded_asset(definition, False)


def apply():
    assets = {
        "card_frame": import_asset("PlainNormal.png", "T_SelectionCardPlainNormal"),
        "selected_card_frame": import_asset("PlainSelected.png", "T_SelectionCardPlainSelected"),
        "confirm_plate": import_asset("Controls.png", "T_SelectionTextControls"),
    }
    import_asset("Labels.png", "T_SelectionTextLabels")
    import_asset("Accents.png", "T_SelectionAccents")
    import_asset("PlayerSlots.png", "T_SelectionPlayerSlots")

    catalog = unreal.load_asset(ASSETS + "/DA_ClassSelectionCatalog")
    for name, asset in assets.items():
        catalog.set_editor_property(name, asset)
    apply_captions(catalog)
    tune_character_preview()
    unreal.SystemLibrary.execute_console_command(None, "Bolders.LayoutClassSelection")
    unreal.log("CLASS_SELECTION_STYLE_IMPORTED")


if __name__ == "__main__":
    apply()
