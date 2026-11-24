"""Render a quiet, seamless furnace bed and a party-defeat cue from scratch."""

import argparse
from array import array
from math import exp, log10, pi, sin, sqrt, tanh
from pathlib import Path
from random import Random
import sys
import wave


RATE = 32000
PERIOD_SECONDS = 44
TAU = 2 * pi


def periodic_lowpass(samples, cutoff):
    """Solve the filter's state at the end of a loop, then render from that state."""
    keep = exp(-TAU * cutoff / RATE)
    blend = 1 - keep
    after_zero_start = 0.0
    for value in samples:
        after_zero_start += blend * (value - after_zero_start)
    state = after_zero_start / (1.0 - keep ** len(samples))
    output = array('f')
    for value in samples:
        state += blend * (value - state)
        output.append(state)
    return output


def periodic_air_band(samples, low, high):
    upper = periodic_lowpass(periodic_lowpass(samples, high), high)
    below = periodic_lowpass(upper, low)
    return array('f', (fast - slow for fast, slow in zip(upper, below)))


def start_loop_at_quiet_boundary(samples):
    """Rotate the periodic bed to a quiet, energy-matched boundary; leave its waveform intact."""
    window = RATE // 20
    scan_length = RATE * 3
    energies = array('d', [0.0])
    for value in samples[:scan_length + window]:
        energies.append(energies[-1] + value * value)
    best = (float('inf'), window)
    for index in range(window, scan_length):
        before = energies[index] - energies[index - window]
        after = energies[index + window] - energies[index]
        drift_db = abs(10.0 * log10((after + 1e-14) / (before + 1e-14)))
        peak = max(abs(samples[index]), abs(samples[index - 1]))
        seam_step = abs(samples[index] - samples[index - 1])
        score = seam_step * 32000 + peak * 2200.0 + drift_db * 5.0
        if score < best[0]:
            best = score, index
    start = best[1]
    return samples[start:] + samples[:start]


def make_furnace_bed():
    frame_count = PERIOD_SECONDS * RATE
    rng = Random(25001123)
    white = array('f', (rng.uniform(-1.0, 1.0) for _ in range(frame_count)))
    chamber = periodic_air_band(white, 35, 132)
    draft = periodic_air_band(white, 122, 485)
    ash = periodic_air_band(white, 410, 1050)
    mix = array('f')
    for index in range(frame_count):
        seconds = index / RATE
        cycle = TAU * index / frame_count
        chamber_weight = 1.08 + .12 * sin(2 * cycle + .35) + .075 * sin(5 * cycle - .11)
        draft_weight = .62 + .14 * sin(3 * cycle + 1.35) + .095 * sin(7 * cycle - .64)
        ash_weight = .082 + .040 * sin(4 * cycle + 2.08)
        distant_resonance = .0018 * sin(TAU * 57 * seconds + .24 * sin(2 * cycle))
        mix.append((chamber[index] * chamber_weight + draft[index] * draft_weight + ash[index] * ash_weight
                    + distant_resonance))

    # Soft limiting preserves heat swells while keeping the bed well below combat cues.
    mean = sum(mix) / len(mix)
    shaped = array('f', (tanh((value - mean) * 3.4) for value in mix))
    mean = sum(shaped) / len(shaped)
    center = array('f', (value - mean for value in shaped))
    rms = sqrt(sum(value * value for value in center) / len(center))
    gain = 10 ** (-35.5 / 20) / rms
    return start_loop_at_quiet_boundary(array('f', (value * gain for value in center)))


def smooth_volume(value):
    fraction = max(0.0, min(1.0, value))
    return fraction * fraction * (3.0 - 2.0 * fraction)


def falling_resonance(seconds, start, frequency, decay, gain):
    age = seconds - start
    if age < 0.0:
        return 0.0
    aperture = smooth_volume(age / .034) * exp(-age / decay)
    phase = TAU * (frequency * age + .32 * (1.0 - exp(-14.0 * age)))
    return gain * aperture * (sin(phase) + .14 * sin(phase * 1.68 + .25) + .08 * sin(phase * 2.91))


def make_defeat_cue():
    duration = .92
    frame_count = round(RATE * duration)
    mix = array('f')
    for index in range(frame_count):
        seconds = index / RATE
        first = falling_resonance(seconds, .064, 203, .16, .73)
        second = falling_resonance(seconds, .255, 137, .24, .68)
        closure = falling_resonance(seconds, .263, 69, .14, .16)
        tail = smooth_volume((duration - seconds) / .132)
        mix.append(tanh(first + second + closure) * tail)
    gain = 10 ** (-13.25 / 20) / max(map(abs, mix))
    return array('f', (value * gain for value in mix))


def write_mono_pcm(path, samples):
    pcm = array('h', (round(value * 32767) for value in samples))
    if sys.byteorder != 'little':
        pcm.byteswap()
    with wave.open(str(path), 'wb') as wav:
        wav.setparams((1, 2, RATE, len(pcm), 'NONE', 'not compressed'))
        wav.writeframes(pcm.tobytes())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).resolve().parent)
    output = parser.parse_args().output_dir
    output.mkdir(parents=True, exist_ok=True)
    write_mono_pcm(output / 'SW_FoundryFurnaceAir_Loop.wav', make_furnace_bed())
    write_mono_pcm(output / 'SW_FoundryPartyDefeat.wav', make_defeat_cue())
