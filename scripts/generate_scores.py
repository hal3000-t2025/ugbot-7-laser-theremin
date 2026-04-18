Import("env")

import sys
from dataclasses import dataclass
from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(project_dir / "tools"))

from abc_to_score import parse_abc  # type: ignore


@dataclass
class ScoreSpec:
    source: str
    score_name: str
    score_id: str
    waveform: str
    loop: bool
    display_name: str = ""


def render_score_block(project_dir: Path, spec: ScoreSpec) -> str:
    parsed = parse_abc(project_dir / spec.source)
    display_name = spec.display_name or parsed.title
    event_lines = "\n".join(
        f"    {{{midi_note}, {duration_ms}}},"
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
    1.00f,
}};
"""


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
    ),
]

output_path = project_dir / "src" / "control" / "generated_scores.h"
output_path.write_text(render_header(project_dir, score_specs), encoding="utf-8")
print(f"generated {output_path} with {len(score_specs)} scores")
