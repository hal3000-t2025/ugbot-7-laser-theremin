#include "control/pitch_snapper.h"

#include <Arduino.h>
#include <cmath>

namespace {

constexpr uint8_t kChromaticIntervals[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kMajorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint8_t kMinorIntervals[] = {0, 2, 3, 5, 7, 8, 10};
constexpr uint8_t kPentatonicMajorIntervals[] = {0, 2, 4, 7, 9};
constexpr uint8_t kPentatonicMinorIntervals[] = {0, 3, 5, 7, 10};

template <size_t kCount>
float nearestNoteInIntervals(float raw_note, uint8_t root_pitch_class, const uint8_t (&intervals)[kCount]) {
  const int center_octave = static_cast<int>(floorf(raw_note / 12.0f));
  float nearest_note = raw_note;
  float nearest_distance = INFINITY;

  for (int octave = center_octave - 3; octave <= center_octave + 3; ++octave) {
    const float octave_base = static_cast<float>(octave * 12 + root_pitch_class);
    for (const uint8_t interval : intervals) {
      const float candidate = octave_base + static_cast<float>(interval);
      const float distance = fabsf(candidate - raw_note);
      if (distance < nearest_distance) {
        nearest_distance = distance;
        nearest_note = candidate;
      }
    }
  }

  return nearest_note;
}

}  // namespace

PitchSnapResult PitchSnapper::snap(float raw_note, const PitchSnapConfig& config) const {
  PitchSnapResult result = {};
  result.raw_note = raw_note;
  result.snapped_note = nearestScaleNote(raw_note, config);

  if (config.snap_width_semitones <= 0.0f || config.max_snap_strength <= 0.0f) {
    result.mixed_note = raw_note;
    result.strength = 0.0f;
    return result;
  }

  const float distance = fabsf(result.snapped_note - raw_note);
  const float normalized = constrain(1.0f - (distance / config.snap_width_semitones), 0.0f, 1.0f);
  const float shaped = normalized * normalized * (3.0f - 2.0f * normalized);
  result.strength = config.max_snap_strength * shaped;
  result.mixed_note = raw_note + (result.snapped_note - raw_note) * result.strength;
  return result;
}

float PitchSnapper::frequencyToMidiNote(float frequency_hz) {
  if (frequency_hz <= 0.0f) {
    return 0.0f;
  }

  return 69.0f + 12.0f * log2f(frequency_hz / 440.0f);
}

float PitchSnapper::midiNoteToFrequency(float midi_note) {
  return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

const char* PitchSnapper::scaleTypeName(PitchScaleType scale_type) {
  switch (scale_type) {
    case PitchScaleType::kChromatic:
      return "chromatic";
    case PitchScaleType::kMajor:
      return "major";
    case PitchScaleType::kMinor:
      return "minor";
    case PitchScaleType::kPentatonicMajor:
      return "pmajor";
    case PitchScaleType::kPentatonicMinor:
      return "pminor";
  }

  return "unknown";
}

const char* PitchSnapper::pitchClassName(uint8_t pitch_class) {
  static constexpr const char* kNames[] = {
      "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return kNames[pitch_class % 12];
}

float PitchSnapper::nearestScaleNote(float raw_note, const PitchSnapConfig& config) const {
  const uint8_t root_pitch_class = config.root_pitch_class % 12;
  switch (config.scale_type) {
    case PitchScaleType::kChromatic:
      return nearestNoteInIntervals(raw_note, root_pitch_class, kChromaticIntervals);
    case PitchScaleType::kMajor:
      return nearestNoteInIntervals(raw_note, root_pitch_class, kMajorIntervals);
    case PitchScaleType::kMinor:
      return nearestNoteInIntervals(raw_note, root_pitch_class, kMinorIntervals);
    case PitchScaleType::kPentatonicMajor:
      return nearestNoteInIntervals(raw_note, root_pitch_class, kPentatonicMajorIntervals);
    case PitchScaleType::kPentatonicMinor:
      return nearestNoteInIntervals(raw_note, root_pitch_class, kPentatonicMinorIntervals);
  }

  return raw_note;
}
