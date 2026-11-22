"""Adjust only the existing AttackHit events; retain all other montage data."""
import unreal

fractions = {'Chop': .56, 'Slice_Horizontal': .18, 'Stab': .30}
root = '/Game/BoldersDemo/Assets/Characters/Warrior/Animations/Montages/AM_Warrior_'
lib = unreal.AnimationLibrary
for name, fraction in fractions.items():
    montage = unreal.load_asset(root + name)
    events = lib.get_animation_notify_events(montage)
    hits = [event for event in events if str(event.get_editor_property('notify_name')) == 'AttackHit']
    assert len(hits) == 1, (name, len(hits))
    tracks = lib.get_animation_notify_track_names(montage)
    assert tracks, name
    assert lib.remove_animation_notify_events_by_name(montage, 'AttackHit') == 1
    notify = lib.add_animation_notify_event(montage, tracks[0], montage.get_play_length() * fraction, unreal.AnimNotify_AttackHit)
    assert notify, name
    unreal.EditorAssetLibrary.save_loaded_asset(montage, False)
    print('HIT_TIMING', name, montage.get_play_length(), fraction, montage.get_play_length() * fraction)
