#pragma once

#include <stdint.h>

class HandMapper {
 public:
  float pitchNoteFromDistanceMm(
      uint16_t distance_mm,
      uint16_t near_mm,
      uint16_t far_mm,
      float curve_gamma,
      float min_note,
      float max_note) const;
  float pitchFromDistanceMm(
      uint16_t distance_mm,
      uint16_t near_mm,
      uint16_t far_mm,
      float curve_gamma,
      float min_frequency_hz,
      float max_frequency_hz) const;
  float volumeFromDistanceMm(
      uint16_t distance_mm,
      uint16_t near_mm,
      uint16_t far_mm,
      float silence_gate,
      float min_volume,
      float max_volume) const;
  float playbackRateFromDistanceMm(
      uint16_t distance_mm,
      uint16_t near_mm,
      uint16_t far_mm,
      float min_rate,
      float max_rate) const;
};
