"""Generate editable FBX terrain for Volcano Ascent with Blender in background mode."""

import bisect
import json
import math
import random
import sys
from pathlib import Path

import bpy


# Coordinates are in Unreal meters; camera and collision are authored in the level.
# x, y, center height, left half-width, right half-width.
KNOTS = [
    (545, -181, 4.2, 18, 20),
    (570, -172, 4.2, 30, 31),
    (596, -157, 4.3, 26, 27),
    (620, -137, 5.2, 15, 13),
    (635, -110, 6.6, 10, 13),
    (621, -81, 8.0, 24, 23),
    (598, -61, 8.5, 25, 28),
    (566, -55, 8.9, 17, 14),
    (536, -41, 9.4, 13, 13),
    (509, -24, 10.0, 23, 20),
    (482, -14, 10.5, 22, 17),
    (459, -9, 10.8, 12, 12),
    (443, -3, 11.0, 7, 7),
    (424, 6, 11.3, 4.4, 4.4),
    (402, 15, 11.6, 4.4, 4.4),
    (383, 21, 11.3, 11, 9),
    (354, 23, 10.3, 13, 12),
    (325, 22, 9.2, 11, 11),
]

BRIDGES = {
    'CalderaCauseway': [(325, 22, 9.2), (304, 21, 8.4), (280, 18, 7.3),
                        (252, 15, 6.1), (229, 14, 4.6), (213, 12, 3.58)],
    'CraterBridge': [(190, 18, 3.58), (173, 22, 3.75), (151, 27, 5.8),
                     (114, 33, 9.4), (80, 39, 12.9), (58, 42, 15.58), (43, 43, 15.59)],
}

OUTPUT = Path(sys.argv[sys.argv.index('--') + 1]).resolve()
OUTPUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
RNG = random.Random(63271)


def spline(t, a, b, c, d):
    return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t**2
                  + (-a + 3 * b - 3 * c + d) * t**3)


def sample():
    points = []
    for i, (b, c) in enumerate(zip(KNOTS, KNOTS[1:])):
        a = KNOTS[max(0, i - 1)]
        d = KNOTS[min(len(KNOTS) - 1, i + 2)]
        steps = math.ceil(math.dist(b[:2], c[:2]) / 2.4)
        for step in range(steps):
            t = step / steps
            points.append(tuple(spline(t, *values) for values in zip(a, b, c, d)))
    return points + [KNOTS[-1]]


ROWS = sample()
RUN = [0.0]
for p, q in zip(ROWS, ROWS[1:]):
    RUN.append(RUN[-1] + math.dist(p[:3], q[:3]))


def normal_at(i):
    a = ROWS[max(0, i - 1)]
    b = ROWS[min(i + 1, len(ROWS) - 1)]
    dx, dy = b[0] - a[0], b[1] - a[1]
    return -dy / math.hypot(dx, dy), dx / math.hypot(dx, dy)


def mix(a, b, amount):
    return tuple(x + (y - x) * amount for x, y in zip(a, b))


GREEN = ((.075, .155, .103), (.23, .28, .168), (.15, .2, .146))
RUIN = ((.176, .205, .186), (.34, .325, .263), (.245, .246, .214))
ASH = ((.102, .111, .139), (.24, .205, .173), (.15, .153, .174))


def shelf(i, u):
    x, y, z, left, right = ROWS[i]
    d = RUN[i]
    nx, ny = normal_at(i)
    width = left if u < 0 else right
    side = abs(u)
    waviness = math.sin(d * .17) * .05 + math.sin(d * .39) * .028
    offset = u * width * (1 + waviness * side**2)
    shoulder = max(0, (side - .52) / .48)
    noise = math.sin(d * .2 + u * 7) * .53 + math.cos(d * .47 + u * 13) * .21
    edge_height = (max(0, min(1, (width - 7) / 12)) * shoulder**2
                   * ((2.2 if u < 0 else 1.25) + noise * 2.0))
    height = z + edge_height + noise * min(1, abs(offset) / 6) * .19
    return x + nx * offset, y + ny * offset, height


def ground_color(i, u):
    d = RUN[i]
    width = ROWS[i][3 if u < 0 else 4]
    palette = [mix(a, b, min(1, max(0, (d - 79) / 83))) for a, b in zip(GREEN, RUIN)]
    palette = [mix(a, b, min(1, max(0, (d - 185) / 106))) for a, b in zip(palette, ASH)]
    distance = abs(u * width - .9 * math.sin(d * .12) - .42 * math.sin(d * .53))
    path = min(1, max(0, (4.1 - distance) / 2.9))
    edge = max(0, abs(u) - .5) * .6
    color = mix(mix(palette[0], palette[2], edge), palette[1], path * .88)
    noise = .91 + .1 * math.sin(d * .72 + u * 22) + .055 * math.cos(d * 2.02 - u * 13)
    return tuple(channel * noise for channel in color)


class Mesh:
    def __init__(self):
        self.vertices = []
        self.colors = []
        self.faces = []

    def vertex(self, xyz, color):
        index = len(self.vertices)
        self.vertices.append(xyz)
        self.colors.append(color)
        return index

    def quad(self, a, b, c, d, flip=False):
        if flip:
            self.faces.extend([(a, b, d), (b, c, d)])
        else:
            self.faces.extend([(a, b, c), (a, c, d)])

    def export(self, name):
        mesh = bpy.data.meshes.new(name)
        # Blender's FBX export into Unreal reverses Y. Mirror vertices and winding here.
        mesh.from_pydata([(x, -y, z) for x, y, z in self.vertices], [],
                         [face[::-1] for face in self.faces])
        mesh.update()
        colors = mesh.color_attributes.new(name='TerrainColor', type='FLOAT_COLOR', domain='CORNER')
        for loop, color in zip(mesh.loops, colors.data):
            color.color = (*self.colors[loop.vertex_index], 1)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(OUTPUT / f'{name}.fbx'), use_selection=True,
                                 mesh_smooth_type='FACE', colors_type='LINEAR', bake_anim=False)
        obj.select_set(False)


def deck(name, vertices, columns):
    mesh = Mesh()
    for row in vertices:
        for point, color in row:
            mesh.vertex(point, color)
    for i in range(len(vertices) - 1):
        for j in range(columns - 1):
            a = i * columns + j
            mesh.quad(a, a + columns, a + columns + 1, a + 1, (i + j) % 2 == 0)
    mesh.export(name)


def surface_segments():
    columns = 23
    cuts = [0, bisect.bisect_left(RUN, 118), bisect.bisect_left(RUN, 218),
            bisect.bisect_left(RUN, 315), len(ROWS) - 1]
    for name, first, last in zip(('Forest', 'Ruins', 'Canyon', 'MountainPass'), cuts, cuts[1:]):
        deck('SM_Ascent' + name, [[(shelf(i, u), ground_color(i, u)) for u in
                                  (-1 + 2 * col / (columns - 1) for col in range(columns))]
                                 for i in range(first, last + 1)], columns)
        flanks = Mesh()
        for sign in (-1, 1):
            base = len(flanks.vertices)
            for i in range(first, last + 1):
                x, y, z = shelf(i, sign)
                nx, ny = normal_at(i)
                depth = 4.5 if 316 < RUN[i] < 362 else 13 + 4 * math.sin(RUN[i] * .093)
                top = ground_color(i, sign)
                for shift, dz, tint in ((0, 0, .94), (2, depth * .24, .59),
                                        (5.8, depth * .62, .40), (9.2, depth, .46)):
                    flanks.vertex((x + nx * sign * shift, y + ny * sign * shift, z - dz),
                                  tuple(c * tint for c in top))
            for i in range(last - first):
                for j in range(3):
                    a = base + i * 4 + j
                    quad = (a, a + 1, a + 5, a + 4)
                    flanks.quad(*(quad if sign < 0 else quad[::-1]), (i + j) % 2 == 0)
        flanks.export('SM_Ascent' + name + 'Banks')


def mountains():
    # Terrain layers frame the near route and connect visually to the original caldera.
    rings = [
        ('ForestFoothills', [(542,-235,30,32,12),(587,-247,51,37,28),(639,-242,50,36,38),
                            (686,-199,42,39,32),(700,-143,28,52,36),(663,-199,26,20,32),
                            (537,-113,38,26,23),(522,-144,24,19,16),(600,-123,14,14,8)],
         (.076,.13,.111)),
        ('RuinsRidge', [(688,-89,43,32,41),(675,-49,31,26,47),(635,-11,35,31,33),
                        (608,-4,28,37,28),(584,11,26,26,35),(565,-102,26,18,22),
                        (571,-25,15,20,17)], (.131,.144,.146)),
        ('CanyonRidge', [(552,30,27,38,57),(523,63,36,45,71),(479,63,41,30,56),
                         (435,53,34,25,46),(414,-64,28,25,28),(452,-78,45,35,58),
                         (503,-75,30,21,33),(470,16,20,13,16)], (.087,.099,.131)),
    ]
    for name, columns, palette in rings:
        mesh = Mesh()
        for x, y, rx, ry, height in columns:
            index = len(mesh.vertices)
            angle = RNG.uniform(0, math.tau)
            pattern = [RNG.uniform(.75, 1.2) for _ in range(9)]
            for tier, (size, z) in enumerate(((1, -25), (.88, -9), (.62, height * .41),
                                               (.43, height * .74), (.16, height))):
                for j, ratio in enumerate(pattern):
                    theta = angle + j * math.tau / len(pattern) + tier * .15
                    r = size * ratio
                    color = tuple(c * (.75 + tier * .11 + RNG.uniform(-.16, .19)) for c in palette)
                    mesh.vertex((x + math.cos(theta) * rx * r + tier * .55,
                                 y + math.sin(theta) * ry * r - tier * .75,
                                 z + RNG.uniform(-3, 3)), color)
            for tier in range(4):
                for j in range(9):
                    a = index + tier * 9 + j
                    b = index + tier * 9 + (j + 1) % 9
                    mesh.quad(a, b, b + 9, a + 9, (tier + j) % 2 == 1)
            peak = mesh.vertex((x + 2.2, y - 3, height + 2), palette)
            mesh.faces.extend((index + 36 + j, index + 36 + (j + 1) % 9, peak) for j in range(9))
        mesh.export('SM_Ascent' + name)


def bridges():
    for name, anchors in BRIDGES.items():
        mesh = Mesh()
        rows = []
        for p, q in zip(anchors, anchors[1:]):
            count = math.ceil(math.dist(p, q) / 3.5)
            rows.extend(mix(p, q, t / count) for t in range(count))
        rows.append(anchors[-1])
        for i, (x, y, z) in enumerate(rows):
            a, b = rows[max(0, i - 1)], rows[min(len(rows) - 1, i + 1)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            length = math.hypot(dx, dy)
            for width, dz, color in ((-4.6, -.55, (.085, .074, .077)),
                                     (-4.6, 0, (.153, .143, .148)),
                                     (-3.9, 0, (.27, .233, .196)),
                                     (0, 0, (.19, .187, .191)),
                                     (3.9, 0, (.27, .233, .196)),
                                     (4.6, 0, (.153, .143, .148)),
                                     (4.6, -.55, (.085, .074, .077))):
                scale = RNG.uniform(.86, 1.14)
                mesh.vertex((x - dy * width / length, y + dx * width / length, z + dz),
                            tuple(c * scale for c in color))
        for i in range(len(rows) - 1):
            for j in range(6):
                a = i * 7 + j
                mesh.quad(a, a + 7, a + 8, a + 1, (i + j) % 2 == 0)
        mesh.export('SM_Ascent' + name)


def basin():
    rows = []
    for x in range(300, 811, 8):
        row = []
        for y in range(-332, 157, 8):
            hill = (math.sin(x * .048 + y * .027) + math.cos(y * .064)) * 3
            z = -29 + hill + 2 * math.sin(x * .087 - y * .046)
            p = mix((.054, .071, .092), (.091, .129, .092), min(1, max(0, (x - 435) / 198)))
            shade = .88 + .21 * math.sin(x * .183 + y * .251)
            row.append(((x, y, z), tuple(c * shade for c in p)))
        rows.append(row)
    deck('SM_AscentFoothillBasin', rows, len(rows[0]))


def prepare_kenney():
    pack_assets = {
        'nature': ('GLTF format', ('tree_pineTallA_detailed', 'tree_pineTallB_detailed',
                   'tree_pineTallD_detailed', 'tree_detailed_dark', 'tree_pineRoundC',
                   'tree_tall_fall', 'plant_bushDetailed', 'plant_bushSmall', 'plant_flatShort',
                   'rock_largeA', 'rock_tallC', 'rock_smallFlatC', 'statue_columnDamaged')),
        'castle': ('GLB format', ('wall-half', 'wall-corner-half', 'wall-pillar',
                   'tower-square-base-border', 'tower-square-mid-windows', 'tower-square-arch',
                   'flag-banner-long', 'wall-doorway')),
    }
    for pack, (extension_folder, names) in pack_assets.items():
        source = OUTPUT.parent / f'{pack}-kit' / 'Models' / extension_folder
        if not source.exists():
            continue
        destination = OUTPUT.parent / 'Converted' / pack
        destination.mkdir(parents=True, exist_ok=True)
        for name in names:
            bpy.ops.object.select_all(action='DESELECT')
            bpy.ops.import_scene.gltf(filepath=str(source / f'{name}.glb'))
            imported = list(bpy.context.selected_objects)
            if pack == 'nature':
                for obj in imported:
                    if obj.type != 'MESH' or not obj.data.materials:
                        continue
                    model = obj.data
                    if model.color_attributes:
                        continue
                    colors = model.color_attributes.new(name='Palette', type='FLOAT_COLOR', domain='CORNER')
                    for face in model.polygons:
                        material = model.materials[face.material_index]
                        shader = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
                        r, g, b, a = shader.inputs['Base Color'].default_value if shader else material.diffuse_color
                        for i in face.loop_indices:
                            colors.data[i].color = (r * .76, g * .9, b * .84, a)
            bpy.ops.export_scene.fbx(filepath=str(destination / f'{name}.fbx'), use_selection=True,
                                     mesh_smooth_type='FACE', colors_type='LINEAR', bake_anim=False)
            for obj in imported:
                bpy.data.objects.remove(obj, do_unlink=True)


surface_segments()
mountains()
bridges()
basin()
(OUTPUT / 'route.json').write_text(json.dumps({'centers': ROWS, 'distances': RUN,
   'shelf': [[shelf(i, j / 10) for j in range(-10, 11)] for i in range(len(ROWS))],
   'bridges': BRIDGES},
   separators=(',', ':')), encoding='utf-8')
prepare_kenney()
