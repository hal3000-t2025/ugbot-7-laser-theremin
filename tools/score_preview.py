#!/usr/bin/env python3
"""Render firmware scores into directly playable WAV previews."""

from __future__ import annotations

from array import array
from dataclasses import dataclass
import math
from pathlib import Path
import sys
import wave


AUDIO_SAMPLE_RATE = 32000
WAVETABLE_SIZE = 128
MAX_OUTPUT_VOLUME = 0.14
SCORE_PITCH_GLIDE_ALPHA = 0.18
SCORE_VOLUME_ATTACK_ALPHA = 0.28
SCORE_VOLUME_RELEASE_ALPHA = 0.36
SCORE_SMOOTHING_REFERENCE_MS = 10.0
PREVIEW_RELEASE_TAIL_MS = 240
EMBEDDED_SAMPLE_RATE_HZ = 10000.0
EMBEDDED_SAMPLE_BASE_FREQUENCY_HZ = 302.61
EMBEDDED_SAMPLE_GAIN = 1.65
EMBEDDED_SAMPLE2_BASE_FREQUENCY_HZ = 130.81
EMBEDDED_SAMPLE2_GAIN = 4.00

TWO_PI = math.tau
PI = math.pi
HALF_PI = math.pi / 2.0

WAVEFORM_NAME_MAP = {
    "kSine": "sine",
    "kSquare": "square",
    "kTriangle": "triangle",
    "kWarm": "warm",
    "kHollow": "hollow",
    "kBright": "bright",
    "kSample": "sample",
    "kSample2": "sample2",
    "sine": "sine",
    "square": "square",
    "triangle": "triangle",
    "warm": "warm",
    "hollow": "hollow",
    "bright": "bright",
    "sample": "sample",
    "sample2": "sample2",
}

WAVETABLE_HARMONICS = {
    "warm": (1.00, 0.42, 0.20, 0.10, 0.05),
    "hollow": (1.00, 0.00, 0.60, 0.00, 0.32, 0.00, 0.18),
    "bright": (1.00, 0.72, 0.55, 0.42, 0.32, 0.24, 0.18, 0.12),
}

_WAVETABLE_CACHE: dict[str, list[float]] = {}
_EMBEDDED_SAMPLE_CACHE: dict[str, array[int]] = {}

EMBEDDED_SAMPLE_SPECS = {
    "sample": {
        "path": Path("assets/audio_samples/dream_tides_full_mono10k.pcm"),
        "base_frequency_hz": EMBEDDED_SAMPLE_BASE_FREQUENCY_HZ,
        "gain": EMBEDDED_SAMPLE_GAIN,
    },
    "sample2": {
        "path": Path("assets/audio_samples/voice_ah_mono10k.pcm"),
        "base_frequency_hz": EMBEDDED_SAMPLE2_BASE_FREQUENCY_HZ,
        "gain": EMBEDDED_SAMPLE2_GAIN,
    },
}


@dataclass(frozen=True)
class PreviewRenderResult:
    output_path: Path
    frame_count: int
    duration_seconds: float


def normalize_waveform_name(waveform: str) -> str:
    normalized = WAVEFORM_NAME_MAP.get(waveform)
    if normalized is None:
        raise ValueError(f"unsupported waveform for preview render: {waveform}")
    return normalized


def midi_note_to_frequency(midi_note: float) -> float:
    return 440.0 * math.pow(2.0, (midi_note - 69.0) / 12.0)


def smoothing_alpha_for_elapsed(base_alpha: float, elapsed_ms: float) -> float:
    if base_alpha <= 0.0:
        return 0.0
    if base_alpha >= 1.0:
        return 1.0
    frame_ratio = elapsed_ms / SCORE_SMOOTHING_REFERENCE_MS
    return 1.0 - math.pow(1.0 - base_alpha, frame_ratio)


def build_wavetable(harmonics: tuple[float, ...]) -> list[float]:
    table = [0.0] * WAVETABLE_SIZE
    peak = 0.0
    for sample_index in range(WAVETABLE_SIZE):
        phase = (TWO_PI * sample_index) / WAVETABLE_SIZE
        value = 0.0
        for harmonic_index, amplitude in enumerate(harmonics):
            if amplitude == 0.0:
                continue
            value += amplitude * math.sin(phase * float(harmonic_index + 1))
        table[sample_index] = value
        peak = max(peak, abs(value))

    if peak <= 0.0:
        return table

    return [value / peak for value in table]


def wavetable_for(waveform: str) -> list[float]:
    if waveform not in WAVETABLE_HARMONICS:
        raise ValueError(f"waveform {waveform} does not use a wavetable")
    if waveform not in _WAVETABLE_CACHE:
        _WAVETABLE_CACHE[waveform] = build_wavetable(WAVETABLE_HARMONICS[waveform])
    return _WAVETABLE_CACHE[waveform]


def read_pcm16le(path: Path) -> array[int]:
    pcm = array("h")
    raw = path.read_bytes()
    pcm.frombytes(raw)
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm


def embedded_sample_data(project_dir: Path, waveform: str) -> array[int]:
    if waveform not in EMBEDDED_SAMPLE_SPECS:
        raise ValueError(f"waveform {waveform} does not use an embedded sample")
    if waveform not in _EMBEDDED_SAMPLE_CACHE:
        pcm_path = project_dir / EMBEDDED_SAMPLE_SPECS[waveform]["path"]
        _EMBEDDED_SAMPLE_CACHE[waveform] = read_pcm16le(pcm_path)
    return _EMBEDDED_SAMPLE_CACHE[waveform]


def sample_wavetable(waveform: str, phase: float) -> float:
    table = wavetable_for(waveform)
    normalized_phase = phase / TWO_PI
    position = normalized_phase * WAVETABLE_SIZE
    index = int(position) % WAVETABLE_SIZE
    next_index = (index + 1) % WAVETABLE_SIZE
    fraction = position - index
    current = table[index]
    nxt = table[next_index]
    return current + (nxt - current) * fraction


def sample_embedded_loop(
    waveform: str,
    sample_data: array[int],
    sample_position: float,
) -> float:
    sample_count = len(sample_data)
    if sample_count == 0:
        return 0.0

    while sample_position >= sample_count:
        sample_position -= sample_count
    while sample_position < 0.0:
        sample_position += sample_count

    index = int(sample_position) % sample_count
    next_index = (index + 1) % sample_count
    fraction = sample_position - index
    current = sample_data[index] / 32768.0
    nxt = sample_data[next_index] / 32768.0
    gain = EMBEDDED_SAMPLE_SPECS[waveform]["gain"]
    return (current + (nxt - current) * fraction) * gain


def sample_position_step(waveform: str, frequency_hz: float) -> float:
    base_frequency_hz = EMBEDDED_SAMPLE_SPECS[waveform]["base_frequency_hz"]
    return (
        (EMBEDDED_SAMPLE_RATE_HZ / AUDIO_SAMPLE_RATE)
        * (frequency_hz / base_frequency_hz)
    )


def render_waveform_sample(
    waveform: str,
    phase: float,
    sample_position: float,
    sample_data: array[int] | None,
) -> float:
    if waveform == "sine":
        return math.sin(phase)
    if waveform == "square":
        return 1.0 if phase < PI else -1.0
    if waveform == "triangle":
        if phase < HALF_PI:
            return phase / HALF_PI
        if phase < PI + HALF_PI:
            return 2.0 - (phase / HALF_PI)
        return (phase / HALF_PI) - 4.0
    if waveform in WAVETABLE_HARMONICS:
        return sample_wavetable(waveform, phase)
    if waveform in EMBEDDED_SAMPLE_SPECS:
        if sample_data is None:
            raise ValueError("sample waveform requires embedded sample data")
        return sample_embedded_loop(waveform, sample_data, sample_position)
    raise ValueError(f"unsupported waveform render: {waveform}")


def clamp_pcm(value: float) -> int:
    pcm = int(round(value * 32767.0))
    if pcm > 32767:
        return 32767
    if pcm < -32768:
        return -32768
    return pcm


def apply_event_targets(
    midi_note: int,
    volume_scale: float,
    previous_note: float,
) -> tuple[float, float, float]:
    if midi_note < 0:
        return previous_note, previous_note, 0.0
    target_note = float(midi_note)
    return target_note, target_note, volume_scale


def render_score_preview(
    project_dir: Path,
    output_path: Path,
    events: list[tuple[int, int]],
    waveform: str,
    *,
    transpose_semitones: int = 0,
    volume_scale: float = 1.0,
    base_volume: float = MAX_OUTPUT_VOLUME,
    loops: int = 1,
) -> PreviewRenderResult:
    if loops < 1:
        raise ValueError("preview loops must be >= 1")
    if not events:
        raise ValueError("score preview requires at least one event")

    normalized_waveform = normalize_waveform_name(waveform)
    sample_data = (
        embedded_sample_data(project_dir, normalized_waveform)
        if normalized_waveform in EMBEDDED_SAMPLE_SPECS
        else None
    )
    rendered_events = [
        (
            midi_note if midi_note < 0 else midi_note + transpose_semitones,
            duration_ms,
        )
        for _ in range(loops)
        for midi_note, duration_ms in events
    ]

    ms_per_sample = 1000.0 / AUDIO_SAMPLE_RATE
    glide_alpha = smoothing_alpha_for_elapsed(SCORE_PITCH_GLIDE_ALPHA, ms_per_sample)
    attack_alpha = smoothing_alpha_for_elapsed(SCORE_VOLUME_ATTACK_ALPHA, ms_per_sample)
    release_alpha = smoothing_alpha_for_elapsed(SCORE_VOLUME_RELEASE_ALPHA, ms_per_sample)

    current_note = 57.0
    last_note = 57.0
    current_volume_scale = 0.0
    phase = 0.0
    sample_position = 0.0
    frames = array("h")

    initial_note, _, current_target_volume = apply_event_targets(
        rendered_events[0][0],
        volume_scale,
        last_note,
    )
    current_note = initial_note
    last_note = initial_note

    def append_frames(sample_count: int, target_note: float, target_volume_scale: float) -> None:
        nonlocal current_note
        nonlocal current_volume_scale
        nonlocal phase
        nonlocal sample_position

        for _ in range(sample_count):
            current_note += (target_note - current_note) * glide_alpha
            frequency_hz = midi_note_to_frequency(current_note)

            volume_alpha = (
                attack_alpha
                if target_volume_scale >= current_volume_scale
                else release_alpha
            )
            current_volume_scale += (
                target_volume_scale - current_volume_scale
            ) * volume_alpha
            if current_volume_scale < 0.0005:
                current_volume_scale = 0.0

            waveform_sample = render_waveform_sample(
                normalized_waveform,
                phase,
                sample_position,
                sample_data,
            )
            pcm = clamp_pcm(waveform_sample * base_volume * current_volume_scale)
            frames.append(pcm)
            frames.append(pcm)

            phase += TWO_PI * frequency_hz / AUDIO_SAMPLE_RATE
            if phase >= TWO_PI:
                phase %= TWO_PI

            if normalized_waveform in EMBEDDED_SAMPLE_SPECS and sample_data is not None:
                sample_position += sample_position_step(normalized_waveform, frequency_hz)
                sample_wrap = len(sample_data)
                while sample_position >= sample_wrap:
                    sample_position -= sample_wrap

    for midi_note, duration_ms in rendered_events:
        target_note, last_note, current_target_volume = apply_event_targets(
            midi_note,
            volume_scale,
            last_note,
        )
        sample_count = max(1, round(duration_ms * AUDIO_SAMPLE_RATE / 1000.0))
        append_frames(sample_count, target_note, current_target_volume)

    release_samples = max(1, round(PREVIEW_RELEASE_TAIL_MS * AUDIO_SAMPLE_RATE / 1000.0))
    append_frames(release_samples, last_note, 0.0)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output_path), "wb") as wav_file:
        wav_file.setnchannels(2)
        wav_file.setsampwidth(2)
        wav_file.setframerate(AUDIO_SAMPLE_RATE)
        wav_file.writeframes(frames.tobytes())

    frame_count = len(frames) // 2
    return PreviewRenderResult(
        output_path=output_path,
        frame_count=frame_count,
        duration_seconds=frame_count / AUDIO_SAMPLE_RATE,
    )
