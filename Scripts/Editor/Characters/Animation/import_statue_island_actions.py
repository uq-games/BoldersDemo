"""Import the island combat clips with the axis calibration of the reviewed Mixamo skeleton."""
import json
import os
from pathlib import Path

import unreal as ue

BOSS = '/Game/BoldersDemo/Assets/Characters/StatueBoss/MixamoAxe'
ANIMATIONS = BOSS + '/Animations'


def run():
    source = Path(os.getenv('BOLDERS_MIXAMO_SOURCES',
        'H:/UE5/AssetSources/BoldersDemo/StatueBoss/TPose_Mixamo/AxePack/UE5'))
    spec = json.loads((source / 'IslandCombatMotion.json').read_text(encoding='utf-8'))
    mesh = ue.load_asset(BOSS + '/SK_StatueBoss_Mixamo')
    skeleton = mesh.get_editor_property('skeleton')
    library = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    feature = 'Interchange.FeatureFlags.Import.FBX'
    old = ue.SystemLibrary.get_console_variable_bool_value(feature)
    ue.SystemLibrary.execute_console_command(None, feature + ' 0')
    pose_options = ue.AnimPoseEvaluationOptions()
    pose_options.set_editor_properties(dict(optional_skeletal_mesh=mesh, should_retarget=False,
        evaluation_type=ue.AnimDataEvalType.COMPRESSED))
    results = {}
    try:
        for label, meta in spec['clips'].items():
            name = 'A_StatueBoss_Mixamo_' + label
            path = ANIMATIONS + '/' + name
            options = ue.FbxImportUI()
            options.set_editor_properties(dict(automated_import_should_detect_type=False,
                mesh_type_to_import=ue.FBXImportType.FBXIT_ANIMATION, import_as_skeletal=True,
                import_mesh=False, import_animations=True, skeleton=skeleton, import_materials=False,
                import_textures=False))
            settings = options.anim_sequence_import_data
            settings.set_editor_properties(dict(use_default_sample_rate=True, convert_scene=True,
                force_front_x_axis=True, convert_scene_unit=True,
                animation_length=ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME))
            task = ue.AssetImportTask()
            task.set_editor_properties(dict(filename=str(source / (name + '.fbx')), destination_path=ANIMATIONS,
                destination_name=name, automated=True, replace_existing=True, replace_existing_settings=True,
                save=True, factory=ue.FbxFactory(), options=options))
            if not library.does_asset_exist(path):
                tools.import_asset_tasks([task])
                assert task.imported_object_paths, label
            anim = ue.load_asset(path)
            assert anim.get_editor_property('skeleton') == skeleton
            # The legacy animation-only factory reads the persisted transform during reimport.
            data = anim.get_editor_property('asset_import_data')
            data.set_editor_property('import_rotation', ue.Rotator(yaw=-90))
            tools.import_asset_tasks([task])
            assert library.save_loaded_asset(anim, False)
            expected = (meta['frames'] - 1) / spec['fps']
            assert abs(anim.get_play_length() - expected) < .001, label
            error = 0.
            for frame, bones in meta['cm_samples'].items():
                pose = ue.AnimPoseExtensions.get_anim_pose_at_frame(anim, int(frame), pose_options)
                for bone, (x, y, z) in bones.items():
                    t = ue.AnimPoseExtensions.get_bone_pose(pose, bone.split(':')[-1], ue.AnimPoseSpaces.WORLD)
                    error = max(error, (t.translation - ue.Vector(-y, -x, z)).length())
            assert error < .16, (label, error)
            results[label] = {'seconds': anim.get_play_length(), 'sampled_bone_error_cm': error}
    finally:
        ue.SystemLibrary.execute_console_command(None, feature + (' 1' if old else ' 0'))
    work = Path(ue.Paths.project_saved_dir()) / 'IslandBossRework'
    work.mkdir(exist_ok=True)
    (work / 'mixamo_motion_import.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print('ISLAND_MIXAMO_CLIPS_OK', results)


if __name__ == '__main__':
    run()

