"""Generate original positional cues for the three mushroom guardian attacks."""

from math import exp, sin, tanh
from pathlib import Path
import struct
import wave

from generate_hazard_audio import NoiseLayers, RATE, TAU, fade_in, grain


OUTPUT = Path(__file__).resolve().parent / 'Guardians'


def scrapper_warn(t, bass, body, mids, top):
    bark = sin(TAU * (228 * t + 5.5 * (1 - exp(-22 * t))))
    rattle = .65 + .35 * sin(TAU * (33 * t + 9 * t * t))
    return (.29 * bark + 1.2 * body + .48 * mids) * rattle * grain(t, 0., .026, .105)


def scrapper_hit(t, bass, body, mids, top):
    rush = (.8 * mids + .34 * top) * grain(t, 0., .012, .075)
    smack = (.22 * sin(TAU * 164 * t) + 2.0 * body + .62 * mids) * grain(t, .028, .003, .047)
    return rush + smack


def spore_warn(t, bass, body, mids, top):
    swell = fade_in(t, .14) * exp(-t / .5)
    phase = TAU * (192 * t + 510 * t * t + 8 * sin(TAU * .34 * t))
    shimmer = .33 * sin(phase) + .11 * sin(1.55 * phase + .49 * sin(TAU * 28 * t))
    return swell * (shimmer + .72 * mids + .20 * top)


def spore_launch(t, bass, body, mids, top):
    pop = (2.4 * body + .69 * top) * grain(t, 0., .003, .032)
    whistle = sin(TAU * (590 * t + 13.3 * (1 - exp(-17 * t))))
    return pop + (.22 * whistle + .15 * mids) * grain(t, .035, .008, .125)


def bulwark_warn(t, bass, body, mids, top):
    groan = sin(TAU * (67 * t + 6.5 * (1 - exp(-6 * t))))
    creaks = (body * 1.36 + .18 * mids) * (grain(t, .07, .017, .08) + grain(t, .28, .018, .075))
    return (.36 * groan + 1.9 * bass) * grain(t, 0., .09, .40) + creaks


def bulwark_hit(t, bass, body, mids, top):
    weight = sin(TAU * (48 * t + 6.7 * (1 - exp(-11 * t))))
    drop = (.55 * weight + 3.0 * bass + .85 * body) * grain(t, 0., .008, .154)
    crumble = (.85 * body + .56 * mids + .29 * top) * (grain(t, .016, .004, .046) + .48 * grain(t, .093, .01, .080))
    return drop + crumble


def write(name, duration, seed, render):
    noise = NoiseLayers(seed)
    samples = []
    for index in range(round(RATE * duration)):
        time = index / RATE
        samples.append(tanh(render(time, *noise.step()) * 1.4) * fade_in(duration - time, .055))
    gain = .72 * 32767 / max(map(abs, samples))
    pcm = struct.pack(f'<{len(samples)}h', *(round(sample * gain) for sample in samples))
    with wave.open(str(OUTPUT / f'{name}.wav'), 'wb') as clip:
        clip.setparams((1, 2, RATE, len(samples), 'NONE', 'not compressed'))
        clip.writeframes(pcm)


if __name__ == '__main__':
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for cue in (
        ('SW_GuardianScrapperWarn', .31, 419, scrapper_warn),
        ('SW_GuardianScrapperHit', .22, 552, scrapper_hit),
        ('SW_GuardianSporeWarn', .55, 79, spore_warn),
        ('SW_GuardianSporeLaunch', .39, 346, spore_launch),
        ('SW_GuardianBulwarkWarn', .58, 951, bulwark_warn),
        ('SW_GuardianBulwarkHit', .56, 238, bulwark_hit),
    ):
        write(*cue)
