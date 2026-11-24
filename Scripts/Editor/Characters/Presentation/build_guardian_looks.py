"""Give the volcano guardians readable cap variants while preserving their texture atlas."""

import unreal as ue


SOURCE = '/Game/MonsterForSurvivalGame/Material/DefaultPBR_MAT'
FOLDER = '/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Materials'
MASTER = f'{FOLDER}/M_GuardianCap'
LOOKS = {
    'MI_GuardianScrapper': (.47, .031, .015),
    'MI_GuardianSporecaster': (.264, .232, .060),
    'MI_GuardianBulwark': (.084, .058, .034),
}

lib = ue.EditorAssetLibrary
graph = ue.MaterialEditingLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
base_color = ue.MaterialProperty.MP_BASE_COLOR


def create_master():
    material = ue.load_asset(MASTER) if lib.does_asset_exist(MASTER) else lib.duplicate_asset(SOURCE, MASTER)
    assert isinstance(material, ue.Material), MASTER
    atlas = graph.get_material_property_input_node(material, base_color)
    assert atlas, 'Guardian source needs a base-color connection'
    if isinstance(atlas, ue.MaterialExpressionCustom) and atlas.get_editor_property('description') == 'Guardian cap tint':
        return material

    previous_pin = graph.get_material_property_input_node_output_name(material, base_color)
    tint = graph.create_material_expression(material, ue.MaterialExpressionVectorParameter, -450, 260)
    tint.set_editor_properties(dict(parameter_name='CapTint', default_value=ue.LinearColor(.47, .031, .015, 1.)))
    color = graph.create_material_expression(material, ue.MaterialExpressionCustom, -100, -30)
    slots = []
    for name in ('Base', 'CapTint'):
        slot = ue.CustomInput()
        slot.set_editor_property('input_name', name)
        slots.append(slot)
    color.set_editor_properties(dict(inputs=slots, description='Guardian cap tint',
                                     output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code='''
float3 c = saturate(Base.rgb);
float mask = smoothstep(.048, .19, c.r - c.g) * smoothstep(.036, .17, c.r - c.b);
mask *= 1.0 - smoothstep(.18, .40, c.g);
float shade = lerp(.48, 1.18, smoothstep(.02, .82, c.r));
return lerp(c, saturate(CapTint.rgb * shade), mask);
'''))
    assert graph.connect_material_expressions(atlas, previous_pin, color, 'Base')
    assert graph.connect_material_expressions(tint, '', color, 'CapTint')
    assert graph.connect_material_property(color, '', base_color)
    graph.recompile_material(material)
    assert lib.save_loaded_asset(material)
    return material


material = create_master()
for name, tint in LOOKS.items():
    path = f'{FOLDER}/{name}'
    instance = ue.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(
        name, FOLDER, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    assert isinstance(instance, ue.MaterialInstanceConstant), path
    graph.set_material_instance_parent(instance, material)
    assert graph.set_material_instance_vector_parameter_value(instance, 'CapTint', ue.LinearColor(*tint, 1.))
    assert lib.save_loaded_asset(instance)

ue.log(f'VOLCANO_GUARDIAN_LOOKS_SAVED {len(LOOKS)}')
