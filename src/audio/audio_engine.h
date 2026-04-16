#pragma once

#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>

enum class Waveform {
  kSine,
  kSquare,
  kTriangle,
  kWarm,
  kHollow,
  kBright,
  kSample,
};

class AudioEngine {
 public:
  bool begin();
  void update();

  void setFrequency(float frequency_hz);
  void setVolume(float volume);
  void setWaveform(Waveform waveform);
  Waveform waveform() const;
  static const char* waveformName(Waveform waveform);

 private:
 static void initializeWavetables();
  void fillBuffer();
  static float renderWaveformSample(Waveform waveform, float phase, float sample_position);
  static float sampleWavetable(Waveform waveform, float phase);
  static float sampleEmbeddedLoop(float sample_position);
  static float samplePositionStep(float frequency_hz);

  float frequency_hz_ = 440.0f;
  float volume_ = 0.18f;
  float phase_ = 0.0f;
  float sample_position_ = 0.0f;
  float previous_sample_position_ = 0.0f;
  Waveform waveform_ = Waveform::kSine;
  Waveform target_waveform_ = Waveform::kSine;
  Waveform previous_waveform_ = Waveform::kSine;
  float waveform_blend_ = 1.0f;
  mutable portMUX_TYPE parameter_lock_ = portMUX_INITIALIZER_UNLOCKED;
};
