#include "control/calibration_store.h"

#include <Preferences.h>

namespace {

constexpr const char* kNamespace = "theremin";
constexpr const char* kKeyPitchNear = "p_near";
constexpr const char* kKeyPitchFar = "p_far";
constexpr const char* kKeyVolumeNear = "v_near";
constexpr const char* kKeyVolumeFar = "v_far";
constexpr const char* kKeyPitchSmooth = "p_smooth";
constexpr const char* kKeyPitchCurve = "p_curve";
constexpr const char* kKeyPitchSnapWidth = "p_snap_w";
constexpr const char* kKeyPitchSnapStrength = "p_snap_s";
constexpr const char* kKeyPitchSnapSmooth = "p_snap_sm";
constexpr const char* kKeyPitchSnapScale = "p_snap_sc";
constexpr const char* kKeyPitchSnapRoot = "p_snap_rt";
constexpr const char* kKeyVolumeSmooth = "v_smooth";
constexpr const char* kKeyVolumeGate = "v_gate";
constexpr const char* kKeyMaxVolume = "max_vol";

}  // namespace

bool CalibrationStore::load(CalibrationSettings& settings) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  if (!preferences.isKey(kKeyPitchNear)) {
    preferences.end();
    return false;
  }

  settings.pitch_near_mm = preferences.getUShort(kKeyPitchNear, settings.pitch_near_mm);
  settings.pitch_far_mm = preferences.getUShort(kKeyPitchFar, settings.pitch_far_mm);
  settings.volume_near_mm = preferences.getUShort(kKeyVolumeNear, settings.volume_near_mm);
  settings.volume_far_mm = preferences.getUShort(kKeyVolumeFar, settings.volume_far_mm);
  settings.pitch_smoothing_alpha =
      preferences.getFloat(kKeyPitchSmooth, settings.pitch_smoothing_alpha);
  settings.pitch_curve_gamma = preferences.getFloat(kKeyPitchCurve, settings.pitch_curve_gamma);
  settings.pitch_snap_width_semitones =
      preferences.getFloat(kKeyPitchSnapWidth, settings.pitch_snap_width_semitones);
  settings.pitch_snap_max_strength =
      preferences.getFloat(kKeyPitchSnapStrength, settings.pitch_snap_max_strength);
  settings.pitch_snap_smoothing_alpha =
      preferences.getFloat(kKeyPitchSnapSmooth, settings.pitch_snap_smoothing_alpha);
  settings.pitch_snap_scale = static_cast<PitchScaleType>(
      preferences.getUChar(kKeyPitchSnapScale, static_cast<uint8_t>(settings.pitch_snap_scale)));
  settings.pitch_snap_root = preferences.getUChar(kKeyPitchSnapRoot, settings.pitch_snap_root);
  settings.volume_smoothing_alpha =
      preferences.getFloat(kKeyVolumeSmooth, settings.volume_smoothing_alpha);
  settings.volume_silence_gate =
      preferences.getFloat(kKeyVolumeGate, settings.volume_silence_gate);
  settings.max_output_volume = preferences.getFloat(kKeyMaxVolume, settings.max_output_volume);
  preferences.end();

  settings.clamp();
  return true;
}

bool CalibrationStore::save(const CalibrationSettings& settings) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }

  CalibrationSettings sanitized = settings;
  sanitized.clamp();

  preferences.putUShort(kKeyPitchNear, sanitized.pitch_near_mm);
  preferences.putUShort(kKeyPitchFar, sanitized.pitch_far_mm);
  preferences.putUShort(kKeyVolumeNear, sanitized.volume_near_mm);
  preferences.putUShort(kKeyVolumeFar, sanitized.volume_far_mm);
  preferences.putFloat(kKeyPitchSmooth, sanitized.pitch_smoothing_alpha);
  preferences.putFloat(kKeyPitchCurve, sanitized.pitch_curve_gamma);
  preferences.putFloat(kKeyPitchSnapWidth, sanitized.pitch_snap_width_semitones);
  preferences.putFloat(kKeyPitchSnapStrength, sanitized.pitch_snap_max_strength);
  preferences.putFloat(kKeyPitchSnapSmooth, sanitized.pitch_snap_smoothing_alpha);
  preferences.putUChar(kKeyPitchSnapScale, static_cast<uint8_t>(sanitized.pitch_snap_scale));
  preferences.putUChar(kKeyPitchSnapRoot, sanitized.pitch_snap_root);
  preferences.putFloat(kKeyVolumeSmooth, sanitized.volume_smoothing_alpha);
  preferences.putFloat(kKeyVolumeGate, sanitized.volume_silence_gate);
  preferences.putFloat(kKeyMaxVolume, sanitized.max_output_volume);
  preferences.end();
  return true;
}
