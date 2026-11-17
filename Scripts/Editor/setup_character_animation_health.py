"""Run after Bolders.BuildMushroomAnimation. Saves project-owned defaults and TestMap instances."""
import unreal
from pathlib import Path

unreal.SystemLibrary.execute_console_command(None, 'Bolders.BuildMushroomAnimation')
# Reconstruct inherited Speed references after introducing the shared animation parent.
unreal.SystemLibrary.execute_console_command(None, 'Bolders.BuildWarriorAnimation')
configs = [
 ('/Game/BoldersDemo/Blueprints/Characters/Warrior/BP_Warrior', '/Game/BoldersDemo/Blueprints/Characters/Warrior/ABP_Warrior', None),
 ('/Game/BoldersDemo/Blueprints/Characters/Enemy/BP_EnemyMushroom', '/Game/BoldersDemo/Blueprints/Characters/Enemy/ABP_EnemyMushroom', '/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Animations/AM_Mushroom_Hit')]
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for bp_path, anim_path, hit_path in configs:
 bp = unreal.load_asset(bp_path)
 anim = unreal.load_asset(anim_path)
 assert bp and anim
 anim_class = anim.generated_class()
 hit = unreal.load_asset(hit_path) if hit_path else None
 cdo = unreal.get_default_object(bp.generated_class())
 cdo.modify()
 cdo.set_editor_property('animation_blueprint_class', anim_class)
 if hit: cdo.set_editor_property('hit_reaction_montage', hit)
 mesh = cdo.get_component_by_class(unreal.SkeletalMeshComponent)
 mesh.modify()
 mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT)
 mesh.set_anim_instance_class(anim_class)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
 for actor in unreal.GameplayStatics.get_all_actors_of_class(world, bp.generated_class()):
  actor.modify()
  m = actor.get_component_by_class(unreal.SkeletalMeshComponent)
  m.modify();m.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT);m.set_anim_instance_class(anim_class)
  m.set_update_animation_in_editor(True)
 print('ANIM_CONFIGURED',bp_path,anim_class)

task = unreal.AssetImportTask()
task.filename = str(Path(unreal.Paths.project_dir()) / 'ArtSources/UI/HealthBarFrame.png')
task.destination_path = '/Game/BoldersDemo/Assets/UI/Health'
task.destination_name = 'T_HealthBarFrame'
task.automated = True; task.save = True; task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/BoldersDemo/Assets/UI/Health/T_HealthBarFrame')
assert texture
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
hud = unreal.load_asset('/Game/BoldersDemo/Blueprints/UI/BP_BoldersHUD')
unreal.get_default_object(hud.generated_class()).set_editor_property('health_bar_frame',texture)
unreal.BlueprintEditorLibrary.compile_blueprint(hud)
assert unreal.EditorAssetLibrary.save_loaded_asset(hud,False)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('ANIMATION_HEALTH_SETUP_DONE')
