#
#  generate_deterministic_wav.py
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-09-27.
#

import math
import random
import struct
import wave

SAMPLE_RATE = 44100
DURATION_SECONDS = 10.0
FRAMES_PER_RENDER_FRAME = 735
RENDER_FPS = 60
TOTAL_SAMPLES = int(SAMPLE_RATE * DURATION_SECONDS)
PRNG_SEED = 20260927

assert SAMPLE_RATE // RENDER_FPS == FRAMES_PER_RENDER_FRAME
assert TOTAL_SAMPLES == FRAMES_PER_RENDER_FRAME * RENDER_FPS * int(DURATION_SECONDS)

BASS_KICK_PERIOD_SECONDS = 0.5
BASS_KICK_FREQUENCY_HZ = 60.0
BASS_KICK_DECAY_SECONDS = 0.12

MID_CHORD_FREQUENCIES_HZ = (220.0, 277.18, 329.63)
MID_CHORD_AMPLITUDE = 0.12

TREBLE_BURST_PERIOD_SECONDS = 1.0
TREBLE_BURST_DURATION_SECONDS = 0.05
TREBLE_BURST_AMPLITUDE = 0.35

SWEEP_START_HZ = 30.0
SWEEP_END_HZ = 16000.0
SWEEP_AMPLITUDE = 0.15


def bass_kick_sample(t):
    phase = t % BASS_KICK_PERIOD_SECONDS
    if phase >= BASS_KICK_DECAY_SECONDS:
        return 0.0
    envelope = math.exp(-phase / (BASS_KICK_DECAY_SECONDS * 0.25))
    return envelope * math.sin(2.0 * math.pi * BASS_KICK_FREQUENCY_HZ * phase)


def mid_chord_sample(t):
    value = 0.0
    for frequency in MID_CHORD_FREQUENCIES_HZ:
        value += math.sin(2.0 * math.pi * frequency * t)
    return (value / len(MID_CHORD_FREQUENCIES_HZ)) * MID_CHORD_AMPLITUDE


def sweep_sample(t):
    progress = t / DURATION_SECONDS
    instantaneous_hz = SWEEP_START_HZ * ((SWEEP_END_HZ / SWEEP_START_HZ) ** progress)
    sweep_k = DURATION_SECONDS / math.log(SWEEP_END_HZ / SWEEP_START_HZ)
    phase = 2.0 * math.pi * SWEEP_START_HZ * sweep_k * (
        (SWEEP_END_HZ / SWEEP_START_HZ) ** progress - 1.0
    )
    return SWEEP_AMPLITUDE * math.sin(phase)


def generate():
    prng = random.Random(PRNG_SEED)
    samples = [0.0] * TOTAL_SAMPLES

    for i in range(TOTAL_SAMPLES):
        t = i / SAMPLE_RATE
        value = bass_kick_sample(t) + mid_chord_sample(t) + sweep_sample(t)

        phase_in_burst_period = t % TREBLE_BURST_PERIOD_SECONDS
        if phase_in_burst_period < TREBLE_BURST_DURATION_SECONDS:
            envelope = 1.0 - (phase_in_burst_period / TREBLE_BURST_DURATION_SECONDS)
            value += TREBLE_BURST_AMPLITUDE * envelope * (prng.uniform(-1.0, 1.0))

        samples[i] = value

    peak = max(abs(s) for s in samples)
    if peak > 1.0:
        samples = [s / peak for s in samples]

    return samples


def write_wav(path, samples):
    with wave.open(path, "wb") as wav_file:
        wav_file.setnchannels(2)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)

        frame_data = bytearray()
        for value in samples:
            clamped = max(-1.0, min(1.0, value))
            sample_int16 = int(clamped * 32767.0)
            frame_data += struct.pack("<hh", sample_int16, sample_int16)

        wav_file.writeframes(bytes(frame_data))


if __name__ == "__main__":
    import os

    output_path = os.path.join(os.path.dirname(__file__), "deterministic-test.wav")
    write_wav(output_path, generate())
    print("Wrote", output_path)
