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
  preferences.putFloat(kKeyVolumeSmooth, sanitized.volume_smoothing_alpha);
  preferences.putFloat(kKeyVolumeGate, sanitized.volume_silence_gate);
  preferences.putFloat(kKeyMaxVolume, sanitized.max_output_volume);
  preferences.end();
  return true;
}
