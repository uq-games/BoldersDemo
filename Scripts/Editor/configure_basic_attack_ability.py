"""Move the ability through UE's asset API and configure a warrior Data Asset."""
import unreal

old = '/Game/BoldersDemo/Blueprints/Combat/GA_BasicAttack'
new = '/Game/BoldersDemo/Blueprints/Abilities/GA_BasicAttack'
data_path = '/Game/BoldersDemo/Assets/Combat/DA_WarriorAbilities'
parent = unreal.load_class(None, '/Script/BoldersDemo.BasicAttackAbility')
data_class = unreal.load_class(None, '/Script/BoldersDemo.CharacterAbilityData')
assert parent and data_class
if not unreal.EditorAssetLibrary.does_asset_exist(new):
    assert unreal.EditorAssetLibrary.does_asset_exist(old), 'Existing ability was not found'
    assert unreal.EditorAssetLibrary.rename_asset(old, new), 'Could not move ability asset'
ability = unreal.load_asset(new)
assert ability and unreal.MathLibrary.class_is_child_of(ability.generated_class(), parent)
unreal.BlueprintEditorLibrary.compile_blueprint(ability)
assert unreal.EditorAssetLibrary.save_loaded_asset(ability, False)

data = unreal.load_asset(data_path)
if data is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', data_class)
    data = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'DA_WarriorAbilities', '/Game/BoldersDemo/Assets/Combat', data_class, factory)
    assert data
    data.modify()
    data.set_editor_property('basic_attack', ability.generated_class())
    data.set_editor_property('basic_attack_level', 1)
    assert unreal.EditorAssetLibrary.save_loaded_asset(data, False)
else:
    assert data.get_class() == data_class, 'Existing Data Asset has a different class'
    assert data.get_editor_property('basic_attack'), 'Existing Data Asset needs a configured basic attack'

warrior = unreal.load_asset('/Game/BoldersDemo/Blueprints/Characters/Warrior/BP_Warrior')
assert warrior
combat = unreal.get_default_object(warrior.generated_class()).get_component_by_class(unreal.CharacterCombatComponent)
assert combat
combat.modify()
combat.set_editor_property('ability_data', data)
unreal.BlueprintEditorLibrary.compile_blueprint(warrior)
assert unreal.EditorAssetLibrary.save_loaded_asset(warrior, False)
actual = unreal.get_default_object(warrior.generated_class()).get_component_by_class(unreal.CharacterCombatComponent).get_editor_property('ability_data')
assert actual == data
print('WARRIOR_ABILITY_DATA_CONFIGURED', actual.get_path_name(), data.get_editor_property('basic_attack').get_path_name())
