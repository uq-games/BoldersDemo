"""Assemble the Volcano Ascent level in Unreal Editor 5.9."""

import json
import math
import os
import random
from pathlib import Path

import unreal as ue


SOURCE = '/Game/Inferno_World_Free/Maps/Demonstration'
MAP = '/Game/BoldersDemo/Assets/Maps/L_VolcanoAscent'
ROOT = '/Game/BoldersDemo/Assets/Environment/VolcanoAscent'
ORIGINAL = '/Game/Inferno_World_Free/Meshes'
QA = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_saved_dir())) / 'VolcanoAscent'
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
LEVELS = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
ED = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
LIB = ue.EditorAssetLibrary
MAT = ue.MaterialEditingLibrary
RNG = random.Random(74218)

NATURE = ('tree_pineTallA_detailed', 'tree_pineTallB_detailed', 'tree_pineTallD_detailed',
          'tree_detailed_dark', 'tree_pineRoundC', 'tree_tall_fall', 'plant_bushDetailed',
          'plant_bushSmall', 'plant_flatShort', 'rock_largeA', 'rock_tallC', 'rock_smallFlatC',
          'statue_columnDamaged')
CASTLE = ('wall-half', 'wall-corner-half', 'wall-pillar', 'tower-square-base-border',
          'tower-square-mid-windows', 'tower-square-arch', 'flag-banner-long', 'wall-doorway')
DRESS = {'Nature': {}, 'Castle': {}}
BUILT = 'VolcanoAscentGenerated'
WALK = ue.Name('WalkableGround')
FIRE_BOUNDS = ((0, 0, 15), (147, 145, 48))
GRADE = (
    ('Forest', (605,-172,25), (220,122,146), (.58,.98,.85), (.92,1.03,.94), -.28, 12.),
    ('Ruins', (594,-73,27), (157,93,146), (.68,1.0,.99), (.92,1.02,1.01), -.46, 11.),
    ('Canyon', (455,-1,27), (160,109,146), (.73,.94,1.0), (.95,1.0,1.05), -.49, 10.),
)


def vector_meters(values):
    return ue.Vector(*(v * 100 for v in values))


def asset(path):
    found = ue.load_asset(path)
    assert found, path
    return found


def create_material(name):
    folder = ROOT + '/Materials'
    path = f'{folder}/{name}'
    if LIB.does_asset_exist(path):
        return asset(path), False
    return TOOLS.create_asset(name, folder, ue.Material, ue.MaterialFactoryNew()), True


def node(material, cls, x, y, **properties):
    expression = MAT.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def color_material(name, color=None, texture=None, vertex_gain=1.):
    mat, created = create_material(name)
    if not created:
        return mat
    if texture:
        source = node(mat, ue.MaterialExpressionTextureSample, -570, 0, texture=texture)
        tint = node(mat, ue.MaterialExpressionConstant3Vector, -570, 200,
                    constant=ue.LinearColor(*color, 1))
        multiply = node(mat, ue.MaterialExpressionMultiply, -260, 0)
        MAT.connect_material_expressions(source, 'RGB', multiply, 'A')
        MAT.connect_material_expressions(tint, '', multiply, 'B')
        source = multiply
    elif color:
        source = node(mat, ue.MaterialExpressionConstant3Vector, -360, 0,
                      constant=ue.LinearColor(*color, 1))
    else:
        source = node(mat, ue.MaterialExpressionVertexColor, -360, 0)
        if vertex_gain != 1:
            multiply = node(mat, ue.MaterialExpressionMultiply, -145, 0, const_b=vertex_gain)
            MAT.connect_material_expressions(source, 'RGB', multiply, 'A')
            source = multiply
    MAT.connect_material_property(source, '', ue.MaterialProperty.MP_BASE_COLOR)
    roughness = node(mat, ue.MaterialExpressionConstant, -360, 240, r=.89)
    MAT.connect_material_property(roughness, '', ue.MaterialProperty.MP_ROUGHNESS)
    MAT.recompile_material(mat)
    assert LIB.save_loaded_asset(mat)
    return mat


def lava_material():
    mat, created = create_material('M_AscentLavaChannel')
    if not created:
        return mat
    mat.set_editor_properties(dict(blend_mode=ue.BlendMode.BLEND_MASKED,
                                    shading_model=ue.MaterialShadingModel.MSM_UNLIT))
    uv = node(mat, ue.MaterialExpressionTextureCoordinate, -630, 0)
    time = node(mat, ue.MaterialExpressionTime, -630, 160)
    inputs = []
    for name in ('UV', 'Drift'):
        parameter = ue.CustomInput()
        parameter.set_editor_property('input_name', ue.Name(name))
        inputs.append(parameter)
    shader = node(mat, ue.MaterialExpressionCustom, -370, 0,
                  output_type=ue.CustomMaterialOutputType.CMOT_FLOAT4,
                  inputs=inputs,
                  code='''
float channelOffset = UV.y - 0.5 + 0.105 * sin(UV.x * 15.0) + 0.035 * sin(UV.x * 48.0);
float width = (0.17 + 0.07 * sin(UV.x * 31.0) + 0.04 * cos(UV.x * 82.0))
              * smoothstep(0.0, 0.12, UV.x) * (1.0 - smoothstep(0.87, 1.0, UV.x));
float mask = step(abs(channelOffset), width);
float ripple = sin(UV.x * 93.0 - Drift * 1.1 + sin(UV.y * 30.0) * 1.5)
             * cos(UV.y * 31.0 + sin(UV.x * 58.0) * 0.9);
float heat = smoothstep(-0.36, 0.72, ripple) * (1.0 - smoothstep(0.0, width + 0.0001, abs(channelOffset)));
return float4(lerp(float3(0.24, 0.012, 0.002), float3(1.6, 0.26, 0.022), heat), mask);
''')
    MAT.connect_material_expressions(uv, '', shader, 'UV')
    MAT.connect_material_expressions(time, '', shader, 'Drift')
    rgb = node(mat, ue.MaterialExpressionComponentMask, -80, -30, r=True, g=True, b=True, a=False)
    alpha = node(mat, ue.MaterialExpressionComponentMask, -80, 170, r=False, g=False, b=False, a=True)
    MAT.connect_material_expressions(shader, '', rgb, 'Input')
    MAT.connect_material_expressions(shader, '', alpha, 'Input')
    MAT.connect_material_property(rgb, '', ue.MaterialProperty.MP_EMISSIVE_COLOR)
    MAT.connect_material_property(alpha, '', ue.MaterialProperty.MP_OPACITY_MASK)
    MAT.recompile_material(mat)
    assert LIB.save_loaded_asset(mat)
    return mat


def import_task(source, folder, name, kind=ue.StaticMesh):
    path = f'{folder}/{name}'
    if LIB.does_asset_exist(path):
        return asset(path)
    task = ue.AssetImportTask()
    task.set_editor_properties(dict(filename=source.as_posix(), destination_path=folder,
                                    destination_name=name, automated=True, save=True,
                                    replace_existing=False))
    if kind == ue.StaticMesh:
        data = ue.FbxStaticMeshImportData()
        data.set_editor_properties(dict(combine_meshes=True, auto_generate_collision=False,
                                        generate_lightmap_u_vs=False,
                                        vertex_color_import_option=ue.VertexColorImportOption.REPLACE))
        options = ue.FbxImportUI()
        options.set_editor_properties(dict(automated_import_should_detect_type=False,
                                           mesh_type_to_import=ue.FBXImportType.FBXIT_STATIC_MESH,
                                           import_mesh=True, import_materials=False,
                                           import_textures=False, static_mesh_import_data=data))
        task.set_editor_properties(dict(factory=ue.FbxFactory(), options=options))
    TOOLS.import_asset_tasks([task])
    imported = next((obj for obj in task.get_objects() if isinstance(obj, kind)), None)
    assert imported, (source, task.imported_object_paths)
    return imported


def import_geometry(terrain_material):
    meshes = {}
    for source in sorted((QA / 'Geometry').glob('*.fbx')):
        mesh = import_task(source, ROOT + '/Meshes', source.stem)
        if mesh.get_material(0) != terrain_material:
            mesh.set_material(0, terrain_material)
        b = mesh.get_bounds()
        ue.log(f'ASCENT_MESH {mesh.get_name()} origin={b.origin} extent={b.box_extent}')
        meshes[source.stem.removeprefix('SM_Ascent')] = mesh
    return meshes


def collision(mesh):
    editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if editor is None:
        # The editor module is loaded on demand when opening the first mesh.
        assets = ue.get_editor_subsystem(ue.AssetEditorSubsystem)
        assert assets.open_editor_for_assets([mesh])
        assets.close_all_editors_for_asset(mesh)
        editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    assert editor and editor.set_collision_complexity(mesh, ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assert LIB.save_loaded_asset(mesh)


def import_kenney():
    for kind, filenames in (('Nature', NATURE), ('Castle', CASTLE)):
        folder = '/Game/Kenney' + kind + 'Kit'
        models = QA / 'Converted' / kind.lower()
        color = terrain_material
        if kind == 'Castle':
            atlas = QA / 'castle-kit' / 'Models' / 'FBX format' / 'Textures' / 'colormap.png'
            tex = import_task(atlas, folder + '/Textures', 'T_CastleAtlas', ue.Texture2D)
            color = color_material('M_AscentCastleAtlas', (.69, .75, .76), texture=tex)
        for filename in filenames:
            name = 'SM_' + filename.replace('-', '_')
            mesh = import_task(models / (filename + '.fbx'), folder + '/Meshes', name)
            DRESS[kind][filename] = mesh, color
            if filename in ('wall-half', 'wall-corner-half', *NATURE[9:12]):
                body = mesh.get_editor_property('body_setup')
                flag = ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
                if body.get_editor_property('collision_trace_flag') != flag:
                    collision(mesh)


def duplicate_world():
    if (Path(ue.Paths.project_content_dir()) / 'BoldersDemo' / 'Assets' /
            'Maps' / 'L_VolcanoAscent.umap').exists():
        assert LEVELS.load_level(MAP), MAP
        return ED.get_editor_world()
    duplicate = ue.EditorLoadingAndSavingUtils.new_map_from_template(SOURCE, False)
    assert duplicate and ue.EditorLoadingAndSavingUtils.save_map(duplicate, MAP), MAP
    assert LEVELS.load_level(MAP), MAP
    return ED.get_editor_world()


def actor(cls, label, pos, folder, yaw=0, pitch=0):
    result = ACTORS.spawn_actor_from_class(cls, vector_meters(pos), ue.Rotator(pitch=pitch, yaw=yaw))
    assert result, label
    result.set_actor_label(label)
    result.set_folder_path(ue.Name(folder))
    result.set_editor_property('tags', [ue.Name(BUILT)])
    return result


def static(label, mesh, pos, folder, *, yaw=0, pitch=0, scale=(1, 1, 1),
           walk=False, block=False, material=None):
    result = actor(ue.StaticMeshActor, label, pos, folder, yaw, pitch)
    component = result.static_mesh_component
    component.set_static_mesh(mesh)
    if material:
        for index in range(component.get_num_materials()):
            component.set_material(index, material)
    result.set_actor_scale3d(ue.Vector(*scale))
    component.set_collision_profile_name('BlockAll' if walk or block else 'NoCollision')
    component.set_editor_property('can_ever_affect_navigation', walk or block)
    if walk:
        result.set_editor_property('tags', [ue.Name(BUILT), WALK])
    return result


def prop(label, pair, point, folder, height, *, yaw=0, width=None, block=False):
    mesh, material = pair
    bounds = mesh.get_bounds()
    zscale = height * 50 / bounds.box_extent.z
    xyscale = width * 50 / max(bounds.box_extent.x, bounds.box_extent.y) if width else zscale
    pos = (point[0], point[1], point[2] - (bounds.origin.z - bounds.box_extent.z) * zscale / 100)
    return static(label, mesh, pos, folder, yaw=yaw, scale=(xyscale, xyscale, zscale),
                  block=block, material=material)


def route_point(distance, side=0):
    index = min(range(len(run)), key=lambda i: abs(run[i] - distance))
    point = shelf[index][round((side + 1) * 10)]
    return tuple(point)


def angle_on_route(distance):
    a, b = route_point(distance - 3), route_point(distance + 3)
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def inferno(name, category):
    return asset(f'{ORIGINAL}/{category}/{name}'), None


def place_forest():
    folder = 'Volcano Ascent/Forest/Vegetation'
    trees = (*NATURE[:5], NATURE[5])
    for i in range(99):
        distance = RNG.uniform(0, 143)
        side = RNG.choice((-1, 1)) * RNG.uniform(.52, .97)
        p = route_point(distance, side)
        if abs(p[0] - 565) < 9 and abs(p[1] + 173) < 10:
            continue
        height = RNG.uniform(4.3, 8.9) * (1 if distance < 100 else .7)
        prop(f'Conifer {i+1:03d}', DRESS['Nature'][RNG.choice(trees)], p, folder,
             height, yaw=RNG.uniform(0, 360))
    for i in range(150):
        d = RNG.uniform(0, 152)
        side = RNG.choice((-1, 1)) * RNG.uniform(.30, .97)
        sample = RNG.choice(NATURE[6:9])
        prop(f'Understory {i+1:03d}', DRESS['Nature'][sample], route_point(d, side),
             folder, RNG.uniform(.24, 1.15), yaw=RNG.uniform(0, 360))
    for i in range(22):
        d = RNG.uniform(4, 139)
        side = RNG.choice((-1, 1)) * RNG.uniform(.49, .94)
        prop(f'Glacial Boulder {i+1:02d}', DRESS['Nature'][RNG.choice(NATURE[9:12])],
             route_point(d, side), 'Volcano Ascent/Forest/Stone',
             RNG.uniform(.55, 2.6), yaw=RNG.uniform(0, 360))
    for i, (d, side) in enumerate(((34,-.55), (67,.70), (101,-.84), (126,.82))):
        prop(f'Old Waymarker {i+1}', DRESS['Nature']['statue_columnDamaged'], route_point(d, side),
             'Volcano Ascent/Forest/Stone', 2.05, yaw=angle_on_route(d))
    folder = 'Volcano Ascent/Forest/Stone'
    for i, (d, side, h) in enumerate(((44,-.42,2.7), (49,-.36,1.6), (53,-.41,2.05),
                                     (69,.53,1.8), (78,.49,2.3))):
        prop(f'Mossy Erratic {i+1}', DRESS['Nature'][NATURE[9+i%3]], route_point(d, side),
             folder, h, width=h*1.34, block=True, yaw=37+i*53)


def place_ruins():
    folder = 'Volcano Ascent/Ruins/Architecture'
    for i, (distance, side) in enumerate(((147,-.70), (158,-.75), (171,-.78), (185,-.72),
                                          (158,.76), (173,.78), (185,.80), (198,.72))):
        p = route_point(distance, side)
        key = 'wall-corner-half' if i in (0, 3, 4, 7) else 'wall-half'
        prop(f'Broken Curtain Wall {i+1:02d}', DRESS['Castle'][key], p, folder,
             RNG.uniform(1.5, 2.7), yaw=angle_on_route(distance) + 90, width=RNG.uniform(6, 9), block=True)
    for i, (distance, side) in enumerate(((153,-.47), (159,.49), (174,-.43), (183,.44),
                                         (193,-.47), (199,.54), (207,-.60))):
        prop(f'Fallen Colonnade {i+1:02d}', DRESS['Nature']['statue_columnDamaged'],
             route_point(distance, side), folder, RNG.uniform(1.7, 3.8),
             yaw=RNG.uniform(0, 360), width=1.8)
    for i, (distance, side) in enumerate(((144,.73), (191,-.82), (210,.81))):
        p = route_point(distance, side)
        yaw = angle_on_route(distance) + 90
        prop(f'Watchtower Base {i+1}', DRESS['Castle']['tower-square-base-border'],
             p, folder, 2.55, width=6.4, yaw=yaw)
        prop(f'Watchtower Stonework {i+1}', DRESS['Castle']['tower-square-mid-windows'],
             (p[0], p[1], p[2]+2.5), folder, 4.2, width=5.4, yaw=yaw)
        prop(f'Watchtower Banner {i+1}', DRESS['Castle']['flag-banner-long'],
             (p[0]+2.5, p[1], p[2]+2.7), folder, 2.45, width=1.0, yaw=yaw)
    for i, (d, side) in enumerate(((202,-.53), (215,.63))):
        prop(f'Fractured Arch {i+1}', DRESS['Castle']['tower-square-arch'],
             route_point(d, side), folder, 5.6, width=7.4, yaw=angle_on_route(d) + 90)
    for i in range(47):
        prop(f'Weathered Rubble {i+1:02d}', DRESS['Nature'][RNG.choice(NATURE[9:12])],
             route_point(RNG.uniform(126, 221), RNG.choice((-1, 1)) * RNG.uniform(.45, .95)),
             'Volcano Ascent/Ruins/Stone', RNG.uniform(.15, 1.15), yaw=RNG.uniform(0, 360))
    for i, (distance, side) in enumerate(((150,.25), (171,-.73), (192,.31), (214,-.52))):
        p = route_point(distance, side)
        prop(f'Broken Gate {i+1}', DRESS['Castle']['wall-doorway'], p, folder, 3.4, width=5.2,
             yaw=angle_on_route(distance) - 20 if i % 2 == 0 else angle_on_route(distance) + 35)
    pavers = [color_material('M_AscentPaving'+str(i+1), color) for i,color in enumerate(
        ((.23,.25,.235),(.26,.28,.258),(.196,.218,.205),(.31,.308,.266)))]
    for i, row in enumerate(range(163,183,2)):
        for j, side in enumerate((-.25,-.15,-.05,.05,.15,.25)):
            if RNG.random() < .19:
                continue
            x,y,z = route_point(row, side)
            yaw = angle_on_route(row)
            box(f'Courtyard Paving {i+1:02d} {j+1}', (x,y,z+.027), (1.58,1.55,.065),
                RNG.choice(pavers), folder, yaw=yaw+RNG.uniform(-4,4))


def apply_grade(volume, gain, gamma, exposure, priority):
    volume.set_editor_properties(dict(priority=priority, blend_radius=1450., blend_weight=1.,
                                      unbound=False, enabled=True))
    settings = volume.get_editor_property('settings')
    settings.set_editor_properties(dict(override_color_gain=True, color_gain=ue.Vector4(*gain, 1.),
                                        override_color_gamma=True, color_gamma=ue.Vector4(*gamma, 1.),
                                        override_auto_exposure_bias=True, auto_exposure_bias=exposure))
    volume.set_editor_property('settings', settings)


def lighting():
    for name, p, dimensions, gain, gamma, exposure, priority in GRADE:
        volume = actor(ue.PostProcessVolume, name + ' Color Grade', p, 'Volcano Ascent/Lighting')
        volume.set_actor_scale3d(ue.Vector(*(d / 2 for d in dimensions)))
        apply_grade(volume, gain, gamma, exposure, priority)


def add_light(label, pos, color, brightness, radius, folder):
    light = actor(ue.PointLight, label, pos, folder)
    component = light.get_component_by_class(ue.PointLightComponent)
    component.set_light_color(ue.LinearColor(*color, 1))
    component.set_intensity(brightness)
    component.set_attenuation_radius(radius * 100)
    component.set_cast_shadows(False)


def place_canyon():
    folder = 'Volcano Ascent/Canyon/Basalt'
    rocks = (inferno('SM_Crag_001','Rocks'), inferno('SM_Crag_003','Rocks'),
             inferno('SM_RockBig_003','Rocks'), inferno('SM_RockMid_004','Rocks'))
    for i in range(58):
        d = RNG.uniform(214, 313)
        side = RNG.choice((-1, 1)) * RNG.uniform(.56, 1)
        prop(f'Basalt Outcrop {i+1:02d}', RNG.choice(rocks), route_point(d, side),
             folder, RNG.uniform(.8, 4.3), width=RNG.uniform(1.2, 3.9), yaw=RNG.uniform(0, 360))
    for i, p in enumerate(((517,3,-20), (501,13,-19), (486,12,-22), (466,14,-21),
                           (527,-55,-22), (496,-55,-20), (464,-40,-18), (449,20,-18))):
        prop(f'Basalt Buttress {i+1}', RNG.choice(rocks[:2]), p, folder,
             RNG.uniform(17, 29), width=RNG.uniform(8, 15), yaw=RNG.uniform(0, 360))
    plane = asset('/Engine/BasicShapes/Plane')
    liquid = lava_material()
    for i, (x, y, length, width, yaw) in enumerate(((532,12,54,9,-28), (488,34,45,13,-11),
                                                    (447,39,47,13,14), (476,-44,63,12,26))):
        static(f'Lava Pool {i+1}', plane, (x,y,-10), 'Volcano Ascent/Canyon/Lava',
               yaw=yaw, scale=(length,width*1.45,1), material=liquid)
        add_light(f'Lava Bounce {i+1}', (x,y,-6), (1,.31,.075), 12000, 28,
                  'Volcano Ascent/Canyon/Lava')


def box(label, p, dimensions, mat, folder, yaw=0, block=False):
    return static(label, cube, p, folder, scale=dimensions, material=mat, yaw=yaw, block=block)


def dress_bridge(name, anchors):
    folder = 'Volcano Ascent/MountainPass/' + name
    stone = color_material('M_AscentBridgeStone', (.128,.116,.125))
    trim = color_material('M_AscentBridgeTrim', (.28,.222,.172))
    rail = inferno('SM_ColumnBigBroken_001','Decorations')
    lamp = inferno('SM_Brazier_002','Props')
    distance = 0
    for a, b in zip(anchors, anchors[1:]):
        dx, dy, dz = (q-p for p,q in zip(a,b))
        length = math.hypot(dx,dy)
        nx, ny = -dy/length, dx/length
        yaw = math.degrees(math.atan2(dy, dx))
        for index in range(1, math.ceil(length / 6.6)):
            t = index * 6.6 / length
            x, y, z = a[0]+dx*t, a[1]+dy*t, a[2]+dz*t
            distance += 1
            if distance % 3 == 1:
                box(f'{name} Pier {distance:02d}', (x,y,(z-14)*.5),
                    (3.8,6.4,z+14), stone, folder, yaw=yaw)
                box(f'{name} Pier Cap {distance:02d}', (x,y,z-0.84), (5.8,9.5,.62),
                    trim, folder, yaw=yaw)
            for side in (-1, 1):
                p = x + nx * 4.25 * side, y + ny * 4.25 * side, z
                height = 1.04 if distance % 3 else 1.5
                prop(f'{name} Parapet {distance:02d} {side}', rail, p, folder, height,
                     yaw=yaw, width=.8)
                if distance % 5 == 2:
                    prop(f'{name} Ember Brazier {distance:02d} {side}', lamp,
                         (p[0],p[1],p[2]+.1), folder, 1.3, width=1.25)
                    add_light(f'{name} Torch Light {distance:02d} {side}', (p[0],p[1],p[2]+1.48),
                              (1,.4,.13), 2600, 8.5, folder)


def configure_original():
    original_actors = {a.get_name(): a for a in ACTORS.get_all_level_actors() if not a.actor_has_tag(ue.Name(BUILT))}
    assert len(original_actors) >= 290, len(original_actors)
    for key, delta in (('SM_Cliff_004_938', (0,-108,0)), ('SM_Cliff_003_906', (0,100,0))):
        cliff = original_actors[key]
        if not cliff.actor_has_tag(ue.Name('AscentPassBackdrop')):
            cliff.add_actor_world_offset(vector_meters(delta), False, True)
            cliff.set_editor_property('tags', [ue.Name('AscentPassBackdrop')])
    for existing in original_actors.values():
        if isinstance(existing, ue.PlayerStart):
            existing.set_actor_label('Forest Camera Start')
            existing.set_actor_location(vector_meters((545, -171, 17.2)), False, True)
            existing.set_actor_rotation(ue.Rotator(yaw=3), True)
            existing.set_folder_path(ue.Name('Volcano Ascent/Forest/Gameplay'))
        elif isinstance(existing, ue.StaticMeshActor):
            component = existing.static_mesh_component
            mesh = component.static_mesh
            if mesh and ('M_HDRI' not in [m.get_name() for m in component.get_materials() if m]):
                existing.set_folder_path(ue.Name('Volcano Ascent/Caldera/Original Scene'))
            if existing.get_name() == 'Plane_1622':
                component.set_collision_profile_name('NoCollision')
                component.set_editor_property('can_ever_affect_navigation', False)
            if mesh and any(term in mesh.get_name() for term in ('Platform','Stairs')):
                existing.set_editor_property('tags', list(existing.tags) + [WALK] if WALK not in existing.tags else list(existing.tags))
            if mesh and 'Cliff' in mesh.get_name():
                component.set_editor_property('can_ever_affect_navigation', False)


def configure_navigation():
    folder = 'Volcano Ascent/Navigation'
    bounds = [
        ('Forest', (582,-163,5), (97,86,48)),
        ('Forest Ridge', (628,-122,7), (45,61,48)),
        ('Ruins', (606,-70,9), (86,69,48)),
        ('Ruins Approach', (549,-48,10), (57,46,48)),
        ('Canyon', (492,-20,11), (102,61,56)),
        ('High Bridge', (420,9,11), (67,45,56)),
        ('Mountain Pass', (359,22,10), (67,51,56)),
        ('Caldera Causeway', (269,17,6), (121,25,48)),
        ('Caldera Island', (203,15,4), (62,55,48)),
        ('Crater Bridge', (121,31,10), (142,33,48)),
        ('Original Fire Arena', *FIRE_BOUNDS),
    ]
    for name, position, size in bounds:
        volume = actor(ue.NavMeshBoundsVolume, 'Nav ' + name, position, folder)
        volume.set_actor_scale3d(ue.Vector(*(v / 2 for v in size)))
        center, extent = volume.get_actor_bounds(False)
        assert min(extent.x, extent.y, extent.z) > 100, (name, extent)
        nav = ue.NavigationSystemV1.get_navigation_system(world)
        if nav:
            nav.on_navigation_bounds_updated(volume)
        ue.log(f'ASCENT_NAV {name} center={center} extent={extent}')
    for index, pos in enumerate(((565.3,-173.0,5.12),(565.2,-169.8,5.12)), 1):
        point = actor(ue.TargetPoint, f'Forest Character Spawn {index}', pos,
                      'Volcano Ascent/Forest/Gameplay', yaw=24)
        point.set_editor_property('tags', [ue.Name(BUILT), ue.Name('WarriorSpawn')])


def main():
    global terrain_material, world, run, shelf, cube
    terrain_material = color_material('M_AscentTerrain')
    bridge_material = color_material('M_AscentBridgeSurface', vertex_gain=.55)
    geometry = import_geometry(terrain_material)
    import_kenney()
    for name in ('Forest','Ruins','Canyon','MountainPass','CalderaCauseway','CraterBridge'):
        collision(geometry[name])
    world = duplicate_world()
    for existing in ACTORS.get_all_level_actors():
        if existing.actor_has_tag(ue.Name(BUILT)):
            ACTORS.destroy_actor(existing)

    layout = json.loads((QA / 'Geometry' / 'route.json').read_text(encoding='utf-8'))
    run, shelf = layout['distances'], layout['shelf']
    cube = asset('/Engine/BasicShapes/Cube')
    for name, mesh in geometry.items():
        area = next((zone for zone in ('Forest', 'Ruins', 'Canyon') if name.startswith(zone)), 'MountainPass')
        is_bridge = name in layout['bridges']
        is_route = is_bridge or name in ('Forest', 'Ruins', 'Canyon', 'MountainPass')
        folder = 'Volcano Ascent/' + area + '/Terrain'
        static('Ascent ' + name, mesh, (0, 0, 0), folder, walk=is_route,
               material=bridge_material if is_bridge else None)

    configure_original()
    place_forest()
    place_ruins()
    place_canyon()
    lighting()
    for name, anchors in layout['bridges'].items():
        dress_bridge(name, anchors)
    configure_navigation()
    ue.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    assert LEVELS.save_current_level()
    ue.log('ASCENT_CORE_SAVED actors=' + str(len(ACTORS.get_all_level_actors())))
    if os.getenv('BOLDERS_ASCENT_EXIT_ON_SAVE') == '1':
        ue.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')


if __name__ == '__main__':
    main()
