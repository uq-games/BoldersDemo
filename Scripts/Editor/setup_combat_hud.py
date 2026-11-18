"""Import the combat HUD artwork and configure the existing HUD blueprint."""
from pathlib import Path

import unreal


SOURCE = Path(unreal.Paths.project_dir()) / "ArtSources/UI/CombatHUD"
CONTENT = "/Game/BoldersDemo"
DESTINATION = CONTENT + "/Assets/UI/CombatHUD"


def import_texture(source, name):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / source)
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(DESTINATION + "/" + name)
    assert texture, source
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("max_texture_size", 1024)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def region(size, rect):
    width, height = size
    left, top, right, bottom = rect
    return unreal.Vector4(left / width, top / height, right / width, bottom / height)


def loadout(role, size, slots):
    row = unreal.CombatHUDLoadout()
    row.set_editor_property("character_class", getattr(unreal, role + "Character"))
    row.set_editor_property("atlas", import_texture(role + "Icons.png", "T_" + role + "AttackIcons"))
    icons = []
    for icon_id, rect, available in slots:
        icon = unreal.CombatHUDIcon()
        icon.set_editor_property("id", unreal.Name(icon_id))
        icon.set_editor_property("uv", region(size, rect))
        icon.set_editor_property("available", available)
        icons.append(icon)
    row.set_editor_property("icons", icons)
    return row


def apply():
    frame = import_texture("HealthFrame.png", "T_CombatHealthFrame")
    rows = [
        loadout("Warrior", (1774, 887), (
            ("SwordShield", (28, 27, 859, 860), True),
            ("Greatsword", (915, 27, 1746, 860), False),
        )),
        loadout("Archer", (2172, 724), (
            ("NormalArrow", (30, 91, 549, 605), True),
            ("RicochetArrow", (561, 91, 1080, 605), False),
            ("ExplosiveArrow", (1092, 91, 1611, 605), False),
            ("FreezingArrow", (1623, 91, 2142, 605), False),
        )),
        loadout("Mage", (2172, 724), (
            ("Water", (15, 107, 527, 615), False),
            ("Lightning", (559, 107, 1070, 615), False),
            ("Fire", (1102, 107, 1614, 615), False),
            ("Ice", (1646, 107, 2157, 615), False),
        )),
    ]
    hud = unreal.load_asset(CONTENT + "/Blueprints/UI/BP_BoldersHUD")
    defaults = unreal.get_default_object(hud.generated_class())
    defaults.set_editor_property("health_bar_frame", frame)
    defaults.set_editor_property("health_bar_frame_uv", region((2172, 724), (48, 310, 2126, 410)))
    defaults.set_editor_property("health_bar_fill_bounds", unreal.Vector4(.017, .22, .983, .77))
    defaults.set_editor_property("loadouts", rows)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud)
    assert unreal.EditorAssetLibrary.save_loaded_asset(hud, False)
    unreal.log("COMBAT_HUD_CONFIGURED")


if __name__ == "__main__":
    apply()
