"""Bake the reviewed Blend actions required by the island encounter to centimetre FBX."""
import json
from math import radians
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector


source = Path(bpy.data.filepath).parent
out = source / 'UE5'
out.mkdir(exist_ok=True)
scene = bpy.context.scene
rig = bpy.data.objects['Statue Knight | Mixamo Rig']
axe = bpy.data.objects['Statue Knight | Stone Axe']
rig.name = 'Armature'
animation = rig.animation_data
animation.use_nla = False
fps = scene.render.fps
assert fps == 30 and scene.unit_settings.scale_length == 1


def assign(action):
    animation.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    animation.action = action
    animation.action_slot = action.slots[0]


def curves(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                yield from bag.fcurves


def smooth(low, high, time):
    x = min(1., max(0., (time - low) / (high - low)))
    return x * x * (3. - 2. * x)


def build_leap():
    action = bpy.data.actions['11_AxeLeap'].copy()
    assign(action)
    hip = rig.pose.bones['mixamorig:Hips']
    parent_axes = hip.bone.matrix_local.to_quaternion().inverted()
    source_start, source_end = map(int, action.frame_range)
    for frame in range(source_start, source_end + 1):
        scene.frame_set(frame)
        seconds = (frame - source_start) / fps
        progress = .48 * smooth(.45, 1.60, seconds) + .52 * smooth(2.08, 3.52, seconds)
        hip.location += parent_axes @ Vector((0., 1.08 * progress, 0.))
        hip.keyframe_insert(data_path='location', frame=frame, group=hip.name)
    return action


def transition(name, duration, lean):
    action = bpy.data.actions['01_Idle'].copy()
    action.name = name
    assign(action)
    last = round(duration * fps) + 1
    for frame in range(1, last + 1):
        scene.frame_set(frame)
        time = (frame - 1) / fps
        amount = lean(time)
        rotations = []
        for name, bend in (('Spine', .42), ('Spine2', .58), ('Head', 1.15)):
            bone = rig.pose.bones['mixamorig:' + name]
            axis = bone.matrix.to_quaternion().inverted() @ Vector((1., 0., 0.))
            rotations.append((bone, bone.rotation_quaternion @ Quaternion(axis, radians(amount * bend))))
        for bone, rotation in rotations:
            bone.rotation_quaternion = rotation
            bone.keyframe_insert(data_path='rotation_quaternion', frame=frame, group=bone.name)
    return action, last


leap = build_leap()
awake, awake_end = transition('IslandAwakening', 2.10, lambda t: 16 * (1. - smooth(.25, 1.93, t)))
enrage, enrage_end = transition('IslandEnrage', 1.73, lambda t:
    12 * (smooth(0., .44, t) - smooth(.48, .92, t))
    - 9 * (smooth(.65, 1.01, t) - smooth(1.11, 1.69, t)))
actions = {}
for export, action in {
    'Idle': '01_Idle', 'Walk': '02_Walk', 'Hit': '04_Hit', 'Defeat': '05_Defeat',
}.items():
    a = bpy.data.actions[action]
    actions[export] = (a, *map(int, a.frame_range))
actions['Awakening'] = (awake, 1, awake_end)
actions['Enrage'] = (enrage, 1, enrage_end)
actions['LeapTravel'] = (leap, *map(int, leap.frame_range))

bpy.ops.object.select_all(action='DESELECT')
rig.hide_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
summary = {}
for name, (action, first, end) in actions.items():
    assign(action)
    grips, hips = [], []
    for f in range(first, end + 1):
        scene.frame_set(f)
        hips.append(list(rig.pose.bones['mixamorig:Hips'].head))
        grips.append(min((axe.matrix_world @ v.co).z for v in axe.data.vertices))
    summary[name] = {
        'frames': end - first + 1, 'clip': action.name, 'hips_m': [hips[0], hips[-1]],
        'axe_minimum_height_m': min(grips),
    }
    assert min(grips) >= -.015, (name, min(grips))

rig.data.transform(Matrix.Scale(100., 4))
scene.unit_settings.scale_length = .01
reference = {'clips': summary, 'fps': fps}
for name, (action, first, end) in actions.items():
    for curve in curves(action):
        if curve.data_path.endswith('location'):
            for key in curve.keyframe_points:
                key.co.y *= 100.
                key.handle_left.y *= 100.
                key.handle_right.y *= 100.
    assign(action)
    scene.frame_start, scene.frame_end = first, end
    scene.frame_set(first)
    bpy.ops.export_scene.fbx(
        filepath=str(out / f'A_StatueBoss_Mixamo_{name}.fbx'),
        use_selection=True, object_types={'ARMATURE'}, add_leaf_bones=False,
        use_armature_deform_only=False, bake_anim=True, bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z',
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL')
    sample_points = {}
    for f in sorted({first, (first + end)//2, end}):
        scene.frame_set(f)
        sample_points[f-first] = {bone.name: list(bone.head) for bone in rig.pose.bones}
    summary[name]['cm_samples'] = sample_points

(out / 'IslandCombatMotion.json').write_text(json.dumps(reference, indent=2), encoding='utf-8')

