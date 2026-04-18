#include "control/hand_mapper.h"

#include <Arduino.h>
#include <cmath>

#include "app_config.h"
#include "control/pitch_snapper.h"

float HandMapper::pitchNoteFromDistanceMm(
    uint16_t distance_mm,
    uint16_t near_mm,
    uint16_t far_mm,
    float curve_gamma,
    float min_note,
    float max_note) const {
  const float clamped = constrain(
      static_cast<float>(distance_mm),
      static_cast<float>(near_mm),
      static_cast<float>(far_mm));

  const float normalized =
      (clamped - static_cast<float>(near_mm)) / static_cast<float>(far_mm - near_mm);
  const float shaped_position = powf(normalized, curve_gamma);
  return min_note + (max_note - min_note) * shaped_position;
}

float HandMapper::pitchFromDistanceMm(
    uint16_t distance_mm,
    uint16_t near_mm,
    uint16_t far_mm,
    float curve_gamma,
    float min_frequency_hz,
    float max_frequency_hz) const {
  const float min_note = PitchSnapper::frequencyToMidiNote(min_frequency_hz);
  const float max_note = PitchSnapper::frequencyToMidiNote(max_frequency_hz);
  const float note = pitchNoteFromDistanceMm(
      distance_mm,
      near_mm,
      far_mm,
      curve_gamma,
      min_note,
      max_note);
  return PitchSnapper::midiNoteToFrequency(note);
}

float HandMapper::volumeFromDistanceMm(
    uint16_t distance_mm,
    uint16_t near_mm,
    uint16_t far_mm,
    float silence_gate,
    float min_volume,
    float max_volume) const {
  const float clamped = constrain(
      static_cast<float>(distance_mm),
      static_cast<float>(near_mm),
      static_cast<float>(far_mm));

  const float normalized =
      (clamped - static_cast<float>(near_mm)) / static_cast<float>(far_mm - near_mm);

  const float hand_presence = 1.0f - normalized;
  if (hand_presence <= silence_gate) {
    return 0.0f;
  }

  const float gated_presence = (hand_presence - silence_gate) / (1.0f - silence_gate);
  const float shaped_presence = powf(gated_presence, app_config::kVolumeResponseExponent);
  return min_volume + shaped_presence * (max_volume - min_volume);
}

float HandMapper::playbackRateFromDistanceMm(
    uint16_t distance_mm,
    uint16_t near_mm,
    uint16_t far_mm,
    float min_rate,
    float max_rate) const {
  const float clamped = constrain(
      static_cast<float>(distance_mm),
      static_cast<float>(near_mm),
      static_cast<float>(far_mm));

  const float normalized =
      (clamped - static_cast<float>(near_mm)) / static_cast<float>(far_mm - near_mm);
  return min_rate + normalized * (max_rate - min_rate);
}
