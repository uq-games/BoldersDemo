"""Generate the three basalt crossings which meet the original island terraces."""
from pathlib import Path
import os

import bpy


output = Path(os.environ['BOLDERS_ISLAND_GEOMETRY'])
output.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
stone = bpy.data.materials.new('BasaltDeck')
trim = bpy.data.materials.new('BasaltCoping')

profiles = {
    'West': (1600, 690, 100, 1120),
    'North': (1380, 610, 32, 493),
    'East': (1370, 640, 44, 532),
}
for side, (length, width, rise_start, rise_end) in profiles.items():
    vertices, faces, slots = [], [], []
    def piece(sections, y0, y1, depth, slot):
        base = len(vertices)
        for x, z in sections:
            vertices.extend(((x, y0, z), (x, y1, z), (x, y1, z-depth), (x, y0, z-depth)))
        faces.append(tuple(base+i for i in (0, 1, 2, 3)))
        slots.append(slot)
        for i in range(len(sections)-1):
            a = base + i*4
            for index in range(4):
                next_index = (index + 1) % 4
                faces.append((a+next_index, a+index, a+index+4, a+next_index+4))
                slots.append(slot)
        last = base + (len(sections)-1)*4
        faces.append(tuple(last+i for i in (3, 2, 1, 0)))
        slots.append(slot)

    def height(x):
        return 252. * min(1., max(0., (x-rise_start)/(rise_end-rise_start)))

    half = width * .5
    # The two flat lips overlap the original rock, including the diagonal edge of the star platform.
    piece([(x, height(x)) for x in (0, rise_start, rise_end, length)], -half, half, 58., 0)
    for index, x in enumerate(range(147, length-200, 147)):
        a = x + (index % 3 - 1) * 5
        b = min(a + 129, length-155)
        split = sorted({a, b, *(k for k in (rise_start, rise_end) if a < k < b)})
        sections = [(k, height(k) + 3 + index % 2) for k in split]
        piece(sections, -half-16, -half+45, 42., 1)
        piece(sections, half-45, half+16, 42., 1)
    mesh = bpy.data.meshes.new('IslandRamp' + side)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    name = 'SM_FoundryIslandRamp_' + side
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for material in (stone, trim):
        mesh.materials.append(material)
    for polygon, slot in zip(mesh.polygons, slots):
        polygon.material_index = slot
    uv = mesh.uv_layers.new(name='Basalt')
    for polygon in mesh.polygons:
        for loop in polygon.loop_indices:
            co = mesh.vertices[mesh.loops[loop].vertex_index].co
            if abs(polygon.normal.x) > .72 and abs(polygon.normal.z) < .65:
                u, v = co.y, co.z
            elif abs(polygon.normal.y) > .72:
                u, v = co.x, co.z
            else:
                u, v = co.x, co.y
            uv.data[loop].uv = (u / 172., v / 173.)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(output / (name + '.fbx')), use_selection=True,
        add_leaf_bones=False, object_types={'MESH'}, use_triangles=True, bake_anim=False,
        axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE',
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL')

