#pragma once

#include "control/score_types.h"

namespace generated_scores {

constexpr ScoreEvent kTwinkleEvents[] = {
    {60, 500},
    {60, 500},
    {67, 500},
    {67, 500},
    {69, 500},
    {69, 500},
    {67, 1000},
    {65, 500},
    {65, 500},
    {64, 500},
    {64, 500},
    {62, 500},
    {62, 500},
    {60, 1000},
    {67, 500},
    {67, 500},
    {65, 500},
    {65, 500},
    {64, 500},
    {64, 500},
    {62, 1000},
    {67, 500},
    {67, 500},
    {65, 500},
    {65, 500},
    {64, 500},
    {64, 500},
    {62, 1000},
    {60, 500},
    {60, 500},
    {67, 500},
    {67, 500},
    {69, 500},
    {69, 500},
    {67, 1000},
    {65, 500},
    {65, 500},
    {64, 500},
    {64, 500},
    {62, 500},
    {62, 500},
    {60, 1000},
};

constexpr ScoreDefinition kTwinkleScore = {
    "twinkle",
    "小星星",
    Waveform::kWarm,
    kTwinkleEvents,
    sizeof(kTwinkleEvents) / sizeof(kTwinkleEvents[0]),
    true,
    1.00f,
};

}  // namespace generated_scores
