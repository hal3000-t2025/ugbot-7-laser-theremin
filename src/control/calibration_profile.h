#pragma once

#include <stdint.h>

#include "app_config.h"

struct CalibrationSettings {
  uint16_t pitch_near_mm = app_config::kPitchNearDistanceMm;
  uint16_t pitch_far_mm = app_config::kPitchFarDistanceMm;
  uint16_t volume_near_mm = app_config::kVolumeNearDistanceMm;
  uint16_t volume_far_mm = app_config::kVolumeFarDistanceMm;
  float pitch_smoothing_alpha = app_config::kDefaultPitchSmoothingAlpha;
  float pitch_curve_gamma = app_config::kDefaultPitchCurveGamma;
  float volume_smoothing_alpha = app_config::kDefaultVolumeSmoothingAlpha;
  float volume_silence_gate = app_config::kVolumeSilenceGate;
  float max_output_volume = app_config::kMaximumVolume;

  static CalibrationSettings defaults() {
    return CalibrationSettings{};
  }

  void clamp() {
    pitch_near_mm = clampDistance(pitch_near_mm);
    pitch_far_mm = clampDistance(pitch_far_mm);
    volume_near_mm = clampDistance(volume_near_mm);
    volume_far_mm = clampDistance(volume_far_mm);

    ensureOrder(pitch_near_mm, pitch_far_mm);
    ensureOrder(volume_near_mm, volume_far_mm);

    pitch_smoothing_alpha = clampAlpha(pitch_smoothing_alpha);
    pitch_curve_gamma = clampPitchCurveGamma(pitch_curve_gamma);
    volume_smoothing_alpha = clampAlpha(volume_smoothing_alpha);
    volume_silence_gate = clampGate(volume_silence_gate);
    max_output_volume = clampMaxVolume(max_output_volume);
  }

 private:
  static uint16_t clampDistance(uint16_t value) {
    if (value < 20) {
      return 20;
    }
    if (value > 4000) {
      return 4000;
    }
    return value;
  }

  static void ensureOrder(uint16_t& near_mm, uint16_t& far_mm) {
    if (near_mm >= far_mm) {
      far_mm = near_mm + 10;
      if (far_mm > 4000) {
        far_mm = 4000;
        if (near_mm >= far_mm) {
          near_mm = far_mm - 10;
        }
      }
    }
  }

  static float clampAlpha(float value) {
    if (value < 0.01f) {
      return 0.01f;
    }
    if (value > 1.0f) {
      return 1.0f;
    }
    return value;
  }

  static float clampPitchCurveGamma(float value) {
    if (value < app_config::kPitchCurveGammaMin) {
      return app_config::kPitchCurveGammaMin;
    }
    if (value > app_config::kPitchCurveGammaMax) {
      return app_config::kPitchCurveGammaMax;
    }
    return value;
  }

  static float clampGate(float value) {
    if (value < 0.0f) {
      return 0.0f;
    }
    if (value > 0.80f) {
      return 0.80f;
    }
    return value;
  }

  static float clampMaxVolume(float value) {
    if (value < 0.05f) {
      return 0.05f;
    }
    if (value > 0.25f) {
      return 0.25f;
    }
    return value;
  }
};
