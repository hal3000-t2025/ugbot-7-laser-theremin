import sys
from dataclasses import dataclass
from pathlib import Path

try:
    Import("env")  # type: ignore[name-defined]
except NameError:
    env = None


def resolve_project_dir() -> Path:
    if env is not None:
        return Path(env.subst("$PROJECT_DIR"))
    return Path(__file__).resolve().parents[1]


project_dir = resolve_project_dir()
sys.path.insert(0, str(project_dir / "tools"))

from abc_to_score import parse_abc  # type: ignore
from score_preview import render_score_preview  # type: ignore


@dataclass
class ScoreSpec:
    source: str
    score_name: str
    score_id: str
    waveform: str
    loop: bool
    display_name: str = ""
    transpose_semitones: int = 0
    volume_scale: float = 1.0
    preview_loops: int = 1


def render_score_block(project_dir: Path, spec: ScoreSpec) -> str:
    parsed = parse_abc(project_dir / spec.source)
    display_name = spec.display_name or parsed.title
    event_lines = "\n".join(
        f"    {{{midi_note if midi_note < 0 else midi_note + spec.transpose_semitones}, {duration_ms}}},"
        for midi_note, duration_ms in parsed.events
    )
    return f"""constexpr ScoreEvent k{spec.score_name}Events[] = {{
{event_lines}
}};

constexpr ScoreDefinition k{spec.score_name}Score = {{
    "{spec.score_id}",
    "{display_name}",
    Waveform::{spec.waveform},
    k{spec.score_name}Events,
    sizeof(k{spec.score_name}Events) / sizeof(k{spec.score_name}Events[0]),
    {"true" if spec.loop else "false"},
    {spec.volume_scale:.2f}f,
}};
"""


def render_preview_assets(project_dir: Path, specs: list[ScoreSpec]) -> list[Path]:
    preview_dir = project_dir / "generated_audio"
    outputs: list[Path] = []
    for spec in specs:
        parsed = parse_abc(project_dir / spec.source)
        preview_path = preview_dir / f"{spec.score_id}.wav"
        result = render_score_preview(
            project_dir,
            preview_path,
            parsed.events,
            spec.waveform,
            transpose_semitones=spec.transpose_semitones,
            volume_scale=spec.volume_scale,
            loops=spec.preview_loops,
        )
        outputs.append(result.output_path)
        print(
            f"generated preview {result.output_path} "
            f"({result.duration_seconds:.2f}s, {result.frame_count} frames)"
        )
    return outputs


def render_header(project_dir: Path, specs: list[ScoreSpec]) -> str:
    blocks = "\n".join(render_score_block(project_dir, spec) for spec in specs)
    return f"""#pragma once

#include "control/score_types.h"

namespace generated_scores {{

{blocks}
}}  // namespace generated_scores
"""


score_specs = [
    ScoreSpec(
        source="scores/example.abc",
        score_name="Example",
        score_id="example",
        waveform="kWarm",
        loop=True,
    ),
    ScoreSpec(
        source="scores/example2.abc",
        score_name="Example2",
        score_id="example2",
        waveform="kWarm",
        loop=True,
        transpose_semitones=12,
    ),
    ScoreSpec(
        source="scores/example3.abc",
        score_name="Example3",
        score_id="example3",
        waveform="kWarm",
        loop=True,
    ),
]

output_path = project_dir / "src" / "control" / "generated_scores.h"
output_path.write_text(render_header(project_dir, score_specs), encoding="utf-8")
preview_outputs = render_preview_assets(project_dir, score_specs)
print(
    f"generated {output_path} with {len(score_specs)} scores "
    f"and {len(preview_outputs)} preview wav files"
)
