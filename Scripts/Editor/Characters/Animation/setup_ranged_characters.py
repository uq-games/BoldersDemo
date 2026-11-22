"""Build project-owned archer/mage defaults. Run after compiling the C++ module."""
import unreal
from pathlib import Path
import shutil

E = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
root = '/Game/BoldersDemo'
unreal.SystemLibrary.execute_console_command(None, 'Bolders.BuildRangedAnimation')

def blueprint(path, parent):
    result = unreal.load_asset(path)
    if not result:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/BoldersDemo.' + parent))
        result = tools.create_asset(path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], unreal.Blueprint, factory)
    assert result
    return result

def defaults(bp):
    return unreal.get_default_object(bp.generated_class())

def save(asset):
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    assert E.save_loaded_asset(asset, False)

warrior = defaults(unreal.load_asset(root + '/Blueprints/Characters/Warrior/BP_Warrior'))
warrior_mesh = warrior.get_editor_property('mesh')
warrior_cap = warrior.get_editor_property('capsule_component')
query = warrior.get_component_by_class(unreal.CharacterCombatComponent).get_editor_property('attack_position_query')

# Shared projectile parent, independent visual/configuration children.
arrow = blueprint(root + '/Blueprints/Combat/Projectiles/BP_ArrowProjectile', 'AttackProjectile')
arrow_mesh = defaults(arrow).get_component_by_class(unreal.StaticMeshComponent)
arrow_mesh.set_static_mesh(unreal.load_asset(root + '/Assets/Characters/Archer/Weapons/SM_arrow_bow'))
# The actual mesh tip is +Y (62 cm); align it with +X and put the tip at the collision center.
arrow_mesh.set_relative_rotation(unreal.Rotator(yaw=-90.), False, False)
arrow_mesh.set_relative_location(unreal.Vector(-43.4, 0, 0), False, False)
arrow_mesh.set_relative_scale3d(unreal.Vector(.7, .7, .7))
save(arrow)

magic = blueprint(root + '/Blueprints/Combat/Projectiles/BP_MagicProjectile', 'AttackProjectile')
material_path = root + '/Assets/Combat/Projectiles/M_MagicBolt'
material = unreal.load_asset(material_path)
if not material:
    material = tools.create_asset('M_MagicBolt', root + '/Assets/Combat/Projectiles', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -200, 0)
    color.set_editor_property('constant', unreal.LinearColor(.2, 1.4, 4.0, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save(material)
magic_mesh = defaults(magic).get_component_by_class(unreal.StaticMeshComponent)
magic_mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
magic_mesh.set_relative_scale3d(unreal.Vector(.16, .11, .11))
magic_mesh.set_material(0, material)
movement = defaults(magic).get_component_by_class(unreal.ProjectileMovementComponent)
movement.set_editor_property('initial_speed', 900.)
movement.set_editor_property('max_speed', 900.)
save(magic)

classes = []
for role, projectile, weapon_mesh, socket, reach, damage in [
    ('Archer', arrow, 'SM_bow', 'Bow_L', 750., 20.),
    ('Mage', magic, 'SM_staff', 'Staff_R', 650., 25.),
]:
    assets = root + '/Assets/Characters/' + role
    folder = root + '/Blueprints/Characters/' + role
    ga = blueprint(root + '/Blueprints/Abilities/GA_' + role + 'BasicAttack', 'BasicAttackAbility')
    save(ga)
    data_path = root + '/Assets/Combat/DA_' + role + 'Abilities'
    data = unreal.load_asset(data_path)
    if not data:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.CharacterAbilityData)
        data = tools.create_asset('DA_' + role + 'Abilities', root + '/Assets/Combat', unreal.CharacterAbilityData, factory)
    data.set_editor_property('basic_attack', ga.generated_class())
    data.set_editor_property('basic_attack_level', 1)
    save(data)
    weapon = blueprint(root + '/Blueprints/Combat/Weapons/BP_' + role + 'Weapon', 'Weapon')
    if role == 'Archer':
        unreal.BlueprintEditorLibrary.reparent_blueprint(weapon, unreal.BowWeapon)
        unreal.BlueprintEditorLibrary.compile_blueprint(weapon)
    w = defaults(weapon)
    w.set_editor_property('attach_socket', socket)
    w.set_editor_property('attack_range', reach)
    w.set_editor_property('damage', damage)
    w.set_editor_property('projectile_class', projectile.generated_class())
    w.set_editor_property('attack_montages', [unreal.load_asset(assets + '/Animations/Montages/AM_' + role + '_BasicAttack')])
    w.get_editor_property('mesh').set_static_mesh(unreal.load_asset(assets + '/Weapons/' + weapon_mesh))
    w.get_editor_property('mesh').set_relative_scale3d(unreal.Vector(.7, .7, .7))
    save(weapon)
    bp = blueprint(folder + '/BP_' + role, role + 'Character')
    c = defaults(bp)
    c.set_editor_property('initial_max_health', 100.)
    cap = c.get_editor_property('capsule_component')
    cap.set_capsule_size(warrior_cap.get_unscaled_capsule_radius(), warrior_cap.get_unscaled_capsule_half_height())
    mesh = c.get_editor_property('mesh')
    mesh.set_skeletal_mesh_asset(unreal.load_asset(assets + '/SK_' + role))
    mesh.set_relative_transform(warrior_mesh.get_relative_transform(), False, False)
    anim = unreal.load_asset(folder + '/ABP_' + role)
    assert anim
    c.set_editor_property('animation_blueprint_class', anim.generated_class())
    mesh.set_anim_instance_class(anim.generated_class())
    c.set_editor_property('hit_reaction_montage', unreal.load_asset(assets + '/Animations/AM_' + role + '_Hit'))
    combat = c.get_component_by_class(unreal.CharacterCombatComponent)
    combat.set_editor_property('weapon_class', weapon.generated_class())
    combat.set_editor_property('ability_data', data)
    combat.set_editor_property('attack_position_query', query)
    save(bp)
    classes.append(bp.generated_class())
    print('RANGED_CHARACTER_CONFIGURED', role, reach, damage)

# Configure the two existing player slots for testing; no extra controllable pawns.
gm_path = 'Content/BoldersDemo/Blueprints/Shared/BP_BoldersGameMode.uasset'
backup = Path(unreal.Paths.project_saved_dir()) / 'RangedCharacters/Before/BP_BoldersGameMode.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(Path(unreal.Paths.project_dir()) / gm_path, backup)
gm = unreal.load_asset(root + '/Blueprints/Shared/BP_BoldersGameMode')
defaults(gm).set_editor_property('starting_character_classes', classes)
save(gm)
print('RANGED_SETUP_DONE', [c.get_path_name() for c in classes])
