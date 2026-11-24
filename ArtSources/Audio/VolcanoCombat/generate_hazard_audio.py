"""Render original, deterministic volcano gameplay cues to mono PCM WAV."""

from math import exp, pi, sin, tanh
from pathlib import Path
from random import Random
import struct
import wave


RATE = 32000
TAU = 2 * pi
OUTPUT = Path(__file__).resolve().parent


class NoiseLayers:
    def __init__(self, seed):
        self.random = Random(seed)
        self.cutoffs = [68, 390, 1850, 6500]
        self.state = [0.] * len(self.cutoffs)
        self.coefficients = [1 - exp(-TAU * cutoff / RATE) for cutoff in self.cutoffs]

    def step(self):
        white = self.random.uniform(-1., 1.)
        for index, step in enumerate(self.coefficients):
            self.state[index] += step * (white - self.state[index])
        bass, body, mids, top = self.state
        return bass, body - bass, mids - body, top - mids


def fade_in(t, attack):
    return min(1., max(0., t / attack)) ** 1.5


def grain(t, start, attack, decay):
    age = t - start
    return fade_in(age, attack) * exp(-max(0., age) / decay)


def eruption(t, bass, body, mids, top):
    rise = fade_in(t, .135)
    surge = .6 + .4 * sin(TAU * (2.9 * t + .2 * sin(TAU * .7 * t))) ** 2
    roar = (3.3 * bass + .9 * body + .32 * mids) * surge * rise * exp(-t / .71)
    sub = .19 * sin(TAU * (43 * t + 8 * (1 - exp(-4 * t)))) * rise * exp(-t / .31)
    jet = (.48 * mids + .38 * top) * grain(t, .13, .09, .63)
    debris = body * (2.3 * grain(t, .24, .007, .068) + 1.9 * grain(t, .52, .006, .084))
    return roar + sub + jet + debris


def barrel_blast(t, bass, body, mids, top):
    transients = (.45 * mids + .29 * top) * grain(t, 0., .0017, .016)
    shell = .6 * (body + mids) * (
        grain(t, .046, .005, .041) + .64 * grain(t, .12, .004, .08))
    low = sin(TAU * (35 * t + 9.4 * (1 - exp(-5 * t))))
    thump = (.51 * low + 2.8 * bass + 1.48 * body) * grain(t, 0., .011, .145)
    tail = (2.3 * bass + .7 * body) * grain(t, .03, .025, .26)
    return transients + shell + thump + tail


def orb_discharge(t, bass, body, mids, top):
    sparks = 0.
    for start, pitch, gain in ((.0, 710., 1.), (.045, 1180., .64), (.115, 570., .58), (.215, 960., .33)):
        age = max(0., t - start)
        phase = .58 * pitch * age + .42 * pitch * (1 - exp(-24 * age)) / 24
        bent = sin(TAU * phase + 1.1 * sin(TAU * 114 * age + 1.4 * sin(TAU * 317 * age)))
        sparks += gain * grain(t, start, .004, .07) * (.30 * bent + .63 * mids + .51 * top)
    thud = (1.9 * bass + .1 * sin(TAU * 81 * t)) * grain(t, 0., .009, .14)
    return sparks + thud + .22 * body * grain(t, .09, .04, .13)


def water_steam(t, bass, body, mids, top):
    spray = (.36 * body + .95 * mids + .52 * top) * grain(t, 0., .082, .27)
    turbulence = .67 + .33 * sin(TAU * (7 * t + .3 * sin(TAU * 4 * t)))
    bubbling = (.55 * body + 1.3 * bass) * grain(t, 0., .023, .16)
    return spray * turbulence + bubbling


def water_freeze(t, bass, body, mids, top):
    chimes = 0.
    for start, pitch, gain in ((0., 620., 1.), (.047, 960., .68), (.118, 1380., .54)):
        age = max(0., t - start)
        ping = .57 * sin(TAU * pitch * age) + .18 * sin(TAU * pitch * 1.713 * age)
        chimes += gain * (ping + .24 * mids) * grain(t, start, .003, .10)
    crack = (2.5 * bass + .79 * body + .59 * top) * grain(t, .016, .002, .046)
    return chimes + crack


def write_clip(name, seconds, seed, peak, render):
    noise = NoiseLayers(seed)
    samples = []
    for index in range(round(seconds * RATE)):
        t = index / RATE
        signal = render(t, *noise.step())
        samples.append(tanh(signal * 1.6) * fade_in(seconds - t, .09))
    scale = peak * 32767 / max(map(abs, samples))
    pcm = struct.pack(f"<{len(samples)}h", *(round(sample * scale) for sample in samples))
    with wave.open(str(OUTPUT / f"{name}.wav"), "wb") as clip:
        clip.setparams((1, 2, RATE, len(samples), "NONE", "not compressed"))
        clip.writeframes(pcm)


if __name__ == "__main__":
    for cue in (
        ("SW_VolcanoEruption", 1.45, 61, .63, eruption),
        ("SW_VolcanoBarrelBlast", .84, 42, .78, barrel_blast),
        ("SW_VolcanoOrbDischarge", .47, 103, .57, orb_discharge),
        ("SW_VolcanoWaterSteam", .93, 79, .43, water_steam),
        ("SW_VolcanoWaterFreeze", .53, 812, .46, water_freeze),
    ):
        write_clip(*cue)
