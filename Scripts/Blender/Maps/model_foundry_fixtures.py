"""Bake the four Foundry fixtures and their state meshes in centimetres."""
from math import cos, pi, sin
import os
from pathlib import Path

import bpy


output = Path(os.environ.get('BOLDERS_FOUNDRY_MODEL_SOURCE',
    'H:/UE5/AssetSources/BoldersDemo/VolcanoCombat/Fixtures'))
output.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
materials = {name: bpy.data.materials.new(name) for name in
             ('Basalt', 'Iron', 'Bronze', 'Wood', 'EndGrain', 'Ember', 'Ceramic', 'Crystal')}
parts = []


def add(obj, material):
    obj.data.materials.append(materials[material])
    parts.append(obj)
    return obj


def bevel(obj, width):
    bpy.context.view_layer.objects.active = obj
    beveler = obj.modifiers.new('Cast edges', 'BEVEL')
    beveler.width = width
    beveler.segments = 2
    bpy.ops.object.modifier_apply(modifier=beveler.name)


def box(location, dimensions, material, edge=1., yaw=0.):
    bpy.ops.mesh.primitive_cube_add(size=1., location=location)
    obj = bpy.context.object
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.rotation_euler.z = yaw
    if edge > 0:
        bevel(obj, edge)
    return add(obj, material)


def geometry(vertices, faces, material):
    mesh = bpy.data.meshes.new(material)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name='Fixture')
    for face in mesh.polygons:
        for idx in face.loop_indices:
            x, y, z = mesh.vertices[mesh.loops[idx].vertex_index].co
            uv.data[idx].uv = ((y if abs(face.normal.x) > .8 else x) / 55.,
                              (y if abs(face.normal.z) > .8 else z) / 55.)
    obj = bpy.data.objects.new(material, mesh)
    bpy.context.collection.objects.link(obj)
    return add(obj, material)


def lathe(profile, material, sides=16, center=(0, 0), closed=False):
    vertices = [(center[0] + r*cos(2*pi*i/sides), center[1] + r*sin(2*pi*i/sides), z)
                for z, r in profile for i in range(sides)]
    sections = len(profile)
    faces = [(i*sides+j, i*sides+(j+1)%sides, ((i+1)%sections)*sides+(j+1)%sides, ((i+1)%sections)*sides+j)
             for i in range(sections if closed else sections-1) for j in range(sides)]
    if not closed:
        faces += [tuple(reversed(range(sides))), tuple(range((sections-1)*sides, sections*sides))]
    return geometry(vertices, faces, material)


def finish(name):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = 'SM_Foundry' + name
    scene.cursor.location = (0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.export_scene.fbx(filepath=str(output / (obj.name + '.fbx')), use_selection=True,
        object_types={'MESH'}, bake_anim=False, axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE',
        use_triangles=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL')
    bpy.data.objects.remove(obj, do_unlink=True)
    parts.clear()


# Recessed firebox under a replaceable grate. Four bronze fasteners locate its frame.
box((0,0,7), (128,108,20), 'Basalt', 6)
box((0,0,14), (92,73,3), 'Iron')
for side in (-1,1):
    box((side*52,0,17), (13,90,15), 'Iron', 3)
    box((0,side*40,17), (101,11,15), 'Iron', 2)
    box((side*41,0,9), (16,75,7), 'Ember', 2)
    for y in (-40,40):
        lathe([(24,4),(28,4),(29,2.7)], 'Bronze', 8, (side*51,y))
for i in range(-3,4):
    box((i*12,0,19), (5,73,5), 'Iron', 1)
finish('PressureVent')

# Seven basin bevel levels meet the exact eight-sided wet footprint at Radius=160.
def outline(length, width, cut):
    return [(length-cut,-width),(length,-width+cut),(length,width-cut),(length-cut,width),
            (-length+cut,width),(-length,width-cut),(-length,-width+cut),(-length+cut,-width)]


def band(rings, material):
    corners = [outline(160+offset, 99.2+offset, 30.4) for offset,z in rings]
    vertices = [(x,y,z) for polygon,(_,z) in zip(corners,rings) for x,y in polygon]
    faces = [(i*8+j,i*8+(j+1)%8,(i+1)*8+(j+1)%8,(i+1)*8+j)
             for i in range(len(rings)-1) for j in range(8)]
    return geometry(vertices, faces, material)


band([(25,-3),(28,3),(24,14),(17,21),(9,21),(4,16),(1,7)], 'Basalt')
band([(12,21.1),(8,21.1)], 'Bronze')
for x in (-110,110):
    for y in (-1,1):
        box((x,y*117,19), (20,9,4), 'Bronze', 1.5)
for x in (-170,170):
    box((x,0,12), (14,36,18), 'Iron', 2)
    for y in (-10,10):
        lathe([(20,2.8),(23,2.8)], 'Bronze', 8, (x,y))
finish('CoolingTrough')

# Separate stave surfaces alternate in width. The two hoop assemblies have real bevels.
lathe([(0,31),(4,38),(26,43),(59,47),(86,43),(108,36),(114,33)], 'Wood', 16)
for i in range(16):
    a = i*2*pi/16
    box((45*cos(a),45*sin(a),57), (2., .7, 55), 'EndGrain', .20, a)
for z,r in ((18,42),(86,45)):
    lathe([(z-5,r-1),(z-4,r+1.8),(z+3,r+1.8),(z+5,r-1)], 'Iron', 16, closed=True)
    for i in range(8):
        a = i*pi/4
        lathe([(z+2,2.8),(z+5,2.8)], 'Bronze', 8, (r*cos(a),r*sin(a)))
lathe([(108,35),(115,36),(116,32)], 'EndGrain', 16)
for i in range(-2,3):
    box((i*10,0,116.4),(1.05,57-abs(i)*7,.6),'Wood',.15)
lathe([(115,12),(121,11),(123,8)], 'Bronze', 10, (0,0))
box((0,0,126),(4,4,8),'Ember',1)
finish('PowderKeg')

for i, dimensions in enumerate(((37,14,7),(53,14,7),(65,11,8),(41,13,6),(22,17,9))):
    a = i*2*pi/5+.30
    r = 29+7*(i%2)
    box((r*cos(a),r*sin(a),dimensions[2]*.39), dimensions, 'Wood' if i%3 else 'Iron', 1, a+.48)
lathe([(0,24),(2,30),(4,29),(5,23)], 'Iron', 12, closed=True)
finish('PowderKegSpent')

# Insulating rings and bracket claws leave a clear silhouette around the replaceable crystal.
lathe([(0,51),(4,57),(13,57),(24,42),(30,39)], 'Basalt', 8)
lathe([(28,31),(34,36),(38,33),(46,21)], 'Bronze', 12)
for height in (52,61,70):
    lathe([(height-5,16),(height-4,24),(height+2,25),(height+5,17)], 'Ceramic', 12)
lathe([(72,16),(81,20),(84,25),(89,22)], 'Bronze', 12)
for i in range(4):
    a = pi/4 + i*pi/2
    box((44*cos(a),44*sin(a),38),(16,14,60),'Iron',2.5,a)
    box((39*cos(a),39*sin(a),72),(22,12,11),'Bronze',2,a)
    lathe([(70,4),(79,4),(82,2)],'Bronze',8,(43*cos(a),43*sin(a)))
finish('ArcPylon')
lathe([(-32,1),(-17,15),(-3,22),(13,16),(36,1)], 'Crystal', 7)
finish('ArcCrystal')

