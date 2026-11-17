"""Create a procedural decal and configure existing BaseCharacter Blueprint defaults."""
import unreal

E = unreal.EditorAssetLibrary
M = unreal.MaterialEditingLibrary
folder = '/Game/BoldersDemo/Assets/Shared/Materials'
path = folder + '/M_FactionRing'
mat = E.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'M_FactionRing', folder, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
M.delete_all_material_expressions(mat)

uv = M.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -600, 200)
mask = M.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 200)
input_uv = unreal.CustomInput()
input_uv.set_editor_property('input_name', 'UV')
mask.set_editor_property('inputs', [input_uv])
mask.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
mask.set_editor_property('description', 'Antialiased annulus; transparent center')
mask.set_editor_property('code', '''
float distanceToCenter = length(UV - 0.5);
float aa = max(fwidth(distanceToCenter), 0.001);
return (1.0 - smoothstep(0.44-aa, 0.44+aa, distanceToCenter))
     * smoothstep(0.39-aa, 0.39+aa, distanceToCenter);
''')
M.connect_material_expressions(uv, '', mask, 'UV')
M.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY)
color = M.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -200)
color.set_editor_property('parameter_name', 'RingColor')
color.set_editor_property('default_value', unreal.LinearColor(0.01, 0.08, 1, 1))
M.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
brightness = M.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, -50)
brightness.set_editor_property('parameter_name', 'RingBrightness')
brightness.set_editor_property('default_value', 0.7)
multiply = M.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, -100)
M.connect_material_expressions(color, '', multiply, 'A')
M.connect_material_expressions(brightness, '', multiply, 'B')
M.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
M.recompile_material(mat)
assert E.save_loaded_asset(mat, False)

configured = []
for asset_path in E.list_assets('/Game/BoldersDemo/Blueprints/Characters', recursive=True, include_folder=False):
    bp = E.load_asset(asset_path)
    if not isinstance(bp, unreal.Blueprint):
        continue
    cls = E.load_blueprint_class(asset_path)
    defaults = unreal.get_default_object(cls)
    if not isinstance(defaults, unreal.BaseCharacter):
        continue
    ring = defaults.get_editor_property('faction_ring')
    assert ring, asset_path
    ring.set_editor_property('decal_material', mat)
    # These characters' visible feet exceed their capsule radius; keep the ring outside them.
    ring.set_editor_property('radius', 100.0)
    ring.set_editor_property('player_color', unreal.LinearColor(0.01, 0.08, 1, 1))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert E.save_loaded_asset(bp, False)
    configured.append(asset_path)
assert any('BP_Warrior.' in p for p in configured)
assert any('BP_EnemyMushroom.' in p for p in configured)
unreal.log('FACTION_RING_CONFIGURED: ' + str(configured))
