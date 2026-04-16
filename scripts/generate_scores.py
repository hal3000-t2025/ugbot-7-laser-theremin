Import("env")

import subprocess
import sys
from pathlib import Path


def generate_score(project_dir: Path, source: str, output: str, score_name: str, waveform: str, loop: bool) -> None:
    tool = project_dir / "tools" / "abc_to_score.py"
    source_path = project_dir / source
    output_path = project_dir / output

    command = [
        sys.executable,
        str(tool),
        str(source_path),
        str(output_path),
        "--score-name",
        score_name,
        "--waveform",
        waveform,
    ]
    if loop:
        command.append("--loop")

    subprocess.run(command, check=True, cwd=project_dir)


project_dir = Path(env.subst("$PROJECT_DIR"))

generate_score(
    project_dir=project_dir,
    source="scores/twinkle_twinkle.abc",
    output="src/control/generated_scores.h",
    score_name="Twinkle",
    waveform="kWarm",
    loop=True,
)
