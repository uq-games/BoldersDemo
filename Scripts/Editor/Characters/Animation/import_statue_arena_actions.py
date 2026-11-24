"""Import animation-only FBX authored against the existing statue skeleton and its spatial cues."""

import os
from pathlib import Path

import unreal as ue


DESTINATION = '/Game/BoldersDemo/Assets/Characters/StatueBoss'
PROJECT = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
CLIPS = {'Cleave': 3.7, 'Enrage': 3.8, 'HitFront': 1.1, 'ArenaWalk': 3.6,
         'Awakening': 2.05, 'Defeat': 2.38}
CUES = ('Footfall', 'Axe', 'Stomp', 'Awakening', 'Enrage', 'Defeat')


def task_for(source, folder):
    assert source.is_file(), source
    task = ue.AssetImportTask()
    task.set_editor_properties(dict(filename=str(source), destination_path=folder,
                                    automated=True, replace_existing=True, save=True))
    return task


def run():
    only_defeat = os.getenv('BOLDERS_STATUE_DEFEAT_ONLY') == '1'
    source = Path(os.getenv('BOLDERS_STATUE_ANIMATION_SOURCES',
                            'H:/UE5/AssetSources/BoldersDemo/StatueBoss/ArenaPassV4'))
    clips = {'Defeat': CLIPS['Defeat']} if only_defeat else CLIPS
    skeleton = ue.load_asset(f'{DESTINATION}/SK_StatueBoss_Skeleton')
    assert isinstance(skeleton, ue.Skeleton)
    library = ue.EditorAssetLibrary
    asset_tools = ue.AssetToolsHelpers.get_asset_tools()
    feature = 'Interchange.FeatureFlags.Import.FBX'
    old = ue.SystemLibrary.get_console_variable_bool_value(feature)
    ue.SystemLibrary.execute_console_command(None, feature + ' 0')
    try:
        tasks = []
        for name in clips:
            task = task_for(source / f'A_StatueBoss_{name}.fbx', DESTINATION + '/Animations')
            options = ue.FbxImportUI()
            options.set_editor_properties(dict(automated_import_should_detect_type=False,
                                               mesh_type_to_import=ue.FBXImportType.FBXIT_ANIMATION,
                                               import_as_skeletal=True, import_mesh=False, import_animations=True,
                                               skeleton=skeleton, import_materials=False, import_textures=False))
            options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', True)
            task.options = options
            task.factory = ue.FbxFactory()
            tasks.append(task)
        asset_tools.import_asset_tasks(tasks)
    finally:
        ue.SystemLibrary.execute_console_command(None, feature + (' 1' if old else ' 0'))
    for name, expected in clips.items():
        animation = ue.load_asset(f'{DESTINATION}/Animations/A_StatueBoss_{name}')
        assert isinstance(animation, ue.AnimSequence), name
        assert animation.get_editor_property('skeleton') == skeleton, name
        assert abs(animation.get_play_length() - expected) < .07, (name, animation.get_play_length())
        if only_defeat:
            filename = animation.get_editor_property('asset_import_data').get_first_filename()
            assert Path(filename).resolve() == (source / f'A_StatueBoss_{name}.fbx').resolve(), filename
        assert library.save_loaded_asset(animation, False), name

    if only_defeat:
        ue.log('STATUE_ARENA_DEFEAT_REIMPORTED original package and skeleton, V4 source')
        return

    audio = PROJECT / 'ArtSources' / 'Audio' / 'VolcanoCombat'
    asset_tools.import_asset_tasks([task_for(audio / f'SW_StatueBoss_{name}.wav', DESTINATION + '/Audio') for name in CUES])
    for name in CUES:
        wave = ue.load_asset(f'{DESTINATION}/Audio/SW_StatueBoss_{name}')
        assert isinstance(wave, ue.SoundWave) and library.save_loaded_asset(wave, False), name
    ue.log(f'STATUE_ARENA_ASSETS_READY {len(CLIPS)} clips {len(CUES)} sounds')


if __name__ == '__main__':
    run()
