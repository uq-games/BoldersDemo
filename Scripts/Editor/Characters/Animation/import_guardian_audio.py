"""Reimport source-controlled guardian audio in Unreal Editor."""

from pathlib import Path

import unreal as ue


SOURCE = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / 'ArtSources' / 'Audio' / 'VolcanoCombat' / 'Guardians'
DESTINATION = '/Game/BoldersDemo/Assets/Characters/EnemyMushroom/Audio'
NAMES = (
    'SW_GuardianScrapperWarn', 'SW_GuardianScrapperHit',
    'SW_GuardianSporeWarn', 'SW_GuardianSporeLaunch',
    'SW_GuardianBulwarkWarn', 'SW_GuardianBulwarkHit',
)

tasks = []
for name in NAMES:
    source = SOURCE / f'{name}.wav'
    assert source.is_file(), source
    task = ue.AssetImportTask()
    task.set_editor_properties(dict(filename=str(source), destination_path=DESTINATION,
                                    automated=True, replace_existing=True, save=True))
    tasks.append(task)

ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in NAMES:
    sound = ue.load_asset(f'{DESTINATION}/{name}')
    assert isinstance(sound, ue.SoundWave), name
    assert ue.EditorAssetLibrary.save_loaded_asset(sound, False), name

ue.log(f'VOLCANO_GUARDIAN_AUDIO_IMPORTED {len(NAMES)}')
