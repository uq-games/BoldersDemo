"""Export the arena animations against the existing 21-bone StatueBoss rig.

Run in Blender with the AnimationWorkshop .blend and an output directory after --.
The workshop and the original V2 source are opened read-only.
"""

import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector


workshop, output = (Path(arg) for arg in sys.argv[sys.argv.index('--') + 1:])
output.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(workshop))
scene = bpy.context.scene
rig = bpy.data.objects['Armature']
body = bpy.data.objects['SK_StatueBoss']
axe = bpy.data.objects['SM_StatueBoss_Axe']
raw_axe = [vertex.co.copy() for vertex in axe.data.vertices]
hand_rest = rig.data.bones['hand_r'].matrix_local.copy()
blade_rest = (hand_rest.to_quaternion() @ max(raw_axe, key=lambda vertex: vertex.length)).normalized()

# The workshop's pose solver uses the rig, mesh and math types above.
solver = workshop.parent / 'Tools' / 'pose_helpers.py'
exec(compile(solver.read_text(encoding='utf-8-sig'), str(solver), 'exec'))
rig.animation_data_clear()
base(lean=6, look=10)
carry = rig.pose.bones['hand_r'].head.copy()
left_carry = rig.pose.bones['hand_l'].head.copy()


def pose(pelvis=(0, 0, -.20), lean=6., twist=0., look=10., hand=carry,
         blade=(-.22, .05, -1), left=left_carry, right_step=(0, 0, 0), left_step=(0, 0, 0)):
    return (Vector(pelvis), lean, twist, look, Vector(hand), Vector(blade), Vector(left),
            Vector(right_step), Vector(left_step))


def apply_pose(values):
    hips, lean, twist, look, hand, blade, left, right_step, left_step = values
    base(pelvis=hips, lean=lean, twist=twist, look=look, hand_target=hand, blade_dir=blade)
    shoulder = rig.pose.bones['upperarm_l'].head.copy()
    ik('upperarm_l', 'lowerarm_l', 'hand_l', left, shoulder + Vector((.8, .7, -.5)))
    legs(feet0['r'] + right_step, feet0['l'] + left_step)
    floor_shift(['r', 'l'])


neutral = pose()
asleep = pose(lean=11., look=30., hand=carry + Vector((-.05, .05, 0)), left=left_carry)


def poses_between(points, t):
    for (start, a), (end, b) in zip(points, points[1:]):
        if t <= end:
            blend = smooth((t - start) / (end - start))
            return tuple(x + (y - x) * blend for x, y in zip(a, b))
    return points[-1][1]


def awakening(t):
    return poses_between((
        (0., asleep), (.23, asleep),
        (.86, pose(lean=-3., twist=-7., look=-12., hand=carry + Vector((-.28, .11, .11)),
                   left=left_carry + Vector((.20, -.14, .42)))),
        (1.27, pose(lean=7., twist=4., look=2., left=left_carry + Vector((.13, -.12, .16)))),
        (2.05, neutral),
    ), t)


def walk(t):
    phase = t / 3.6
    swing = math.sin(2. * math.pi * phase)

    def step(offset):
        q = (phase + offset) % 1.
        if q < .58:
            return 0., -.98 + 1.96 * q / .58, 0.
        flight = (q - .58) / .42
        arc = math.sin(math.pi * flight) ** 1.28
        return 0., .98 - 1.96 * smooth(flight), .38 * arc

    return pose(pelvis=(.17 * math.cos(2. * math.pi * phase), 0., -.28), lean=8., look=11.,
                twist=3.7 * swing, hand=carry + Vector((-.05, .19 * swing, .035)),
                left=left_carry + Vector((.035, -.17 * swing, .04)), right_step=step(0.), left_step=step(.5))


def defeat(t):
    reel = pose(pelvis=(.05, .12, -.27), lean=-9, twist=15, look=-9,
                hand=carry + Vector((-.37, .29, .08)), blade=(-.74, .34, -.61),
                left=left_carry + Vector((.20, -.12, .55)))
    failing = pose(pelvis=(.21, -.19, -.78), lean=32, twist=12, look=20,
                   hand=(-1.55, -.43, 1.86), blade=(-1, -.20, -.61), left=(1.45, -.75, 1.82))
    contact = pose(pelvis=(.23, -.35, -1.92), lean=58, twist=10, look=52,
                   hand=(-1.46, -1.00, .89), blade=(-1, -.08, -.30), left=(1.22, -1.62, .51))
    rebound = pose(pelvis=(.25, -.35, -1.86), lean=51, twist=11, look=55,
                   hand=(-1.50, -.95, 1.03), blade=(-1, -.075, -.21), left=(1.17, -1.64, .53))
    values = poses_between(((0., neutral), (.24, reel), (.75, failing), (1.13, contact),
                            (1.31, rebound), (1.92, contact), (2.38, contact)), t)

    # Reestablish support under the torso before dropping it. Keeping the back foot
    # behind the hips in the deep pose drives the oversized calf armor through the floor.
    def catch_step(start, end, forward, lift):
        progress = max(0., min(1., (t - start) / (end - start)))
        return Vector((0., -forward * smooth(progress), lift * math.sin(math.pi * progress)))

    right = catch_step(.53, .83, .72, .19)
    left = catch_step(.22, .51, 1.15, .25)
    return (*values[:7], right, left)


def author(name, duration, motion):
    rig.animation_data_clear()
    measurements = []
    for frame in range(1, round(duration * 30) + 2):
        apply_pose(motion((frame - 1) / 30.))
        key(frame)
        grip = rig.pose.bones['hand_r'].matrix
        row = {'frame': frame, 'foot_l': list(rig.pose.bones['foot_l'].head),
               'foot_r': list(rig.pose.bones['foot_r'].head),
               'axe_min_z': min((grip @ vertex).z for vertex in raw_axe)}
        if name == 'Defeat':
            evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = evaluated.to_mesh()
            row['body_min_z'] = min(vertex.co.z for vertex in mesh.vertices)
            row['body_vertices_below_2cm'] = sum(vertex.co.z < -.02 for vertex in mesh.vertices)
            evaluated.to_mesh_clear()
            row['head_z'] = rig.pose.bones['head'].head.z
            row['pelvis_z'] = rig.pose.bones['pelvis'].head.z
        measurements.append(row)
    finish(name)
    return actions[name], measurements


clips = {short: bpy.data.actions[f'A_StatueBoss_{short}'] for short in ('Cleave', 'Enrage', 'HitFront')}
samples = {}
for short, seconds, motion in (('ArenaWalk', 3.6, walk), ('Awakening', 2.05, awakening), ('Defeat', 2.38, defeat)):
    clips[short], samples[short] = author(short, seconds, motion)

for row in samples['Defeat']:
    assert row['body_min_z'] >= -.006 and row['body_vertices_below_2cm'] == 0 and row['axe_min_z'] >= .035, row

# Measure interpolation as well as keys. Floor support judged by the evaluated skin avoids
# accepting a planted ankle whose oversized calf armor penetrates the combat floor.
rig.animation_data.action = clips['Defeat']
interpolated_body_floor = math.inf
interpolated_axe_floor = math.inf
for frame in range(1, int(clips['Defeat'].frame_range[1])):
    for fraction in (.25, .50, .75):
        scene.frame_set(frame, subframe=fraction)
        bpy.context.view_layer.update()
        evaluated = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        interpolated_body_floor = min(interpolated_body_floor, min(vertex.co.z for vertex in mesh.vertices))
        evaluated.to_mesh_clear()
        grip = rig.pose.bones['hand_r'].matrix
        interpolated_axe_floor = min(interpolated_axe_floor, min((grip @ vertex).z for vertex in raw_axe))
assert interpolated_body_floor >= -.006 and interpolated_axe_floor >= .035, (interpolated_body_floor, interpolated_axe_floor)
samples['DefeatInterpolation'] = dict(step=.25, body_min_z=interpolated_body_floor, axe_min_z=interpolated_axe_floor)

rig.animation_data_clear()
for bone in rig.pose.bones:
    bone.matrix_basis = Matrix.Identity(4)
rig.data.transform(Matrix.Scale(100, 4))
for clip in clips.values():
    for layer in clip.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    if curve.data_path.endswith('location'):
                        for keyframe in curve.keyframe_points:
                            keyframe.co.y *= 100
                            keyframe.handle_left.y *= 100
                            keyframe.handle_right.y *= 100

scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
scene.render.fps = 30
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
rig.animation_data_create()
for clip in clips.values():
    rig.animation_data.action = clip
    scene.frame_start, scene.frame_end = map(int, clip.frame_range)
    bpy.ops.export_scene.fbx(filepath=str(output / f'{clip.name}.fbx'), use_selection=True,
                            object_types={'ARMATURE'}, add_leaf_bones=False, bake_anim=True,
                            bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                            bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z',
                            apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL')

(output / 'authored_pose_samples.json').write_text(json.dumps(samples, indent=2), encoding='utf-8')
