#pragma once

#include <stdint.h>

enum class PitchScaleType : uint8_t {
  kChromatic = 0,
  kMajor = 1,
  kMinor = 2,
  kPentatonicMajor = 3,
  kPentatonicMinor = 4,
};

struct PitchSnapConfig {
  float snap_width_semitones = 0.35f;
  float max_snap_strength = 0.85f;
  PitchScaleType scale_type = PitchScaleType::kMajor;
  uint8_t root_pitch_class = 0;
};

struct PitchSnapResult {
  float raw_note = 69.0f;
  float snapped_note = 69.0f;
  float mixed_note = 69.0f;
  float strength = 0.0f;
};

class PitchSnapper {
 public:
  PitchSnapResult snap(float raw_note, const PitchSnapConfig& config) const;

  static float frequencyToMidiNote(float frequency_hz);
  static float midiNoteToFrequency(float midi_note);
  static const char* scaleTypeName(PitchScaleType scale_type);
  static const char* pitchClassName(uint8_t pitch_class);

 private:
  float nearestScaleNote(float raw_note, const PitchSnapConfig& config) const;
};
