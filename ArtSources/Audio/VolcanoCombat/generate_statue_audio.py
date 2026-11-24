"""Render deterministic, original stone and metal cues for the Caldera Knight."""

from math import exp, sin

from generate_hazard_audio import TAU, grain, write_clip


def resonant(t, pitch, decay, overtone=1.57):
    return (sin(TAU * pitch * t) + .27 * sin(TAU * pitch * overtone * t)) * exp(-t / decay)


def foot(t, bass, body, mids, top):
    contact = (2.5 * bass + 1.3 * body + .31 * mids + .24 * resonant(t, 72, .085)) * grain(t, 0, .005, .11)
    fragments = (.31 * mids + .14 * top) * (grain(t, .048, .004, .048) + .28 * grain(t, .11, .006, .062))
    return contact + fragments


def axe(t, bass, body, mids, top):
    strike = (mids + .64 * top + .58 * resonant(t, 118, .21)) * grain(t, 0, .002, .10)
    grit = (.75 * mids + .48 * body) * grain(t, .036, .009, .16)
    thud = (3.1 * bass + .81 * body + .19 * resonant(t, 57, .17)) * grain(t, .005, .007, .21)
    return strike + grit + thud


def stomp(t, bass, body, mids, top):
    start = (3.7 * bass + 1.34 * body + .31 * resonant(t, 49, .20)) * grain(t, 0, .009, .23)
    rattle = (mids + .23 * top) * (grain(t, .02, .006, .06) + .63 * grain(t, .08, .004, .12))
    return start + rattle + .77 * body * grain(t, .20, .09, .27)


def grind(t, bass, body, mids, top):
    grind_modulation = .56 + .44 * sin(TAU * (5.4 * t + .21 * sin(TAU * 1.65 * t))) ** 2
    return ((2.05 * bass + 1.24 * body + .75 * mids) * grind_modulation * grain(t, 0, .17, .52)
            + .42 * top * (grain(t, .48, .005, .094) + grain(t, .71, .006, .11)))


def roar(t, bass, body, mids, top):
    age = max(0., t - .21)
    rising_phase = 47 * age + 9.6 * (1 - exp(-3.2 * age))
    furnace = .24 * sin(TAU * rising_phase + 1.17 * sin(TAU * 16.7 * age))
    split_grain = (2.8 * bass + 1.26 * body + .66 * mids) * grain(t, .23, .16, .46)
    return .62 * grind(t, bass, body, mids, top) + (furnace + split_grain) * grain(t, .21, .18, .67)


def collapse(t, bass, body, mids, top):
    rubble = foot(t, bass, body, mids, top)
    rubble += (.79 * mids + .23 * top) * grain(t, .18, .013, .14)
    for delay, gain in ((.11, .64), (.25, .51), (.39, .38), (.58, .18)):
        rubble += gain * (2.4 * bass + .66 * body + .3 * mids) * grain(t, delay, .007, .10)
    return rubble


if __name__ == '__main__':
    for cue in (
        ('SW_StatueBoss_Footfall', .49, 417, .54, foot),
        ('SW_StatueBoss_Axe', .65, 31, .71, axe),
        ('SW_StatueBoss_Stomp', .88, 963, .72, stomp),
        ('SW_StatueBoss_Awakening', 1.36, 66, .60, grind),
        ('SW_StatueBoss_Enrage', 1.58, 226, .68, roar),
        ('SW_StatueBoss_Defeat', .96, 864, .74, collapse),
    ):
        write_clip(*cue)
