#!/usr/bin/env python3
"""Minimal ABC to firmware score converter.

Supports the subset currently needed by this project:
- single voice melody
- T/L/M/K/Q headers
- notes A-G / a-g
- rests z / x
- durations like 2, /, /2, 3/2
- accidentals ^ _ =
- octave markers ' and ,
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
import re
from typing import Iterable


NOTE_TO_SEMITONE = {
    "C": 0,
    "D": 2,
    "E": 4,
    "F": 5,
    "G": 7,
    "A": 9,
    "B": 11,
}


@dataclass
class ParsedScore:
    title: str
    events: list[tuple[int, int]]


def parse_fraction(text: str) -> Fraction:
    num, den = text.split("/", 1)
    return Fraction(int(num), int(den))


def parse_qpm(text: str) -> int:
    value = text.strip()
    if "=" in value:
        _, bpm = value.split("=", 1)
        return int(bpm.strip())
    return int(value)


def tokenize(body: str) -> Iterable[str]:
    i = 0
    while i < len(body):
      ch = body[i]
      if ch.isspace() or ch in "|:[]":
          i += 1
          continue

      if ch in "^_=" or ch.isalpha():
          start = i
          while i < len(body) and body[i] in "^_=":
              i += 1
          if i >= len(body):
              raise ValueError("dangling accidental in ABC body")
          if not body[i].isalpha():
              raise ValueError(f"unexpected token near: {body[start:i+1]}")
          i += 1
          while i < len(body) and body[i] in "',":
              i += 1
          while i < len(body) and (body[i].isdigit() or body[i] == "/"):
              i += 1
          yield body[start:i]
          continue

      raise ValueError(f"unsupported ABC token: {ch}")


def parse_length(token: str) -> Fraction:
    if not token:
        return Fraction(1, 1)
    if token.isdigit():
        return Fraction(int(token), 1)
    if token.startswith("/"):
        if token == "/":
            return Fraction(1, 2)
        return Fraction(1, int(token[1:]))
    if "/" in token:
        num, den = token.split("/", 1)
        return Fraction(int(num), int(den))
    raise ValueError(f"unsupported note length: {token}")


def parse_note(token: str, default_length: Fraction, quarter_ms: float) -> tuple[int, int]:
    match = re.fullmatch(r"(?P<acc>[\^_=]*)(?P<note>[A-Ga-gzxZX])(?P<oct>[',]*)(?P<len>[0-9/]*)", token)
    if not match:
        raise ValueError(f"unsupported note token: {token}")

    note = match.group("note")
    length = parse_length(match.group("len"))
    duration_ms = int(float(Fraction(4, 1) * default_length * length) * quarter_ms)

    if note in {"z", "Z", "x", "X"}:
        return -1, duration_ms

    accidental = 0
    for symbol in match.group("acc"):
        if symbol == "^":
            accidental += 1
        elif symbol == "_":
            accidental -= 1
        elif symbol == "=":
            accidental = 0

    midi_note = 60 + NOTE_TO_SEMITONE[note.upper()]
    if note.islower():
        midi_note += 12

    for symbol in match.group("oct"):
        if symbol == "'":
            midi_note += 12
        elif symbol == ",":
            midi_note -= 12

    midi_note += accidental
    return midi_note, duration_ms


def parse_abc(path: Path) -> ParsedScore:
    headers: dict[str, list[str]] = {}
    body_lines: list[str] = []

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split("%", 1)[0].strip()
        if not line:
            continue
        if len(line) > 2 and line[1] == ":":
            headers.setdefault(line[0], []).append(line[2:].strip())
        else:
            body_lines.append(line)

    default_length = parse_fraction(headers.get("L", ["1/8"])[0])
    bpm = parse_qpm(headers.get("Q", ["120"])[0])
    quarter_ms = 60000.0 / bpm
    title = headers.get("T", [path.stem])[0]

    events = [
        parse_note(token, default_length, quarter_ms)
        for token in tokenize(" ".join(body_lines))
    ]
    return ParsedScore(title=title, events=events)


def render_header(score_name: str, display_name: str, events: list[tuple[int, int]], waveform: str, loop: bool) -> str:
    event_lines = "\n".join(
        f"    {{{midi_note}, {duration_ms}}},"
        for midi_note, duration_ms in events
    )
    return f"""#pragma once

#include "control/score_types.h"

namespace generated_scores {{

constexpr ScoreEvent k{score_name}Events[] = {{
{event_lines}
}};

constexpr ScoreDefinition k{score_name}Score = {{
    "{score_name.lower()}",
    "{display_name}",
    Waveform::{waveform},
    k{score_name}Events,
    sizeof(k{score_name}Events) / sizeof(k{score_name}Events[0]),
    {"true" if loop else "false"},
    1.00f,
}};

}}  // namespace generated_scores
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="把最小子集 ABC 转成固件 score header")
    parser.add_argument("input", help="ABC 文件路径")
    parser.add_argument("output", help="输出 header 路径")
    parser.add_argument("--score-name", default="Example", help="生成的 score 名称，默认 Example")
    parser.add_argument("--display-name", default="", help="显示名，默认使用 ABC 第一条标题")
    parser.add_argument("--waveform", default="kSample", help="绑定的波形枚举，默认 kSample")
    parser.add_argument("--loop", action="store_true", help="生成循环播放 score")
    args = parser.parse_args()

    parsed = parse_abc(Path(args.input))
    display_name = args.display_name or parsed.title
    header = render_header(args.score_name, display_name, parsed.events, args.waveform, args.loop)
    Path(args.output).write_text(header, encoding="utf-8")
    print(f"generated {args.output} with {len(parsed.events)} events")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
