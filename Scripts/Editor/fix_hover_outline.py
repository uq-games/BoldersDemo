"""Repair only the existing hover material; do not rebuild UI or save the map."""
import unreal

path = '/Game/BoldersDemo/Assets/Shared/Materials/M_HoverOutline'
material = unreal.EditorAssetLibrary.load_asset(path)
if not isinstance(material, unreal.Material):
    raise RuntimeError('Missing hover material: ' + path)
if material.get_editor_property('material_domain') != unreal.MaterialDomain.MD_POST_PROCESS:
    raise RuntimeError('Hover material must be a post-process material')

before = material.get_editor_property('blendable_location')
# In UE 5.9 Before DOF precedes TSR/TAA; Before Bloom does not.
target = unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF
material.set_editor_property('blendable_location', target)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError('Could not save hover material')
assert material.get_editor_property('blendable_location') == target
unreal.log('HOVER_OUTLINE_FIXED: {} -> {}'.format(before, target))
