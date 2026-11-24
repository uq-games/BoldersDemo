"""Import the Foundry fixture FBX and prepare Niagara systems for the editor authoring command."""
import os
from pathlib import Path

import unreal as ue

ROOT = '/Game/BoldersDemo/Assets/Environment/VolcanoCombat'
MESHES = ROOT + '/Meshes'
MATERIALS = ROOT + '/Materials'
FX = ROOT + '/Effects'
SOURCE = Path(os.environ.get('BOLDERS_FOUNDRY_MODEL_SOURCE',
    'H:/UE5/AssetSources/BoldersDemo/VolcanoCombat/Fixtures'))
LIBRARY = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
EDIT = ue.MaterialEditingLibrary

STYLE = {
    'Jet': ((5.4, .94, .16), False,
            'float2 q=float2(p.x*(1.25+.45*p.y),p.y*.66); float a=exp(-dot(q,q)*4.3);'
            'return float4(lerp(Tint*float3(.61,.34,.19),Tint,a),a*(1-smoothstep(.77,1.,r)));'),
    'Spark': ((3.7, .75, .15), False,
              'float a=exp(-abs(p.x)*12.)*(1-smoothstep(.07,1.,abs(p.y)));'
              'return float4(Tint*(.61+a),a*(1-smoothstep(.76,1.,r)));'),
    'Smoke': ((.15, .115, .09), True,
              'float n=.66+.14*sin(p.x*11.+p.y*4.)+.11*cos(p.x*7.-p.y*13.);'
              'float a=(1-smoothstep(.12,.95,r))*(.62+.38*n); return float4(Tint*(.72+.32*n), a*.62);'),
    'Steam': ((.79, .88, .91), True,
              'float v=.10*sin(9*p.x-7*p.y)+.11*cos(10*p.y+5*p.x);'
              'float a=smoothstep(.92,.08,length(p*float2(.94,.68))+v)*(1-smoothstep(.70,1.,r));'
              'return float4(Tint*(.78+.22*a), a*.55);'),
    'Frost': ((.47, 1.24, 2.30), False,
              'float v=abs(p.x+.21*p.y)*2.25+abs(p.y)*.92;'
              'float a=(1-smoothstep(.84,1.05,v))*smoothstep(.12,.77,.79-.44*p.x-.20*p.y);'
              'return float4(Tint*(.62+a),a);'),
    'Charge': ((.14, .96, 3.8), False,
                'float y=p.y-.14*sin(p.x*16.)-.055*sin(p.x*36.);'
                'float a=exp(-abs(y)*21.)*(1-smoothstep(.76,1.,abs(p.x)));'
                'return float4(Tint*(a+.47),a);'),
    'Fireball': ((4.5, .62, .12), False,
                 'float r2=length(p)+.11*sin(p.x*15.+p.y*11.); float a=1-smoothstep(.13,.94,r2);'
                 'return float4(lerp(Tint*float3(.77,.28,.15),float3(6,2.4,.8),a),a*a);'),
    'Cinder': ((1.6, .30, .055), False,
               'float v=max(abs(p.x+.10*p.y)*1.40,abs(p.y)*1.08);'
               'float a=1-smoothstep(.65,.95,v); return float4(Tint*(.50+.75*a),a);'),
}
SYSTEMS = ('VentPressure', 'VentEruption', 'VentCooling', 'TroughSteam', 'TroughFreeze',
           'TroughConduct', 'KegFuse', 'KegBlast', 'PylonCharge', 'PylonDischarge', 'ArcLink')


def load(path):
    return ue.load_asset(path) if LIBRARY.does_asset_exist(path) else None


def create(name, folder, klass, factory):
    return load(folder + '/' + name) or TOOLS.create_asset(name, folder, klass, factory)


def expression(owner, kind, x, y):
    return EDIT.create_material_expression(owner, getattr(ue, 'MaterialExpression' + kind), x,y)


def fixed(owner, value, x, y):
    node = expression(owner, 'Constant', x, y)
    node.set_editor_property('r', value)
    return node


def output(owner, node, link, prop):
    assert EDIT.connect_material_property(node, link, getattr(ue.MaterialProperty, 'MP_' + prop))


def custom(owner, code, inputs, width):
    node = expression(owner, 'Custom', -240, 20)
    pins = []
    for name, value in inputs.items():
        pin = ue.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    node.set_editor_properties(dict(code=code, inputs=pins,
        output_type=getattr(ue.CustomMaterialOutputType, 'CMOT_FLOAT' + str(width))))
    for name, (value, link) in inputs.items():
        assert EDIT.connect_material_expressions(value, link, node, name)
    return node


def mask(owner, node, rgb=False):
    out = expression(owner, 'ComponentMask', 10, 30 if rgb else 190)
    out.set_editor_properties(dict(r=rgb,g=rgb,b=rgb,a=not rgb))
    assert EDIT.connect_material_expressions(node, '', out, 'Input')
    return out


def particle(name, tint, smoke, code):
    asset_name = 'M_FoundryParticle_' + name
    if load(FX + '/' + asset_name):
        return
    mat = create(asset_name, FX, ue.Material, ue.MaterialFactoryNew())
    mat.set_editor_properties(dict(two_sided=True, blend_mode=(ue.BlendMode.BLEND_TRANSLUCENT if smoke
        else ue.BlendMode.BLEND_ADDITIVE), shading_model=ue.MaterialShadingModel.MSM_UNLIT))
    EDIT.set_material_usage(mat, ue.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    uv = expression(mat, 'TextureCoordinate', -550, 0)
    color = expression(mat, 'ParticleColor', -600, 220)
    shader = ('float2 p=UV*2.-1.; float r=length(p); float3 Tint=float3(%s);'
              'float4 term; ' % ','.join(map(str,tint))
              + code.replace('return float4(', 'term=float4(')
              + 'return float4(term.rgb, saturate(term.a*ParticleAlpha));')
    node = custom(mat, shader, {'UV': (uv, ''), 'ParticleAlpha': (color, 'A')}, 4)
    output(mat, mask(mat,node,True), '', 'EMISSIVE_COLOR')
    output(mat, mask(mat,node), '', 'OPACITY')
    EDIT.recompile_material(mat)
    assert LIBRARY.save_loaded_asset(mat, False)


def arc_material():
    asset_name = 'M_FoundryParticle_ArcLink'
    if load(FX + '/' + asset_name):
        return
    mat = create(asset_name, FX, ue.Material, ue.MaterialFactoryNew())
    mat.set_editor_properties(dict(two_sided=True, blend_mode=ue.BlendMode.BLEND_ADDITIVE,
                                   shading_model=ue.MaterialShadingModel.MSM_UNLIT))
    EDIT.set_material_usage(mat, ue.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES)
    uv = expression(mat, 'TextureCoordinate', -550, 0)
    time = expression(mat, 'Time', -550, 110)
    color = expression(mat, 'ParticleColor', -550, 230)
    shader = '''
float x = saturate(UV.x), y = UV.y - .5;
float f = floor(Clock * 22.);
float t = x * 14.;
float a = frac(sin((floor(t) + f * 23.) * 43.21) * 4831.14) - .5;
float b = frac(sin((floor(t) + 1. + f * 23.) * 43.21) * 4831.14) - .5;
float center = lerp(a, b, frac(t)) * .47 * sin(x * 3.14159);
float main = exp(-abs(y - center) * 68.);
float fork = exp(-abs(y - center - .27 * sin(x * 8. - 2.)) * 46.);
float halo = exp(-abs(y - center) * 12.);
float3 rgb = float3(.12, .57, 2.7) * halo + float3(2., 5., 9.) * main
             + float3(.14, .71, 3.) * fork * sin(x * 3.14159);
return float4(rgb, smoothstep(0., .02, x) * (1. - smoothstep(.98, 1., x)) * ParticleAlpha);
'''
    node = custom(mat, shader, {'UV': (uv, ''), 'Clock': (time, ''), 'ParticleAlpha': (color, 'A')}, 4)
    output(mat, mask(mat, node, True), '', 'EMISSIVE_COLOR')
    output(mat, mask(mat, node), '', 'OPACITY')
    EDIT.recompile_material(mat)
    assert LIBRARY.save_loaded_asset(mat, False)


def surface(name, rgb, metallic, roughness, code=None, luminous=False):
    asset_name = 'M_FoundryFixture_' + name
    existing = load(MATERIALS + '/' + asset_name)
    if existing:
        return existing
    mat = create(asset_name, MATERIALS, ue.Material, ue.MaterialFactoryNew())
    uv = expression(mat, 'TextureCoordinate', -640, -130)
    nodes = {'UV': (uv,'')}
    if luminous:
        time = expression(mat, 'Time', -640, 60)
        nodes['Clock'] = time,''
    color = 'float3 Base=float3(%s);' % ','.join(map(str,rgb))
    node = custom(mat, color + (code or 'return Base;'), nodes, 3)
    output(mat,node,'','EMISSIVE_COLOR' if luminous else 'BASE_COLOR')
    if luminous:
        diffuse = expression(mat, 'Constant3Vector', -430, 240)
        diffuse.set_editor_property('constant', ue.LinearColor(*(v*.085 for v in rgb), 1.))
        output(mat,diffuse,'','BASE_COLOR')
    output(mat,fixed(mat,metallic,-380,265),'','METALLIC')
    output(mat,fixed(mat,roughness,-380,350),'','ROUGHNESS')
    EDIT.recompile_material(mat)
    assert LIBRARY.save_loaded_asset(mat, False)
    return mat


def import_meshes(palette):
    flag = 'Interchange.FeatureFlags.Import.FBX'
    before = ue.SystemLibrary.get_console_variable_bool_value(flag)
    ue.SystemLibrary.execute_console_command(None, flag + ' 0')
    try:
        for fixture in ('PressureVent','CoolingTrough','PowderKeg','PowderKegSpent','ArcPylon','ArcCrystal'):
            name = 'SM_Foundry' + fixture
            task = ue.AssetImportTask()
            config = ue.FbxImportUI()
            config.set_editor_properties(dict(automated_import_should_detect_type=False,
                mesh_type_to_import=ue.FBXImportType.FBXIT_STATIC_MESH, import_as_skeletal=False,
                import_materials=False, import_textures=False))
            config.static_mesh_import_data.set_editor_properties(dict(auto_generate_collision=False,
                force_front_x_axis=False, convert_scene=True, convert_scene_unit=True,
                normal_import_method=ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS))
            task.set_editor_properties(dict(filename=str(SOURCE/(name+'.fbx')), destination_path=MESHES,
                destination_name=name, automated=True, replace_existing=True, replace_existing_settings=True,
                save=True, factory=ue.FbxFactory(), options=config))
            TOOLS.import_asset_tasks([task])
            mesh = load(MESHES + '/' + name)
            assert mesh and task.imported_object_paths, name
            assignments = []
            for i, slot in enumerate(mesh.get_editor_property('static_materials')):
                label = str(slot.get_editor_property('imported_material_slot_name'))
                assert label in palette, (name, label)
                mesh.set_material(i,palette[label])
                assignments.append(label)
            assert LIBRARY.save_loaded_asset(mesh, False)
            print('FOUNDRY_FIXTURE_MESH', name, mesh.get_bounding_box(), assignments)
    finally:
        ue.SystemLibrary.execute_console_command(None, flag + (' 1' if before else ' 0'))


def run():
    wood = 'float n=sin(UV.x*123.+7*sin(UV.y*18.))*sin(UV.x*37.+UV.y*6.); return Base*(.81+.23*n+.14*sin(UV.x*27.));'
    palette = {
        'Basalt': load(MATERIALS+'/M_FoundryBasalt'), 'Iron': load(MATERIALS+'/M_FoundryIron'),
        'Bronze': surface('Bronze',(.31,.145,.058), .72, .37, 'return Base*(.82+.29*abs(sin(UV.x*12.+UV.y*9.)));'),
        'Wood': surface('CaskWood',(.23,.085,.032),0.,.70,wood),
        'EndGrain': surface('CaskEndGrain',(.15,.052,.018),0.,.78,wood),
        'Ceramic': surface('Insulator',(.28,.37,.41), .18,.34, 'return Base*(.82+.23*saturate(sin(UV.y*23.)));'),
        'Ember': surface('Ember',(.96,.105,.012), .1,.50,
            'return Base*(.52+.54*saturate(sin(Clock*7.+UV.x*9.)));', True),
        'Crystal': surface('Crystal',(.13,.61,1.62), .28,.26,
            'return Base*(.48+.64*pow(.5+.5*sin(UV.y*17.-Clock*4.2),3.));', True),
    }
    assert all(palette.values())
    import_meshes(palette)
    surface('Ice', (.21, .49, .57), .10, .37, '''
float2 coords = UV * float2(12., 8.);
coords += .12 * sin(coords.yx * 3.2);
float2 cell = floor(coords), p = frac(coords);
float first = 9., second = 9., facet = 0.;
[unroll] for (int y = -1; y <= 1; ++y) {
  [unroll] for (int x = -1; x <= 1; ++x) {
    float2 offset = float2(x, y), key = cell + offset;
    float2 seed = frac(sin(float2(dot(key, float2(23.13, 71.7)),
                                  dot(key, float2(92.21, 57.2)))) * 4375.9);
    float2 v = offset + .16 + .68 * seed - p;
    float d = dot(v, v);
    if (d < first) { second = first; first = d; facet = seed.x; }
    else { second = min(second, d); }
  }
}
float edge = 1. - smoothstep(.022, .093, second - first);
float interior = .56 + .28 * facet + .10 * sin((coords.x + coords.y) * 3.);
return Base * interior + float3(.30, .43, .49) * edge;
''')
    for name, (tint, smoke, code) in STYLE.items():
        particle(name,tint,smoke,code)
    arc_material()
    for name in SYSTEMS:
        system = create('NS_Foundry' + name, FX, ue.NiagaraSystem, ue.NiagaraSystemFactoryNew())
        assert system and LIBRARY.save_loaded_asset(system, False), name
    print('FOUNDRY_FIXTURE_IMPORT_OK', len(SYSTEMS))


if __name__ == '__main__':
    run()

