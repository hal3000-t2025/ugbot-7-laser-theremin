#include <Arduino.h>
#include <Wire.h>

#include "app_config.h"
#include "audio/audio_engine.h"
#include "board_profile.h"
#include "control/calibration_profile.h"
#include "control/calibration_store.h"
#include "control/generated_scores.h"
#include "control/hand_mapper.h"
#include "control/pitch_snapper.h"
#include "control/score_player.h"
#include "control/smoothing.h"
#include "display/oled_status_display.h"
#include "sensors/tof_sensor.h"

AudioEngine audio_engine;
TofSensor pitch_sensor(board_profile::kPitchSensorXshut);
TofSensor volume_sensor(board_profile::kVolumeSensorXshut);
HandMapper hand_mapper;
PitchSnapper pitch_snapper;
ExponentialSmoother pitch_distance_smoother(app_config::kDistanceEmaAlpha);
ExponentialSmoother pitch_note_smoother(app_config::kDefaultPitchSnapSmoothingAlpha);
ExponentialSmoother volume_distance_smoother(app_config::kDistanceEmaAlpha);
String serial_command;
CalibrationSettings calibration_settings;
CalibrationStore calibration_store;
ScorePlayer score_player;
OledStatusDisplay oled_status_display;
bool audio_muted = false;
bool serial_stream_enabled = true;
bool diagnostic_tone_enabled = false;
unsigned long last_status_ms = 0;
unsigned long last_sensor_poll_ms = 0;
TaskHandle_t audio_task_handle = nullptr;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
TwoWire volume_wire(1);
#endif

namespace {

enum class CalibrationTarget {
  kNone,
  kPitchNear,
  kPitchFar,
  kVolumeNear,
  kVolumeFar,
};

struct CalibrationCaptureState {
  CalibrationTarget target = CalibrationTarget::kNone;
  uint32_t accumulated_mm = 0;
  uint16_t min_mm = UINT16_MAX;
  uint16_t max_mm = 0;
  uint8_t sample_count = 0;
  unsigned long started_ms = 0;
  bool active = false;
};

CalibrationCaptureState calibration_capture;

constexpr uint32_t kAudioTaskStackSize = 4096;
constexpr UBaseType_t kAudioTaskPriority = 2;
constexpr unsigned long kPresetButtonDebounceMs = 30;
constexpr float kVolumeReleaseAlpha = 0.65f;

struct PitchSnapTelemetry {
  float raw_note = 0.0f;
  float snapped_note = 0.0f;
  float mixed_note = 0.0f;
  float smoothed_note = 0.0f;
  float strength = 0.0f;
  float output_frequency_hz = app_config::kPitchMinFrequencyHz;
  bool valid = false;
};

PitchSnapTelemetry pitch_snap_telemetry;
bool pitch_snap_debug_enabled = false;

struct PresetSlot {
  const char* name;
  Waveform waveform;
  const ScoreDefinition* score;
  const char* serial_message;
};

constexpr PresetSlot kPresetSlots[] = {
    {"sine", Waveform::kSine, nullptr, "Waveform set to sine."},
    {"square", Waveform::kSquare, nullptr, "Waveform set to square."},
    {"triangle", Waveform::kTriangle, nullptr, "Waveform set to triangle."},
    {"warm", Waveform::kWarm, nullptr, "Waveform set to warm wavetable."},
    {"hollow", Waveform::kHollow, nullptr, "Waveform set to hollow wavetable."},
    {"bright", Waveform::kBright, nullptr, "Waveform set to bright wavetable."},
    {"sample", Waveform::kSample, nullptr, "Waveform set to embedded sample loop."},
    {"sample2", Waveform::kSample, nullptr, "Waveform set to embedded sample loop."},
    {"twinkle", Waveform::kWarm, &generated_scores::kTwinkleScore, "Preset set to twinkle score."},
    {"twinkle2", Waveform::kWarm, &generated_scores::kTwinkleScore, "Preset set to twinkle score."},
};

constexpr size_t kPresetSlotCount = sizeof(kPresetSlots) / sizeof(kPresetSlots[0]);

size_t current_preset_index = 0;
bool preset_button_last_read = false;
bool preset_button_stable_state = false;
unsigned long preset_button_last_change_ms = 0;

void audioTask(void*) {
  for (;;) {
    audio_engine.update();
  }
}

bool startAudioTask() {
#if CONFIG_FREERTOS_UNICORE
  return xTaskCreate(
             audioTask,
             "audio_task",
             kAudioTaskStackSize,
             nullptr,
             kAudioTaskPriority,
             &audio_task_handle) == pdPASS;
#else
  return xTaskCreatePinnedToCore(
             audioTask,
             "audio_task",
             kAudioTaskStackSize,
             nullptr,
             kAudioTaskPriority,
             &audio_task_handle,
             0) == pdPASS;
#endif
}

float pitchMinMidiNote() {
  return PitchSnapper::frequencyToMidiNote(app_config::kPitchMinFrequencyHz);
}

float pitchMaxMidiNote() {
  return PitchSnapper::frequencyToMidiNote(app_config::kPitchMaxFrequencyHz);
}

PitchSnapConfig currentPitchSnapConfig() {
  PitchSnapConfig config = {};
  config.snap_width_semitones = calibration_settings.pitch_snap_width_semitones;
  config.max_snap_strength = calibration_settings.pitch_snap_max_strength;
  config.scale_type = calibration_settings.pitch_snap_scale;
  config.root_pitch_class = calibration_settings.pitch_snap_root;
  return config;
}

void clearPitchSnapTelemetry() {
  pitch_snap_telemetry = PitchSnapTelemetry{};
}

float applyPitchSnapFromDistance(float distance_mm) {
  const float raw_note = hand_mapper.pitchNoteFromDistanceMm(
      static_cast<uint16_t>(distance_mm),
      calibration_settings.pitch_near_mm,
      calibration_settings.pitch_far_mm,
      calibration_settings.pitch_curve_gamma,
      pitchMinMidiNote(),
      pitchMaxMidiNote());
  const PitchSnapResult snapped = pitch_snapper.snap(raw_note, currentPitchSnapConfig());
  const float smoothed_note =
      pitch_note_smoother.updateWithAlpha(snapped.mixed_note, calibration_settings.pitch_snap_smoothing_alpha);
  pitch_snap_telemetry.raw_note = snapped.raw_note;
  pitch_snap_telemetry.snapped_note = snapped.snapped_note;
  pitch_snap_telemetry.mixed_note = snapped.mixed_note;
  pitch_snap_telemetry.smoothed_note = smoothed_note;
  pitch_snap_telemetry.strength = snapped.strength;
  pitch_snap_telemetry.output_frequency_hz = PitchSnapper::midiNoteToFrequency(smoothed_note);
  pitch_snap_telemetry.valid = true;
  return pitch_snap_telemetry.output_frequency_hz;
}

void resyncPitchSnapState() {
  clearPitchSnapTelemetry();
  pitch_note_smoother.reset();
  if (pitch_distance_smoother.initialized()) {
    applyPitchSnapFromDistance(pitch_distance_smoother.value());
  }
}

void applyCalibrationSettings() {
  calibration_settings.clamp();
  pitch_distance_smoother.setAlpha(calibration_settings.pitch_smoothing_alpha);
  pitch_note_smoother.setAlpha(calibration_settings.pitch_snap_smoothing_alpha);
  volume_distance_smoother.setAlpha(calibration_settings.volume_smoothing_alpha);
  resyncPitchSnapState();
}

float clampNormalized(float value) {
  return constrain(value, 0.0f, 1.0f);
}

bool initializePitchSensor() {
  if (board_profile::kHasDualI2c) {
    return pitch_sensor.begin(Wire);
  }

  return pitch_sensor.begin(Wire, app_config::kPitchSensorI2cAddress);
}

bool initializeVolumeSensor() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  if (board_profile::kHasDualI2c) {
    return volume_sensor.begin(volume_wire);
  }
#endif

  return volume_sensor.begin(Wire, app_config::kVolumeSensorI2cAddress);
}

const char* calibrationTargetName(CalibrationTarget target) {
  switch (target) {
    case CalibrationTarget::kPitchNear:
      return "pitch near";
    case CalibrationTarget::kPitchFar:
      return "pitch far";
    case CalibrationTarget::kVolumeNear:
      return "volume near";
    case CalibrationTarget::kVolumeFar:
      return "volume far";
    case CalibrationTarget::kNone:
      return "none";
  }

  return "none";
}

size_t presetIndexForWaveform(Waveform waveform) {
  for (size_t i = 0; i < kPresetSlotCount; ++i) {
    if (kPresetSlots[i].waveform == waveform) {
      return i;
    }
  }

  return 0;
}

const PresetSlot& currentPreset() {
  return kPresetSlots[current_preset_index];
}

void applyPresetIndex(size_t index) {
  current_preset_index = index % kPresetSlotCount;
  audio_engine.setWaveform(currentPreset().waveform);
  if (currentPreset().score != nullptr) {
    score_player.play(currentPreset().score);
  } else {
    score_player.stop();
  }
}

void announcePresetSelection(const char* source) {
  Serial.printf(
      "%s preset=%u/%u name=%s waveform=%s\n",
      source,
      static_cast<unsigned>(current_preset_index + 1),
      static_cast<unsigned>(kPresetSlotCount),
      currentPreset().name,
      AudioEngine::waveformName(currentPreset().waveform));
  if (currentPreset().score != nullptr) {
    Serial.printf("score=%s autoplay=on\n", currentPreset().score->name);
  }
}

void setPresetFromWaveform(Waveform waveform, bool print_waveform_message) {
  applyPresetIndex(presetIndexForWaveform(waveform));
  if (print_waveform_message) {
    Serial.println(currentPreset().serial_message);
  }
}

bool applyPresetSelectionCommand(int preset_number) {
  if (preset_number < 1 || preset_number > static_cast<int>(kPresetSlotCount)) {
    return false;
  }

  applyPresetIndex(static_cast<size_t>(preset_number - 1));
  announcePresetSelection("Preset set:");
  return true;
}

void advancePreset(const char* source) {
  applyPresetIndex((current_preset_index + 1) % kPresetSlotCount);
  announcePresetSelection(source);
}

void configurePresetButton() {
  if (!board_profile::kHasPresetButton) {
    return;
  }

  const int button_pin = static_cast<int>(board_profile::kPresetButton);
  pinMode(button_pin, INPUT);
  const bool initial_state = digitalRead(button_pin) == HIGH;
  preset_button_last_read = initial_state;
  preset_button_stable_state = initial_state;
  preset_button_last_change_ms = millis();
  Serial.printf(
      "Preset button ready on GPIO%d (initial=%s)\n",
      button_pin,
      initial_state ? "high" : "low");
}

void pollPresetButton() {
  if (!board_profile::kHasPresetButton) {
    return;
  }

  const unsigned long now = millis();
  const bool raw_state = digitalRead(static_cast<int>(board_profile::kPresetButton)) == HIGH;
  if (raw_state != preset_button_last_read) {
    preset_button_last_read = raw_state;
    preset_button_last_change_ms = now;
    return;
  }

  if (raw_state == preset_button_stable_state) {
    return;
  }

  if (now - preset_button_last_change_ms < kPresetButtonDebounceMs) {
    return;
  }

  preset_button_stable_state = raw_state;
  advancePreset("Preset button:");
}

bool calibrationTargetUsesPitchSensor(CalibrationTarget target) {
  return target == CalibrationTarget::kPitchNear || target == CalibrationTarget::kPitchFar;
}

void resetCalibrationCapture() {
  calibration_capture = CalibrationCaptureState{};
}

float currentPitchHz() {
  if (!pitch_sensor.isOnline()) {
    return app_config::kTestToneFrequencyHz;
  }

  if (pitch_note_smoother.initialized()) {
    return PitchSnapper::midiNoteToFrequency(pitch_note_smoother.value());
  }

  if (!pitch_sensor.hasValidReading() && !pitch_distance_smoother.initialized()) {
    return app_config::kPitchMinFrequencyHz;
  }

  const float smoothed_distance = pitch_distance_smoother.initialized()
                                      ? pitch_distance_smoother.value()
                                      : static_cast<float>(pitch_sensor.lastDistanceMm());
  return applyPitchSnapFromDistance(smoothed_distance);
}

float currentVolumeLevel() {
  if (!volume_sensor.isOnline()) {
    return app_config::kInitialVolume;
  }

  if (!volume_sensor.hasValidReading()) {
    return 0.0f;
  }

  const float smoothed_distance = volume_distance_smoother.initialized()
                                      ? volume_distance_smoother.value()
                                      : static_cast<float>(volume_sensor.lastDistanceMm());
  return hand_mapper.volumeFromDistanceMm(
      static_cast<uint16_t>(smoothed_distance),
      calibration_settings.volume_near_mm,
      calibration_settings.volume_far_mm,
      calibration_settings.volume_silence_gate,
      app_config::kMinimumAudibleVolume,
      calibration_settings.max_output_volume);
}

float currentScorePlaybackRate() {
  if (!pitch_sensor.isOnline()) {
    return app_config::kScorePlaybackRateMin;
  }

  if (!pitch_sensor.hasValidReading() && !pitch_distance_smoother.initialized()) {
    return app_config::kScorePlaybackRateMin;
  }

  const float smoothed_distance = pitch_distance_smoother.initialized()
                                      ? pitch_distance_smoother.value()
                                      : static_cast<float>(pitch_sensor.lastDistanceMm());
  return hand_mapper.playbackRateFromDistanceMm(
      static_cast<uint16_t>(smoothed_distance),
      calibration_settings.pitch_near_mm,
      calibration_settings.pitch_far_mm,
      app_config::kScorePlaybackRateMin,
      app_config::kScorePlaybackRateMax);
}

float currentScoreBaseVolumeLevel() {
  if (!volume_sensor.isOnline()) {
    return calibration_settings.max_output_volume;
  }

  return currentVolumeLevel();
}

float currentOutputPitchHz();
float currentOutputVolumeLevel();

float currentPitchDisplayLevel() {
  const float current_pitch_hz = currentOutputPitchHz();
  const float min_pitch_hz = app_config::kPitchMinFrequencyHz;
  const float max_pitch_hz = app_config::kPitchMaxFrequencyHz;
  if (current_pitch_hz <= min_pitch_hz) {
    return 0.0f;
  }

  if (current_pitch_hz >= max_pitch_hz) {
    return 1.0f;
  }

  const float log_min = logf(min_pitch_hz);
  const float log_max = logf(max_pitch_hz);
  const float log_value = logf(current_pitch_hz);
  return clampNormalized((log_value - log_min) / (log_max - log_min));
}

float currentScoreRateDisplayLevel() {
  const float min_rate = app_config::kScorePlaybackRateMin;
  const float max_rate = app_config::kScorePlaybackRateMax;
  const float current_rate = score_player.playbackRate();
  if (max_rate <= min_rate) {
    return 0.0f;
  }

  return clampNormalized((current_rate - min_rate) / (max_rate - min_rate));
}

float currentVolumeDisplayLevel() {
  const float max_volume = calibration_settings.max_output_volume;
  if (max_volume <= 0.0f) {
    return 0.0f;
  }

  return clampNormalized(currentOutputVolumeLevel() / max_volume);
}

void updateOledStatusDisplay(unsigned long now_ms) {
  if (!oled_status_display.ready()) {
    return;
  }

  OledStatusSnapshot snapshot = {};
  snapshot.preset_index = static_cast<uint8_t>(current_preset_index + 1);
  snapshot.preset_count = static_cast<uint8_t>(kPresetSlotCount);
  snapshot.preset_name = currentPreset().name;
  snapshot.primary_label = score_player.isActive() ? "SPD" : "PIT";
  snapshot.secondary_label = "VOL";
  snapshot.primary_level =
      score_player.isActive() ? currentScoreRateDisplayLevel() : currentPitchDisplayLevel();
  snapshot.secondary_level = currentVolumeDisplayLevel();
  oled_status_display.update(snapshot, now_ms);
}

float currentOutputPitchHz() {
  if (diagnostic_tone_enabled) {
    return app_config::kTestToneFrequencyHz;
  }

  if (score_player.isActive()) {
    return score_player.currentFrequencyHz();
  }

  return currentPitchHz();
}

float currentOutputVolumeLevel() {
  if (audio_muted) {
    return 0.0f;
  }

  if (diagnostic_tone_enabled) {
    return app_config::kDiagnosticToneVolume;
  }

  if (score_player.isActive()) {
    return score_player.currentVolumeLevel(currentScoreBaseVolumeLevel());
  }

  return currentVolumeLevel();
}

void applyAudioOutputState() {
  if (audio_muted) {
    audio_engine.setVolume(0.0f);
    return;
  }

  if (diagnostic_tone_enabled) {
    audio_engine.setFrequency(app_config::kTestToneFrequencyHz);
    audio_engine.setVolume(app_config::kDiagnosticToneVolume);
    return;
  }

  if (score_player.isActive()) {
    audio_engine.setFrequency(score_player.currentFrequencyHz());
    audio_engine.setVolume(score_player.currentVolumeLevel(currentScoreBaseVolumeLevel()));
    return;
  }

  audio_engine.setFrequency(currentPitchHz());
  audio_engine.setVolume(currentVolumeLevel());
}

void printSerialCommandHelp() {
  Serial.println("Commands:");
  Serial.println("  sine | square | triangle | warm | hollow | bright | sample | sample2 | twinkle | twinkle2 | 1..10");
  Serial.println("  next | preset <1-10>");
  Serial.println("  mute | unmute | status | stream on | stream off | help");
  Serial.println("  testtone on | testtone off");
  Serial.println("  cal pitch near | cal pitch far");
  Serial.println("  cal volume near | cal volume far");
  Serial.println("  set pitch near <mm> | set pitch far <mm>");
  Serial.println("  set volume near <mm> | set volume far <mm>");
  Serial.println("  set smooth pitch <0.01-1.0> | set filter pitch <0.01-1.0>");
  Serial.println("  set smooth volume <0.01-1.0> | set snap smooth <0.01-1.0>");
  Serial.println("  set curve pitch <0.30-2.50>");
  Serial.println("  set snap width <0.05-1.50> | set snap strength <0.00-1.00>");
  Serial.println("  set snap scale <chromatic|major|minor|pmajor|pminor>");
  Serial.println("  set snap root <C..B | 0..11> | snap debug on|off");
  Serial.println("  set gate volume <0.00-0.80>");
  Serial.println("  set max volume <0.05-0.25>");
  Serial.println("  save | load | defaults");
}

void printCalibrationSettings() {
  Serial.printf(
      "cal pitch near=%u far=%u filter=%.2f curve=%.2f | volume near=%u far=%u smooth=%.2f gate=%.2f max=%.2f\n",
      calibration_settings.pitch_near_mm,
      calibration_settings.pitch_far_mm,
      calibration_settings.pitch_smoothing_alpha,
      calibration_settings.pitch_curve_gamma,
      calibration_settings.volume_near_mm,
      calibration_settings.volume_far_mm,
      calibration_settings.volume_smoothing_alpha,
      calibration_settings.volume_silence_gate,
      calibration_settings.max_output_volume);
  Serial.printf(
      "snap scale=%s root=%s width=%.2f strength=%.2f smooth=%.2f\n",
      PitchSnapper::scaleTypeName(calibration_settings.pitch_snap_scale),
      PitchSnapper::pitchClassName(calibration_settings.pitch_snap_root),
      calibration_settings.pitch_snap_width_semitones,
      calibration_settings.pitch_snap_max_strength,
      calibration_settings.pitch_snap_smoothing_alpha);
}

void printAudioStatus() {
  Serial.printf(
      "waveform=%s preset=%u/%u name=%s muted=%s testtone=%s pitch=%.1fHz volume=%.2f\n",
      AudioEngine::waveformName(audio_engine.waveform()),
      static_cast<unsigned>(current_preset_index + 1),
      static_cast<unsigned>(kPresetSlotCount),
      currentPreset().name,
      audio_muted ? "yes" : "no",
      diagnostic_tone_enabled ? "yes" : "no",
      currentOutputPitchHz(),
      currentOutputVolumeLevel());
  printCalibrationSettings();
  if (calibration_capture.active) {
    Serial.printf(
        "capturing=%s progress=%u/%u\n",
        calibrationTargetName(calibration_capture.target),
        calibration_capture.sample_count,
        app_config::kCalibrationCaptureSamples);
  }
  if (score_player.isActive() && score_player.score() != nullptr) {
    Serial.printf(
        "score=%s event=%u/%u rest=%s rate=%.2fx\n",
        score_player.score()->name,
        static_cast<unsigned>(score_player.currentEventIndex() + 1),
        static_cast<unsigned>(score_player.score()->event_count),
        score_player.isRest() ? "yes" : "no",
        score_player.playbackRate());
  }
  if (pitch_snap_debug_enabled && pitch_snap_telemetry.valid) {
    Serial.printf(
        "snap raw=%.2f snapped=%.2f mixed=%.2f smooth=%.2f strength=%.2f out=%.1fHz\n",
        pitch_snap_telemetry.raw_note,
        pitch_snap_telemetry.snapped_note,
        pitch_snap_telemetry.mixed_note,
        pitch_snap_telemetry.smoothed_note,
        pitch_snap_telemetry.strength,
        pitch_snap_telemetry.output_frequency_hz);
  }
}

void initializeOledStatusDisplay() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  TwoWire& display_wire = board_profile::kHasDualI2c ? volume_wire : Wire;
#else
  TwoWire& display_wire = Wire;
#endif

  if (oled_status_display.begin(display_wire, app_config::kOledI2cAddress)) {
    Serial.printf(
        "OLED ready on shared I2C address 0x%02X.\n",
        app_config::kOledI2cAddress);
    return;
  }

  Serial.printf(
      "OLED not detected on shared I2C address 0x%02X. Continuing without display.\n",
      app_config::kOledI2cAddress);
}

void applyCapturedCalibration(CalibrationTarget target, uint16_t distance_mm) {
  switch (target) {
    case CalibrationTarget::kPitchNear:
      calibration_settings.pitch_near_mm = distance_mm;
      break;
    case CalibrationTarget::kPitchFar:
      calibration_settings.pitch_far_mm = distance_mm;
      break;
    case CalibrationTarget::kVolumeNear:
      calibration_settings.volume_near_mm = distance_mm;
      break;
    case CalibrationTarget::kVolumeFar:
      calibration_settings.volume_far_mm = distance_mm;
      break;
    case CalibrationTarget::kNone:
      return;
  }

  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
}

bool beginCalibrationCapture(CalibrationTarget target) {
  if (target == CalibrationTarget::kNone) {
    return false;
  }

  TofSensor& sensor = calibrationTargetUsesPitchSensor(target) ? pitch_sensor : volume_sensor;
  if (!sensor.isOnline()) {
    Serial.printf("%s sensor offline.\n", calibrationTargetUsesPitchSensor(target) ? "Pitch" : "Volume");
    return false;
  }

  resetCalibrationCapture();
  calibration_capture.target = target;
  calibration_capture.active = true;
  calibration_capture.started_ms = millis();
  Serial.printf(
      "Sampling %s: hold your hand steady for %u frames.\n",
      calibrationTargetName(target),
      app_config::kCalibrationCaptureSamples);
  return true;
}

void updateCalibrationCapture(bool pitch_updated, bool volume_updated) {
  if (!calibration_capture.active) {
    return;
  }

  const bool use_pitch_sensor = calibrationTargetUsesPitchSensor(calibration_capture.target);
  const bool sensor_updated = use_pitch_sensor ? pitch_updated : volume_updated;

  if (millis() - calibration_capture.started_ms > app_config::kCalibrationCaptureTimeoutMs) {
    Serial.printf("Sampling %s timed out.\n", calibrationTargetName(calibration_capture.target));
    resetCalibrationCapture();
    return;
  }

  if (!sensor_updated) {
    return;
  }

  const uint16_t distance_mm =
      use_pitch_sensor ? pitch_sensor.lastDistanceMm() : volume_sensor.lastDistanceMm();
  calibration_capture.accumulated_mm += distance_mm;
  calibration_capture.min_mm = min(calibration_capture.min_mm, distance_mm);
  calibration_capture.max_mm = max(calibration_capture.max_mm, distance_mm);
  ++calibration_capture.sample_count;

  if (calibration_capture.sample_count < app_config::kCalibrationCaptureSamples) {
    return;
  }

  const uint16_t average_mm = static_cast<uint16_t>(
      calibration_capture.accumulated_mm / calibration_capture.sample_count);
  Serial.printf(
      "Captured %s = %u mm (min=%u max=%u, samples=%u).\n",
      calibrationTargetName(calibration_capture.target),
      average_mm,
      calibration_capture.min_mm,
      calibration_capture.max_mm,
      calibration_capture.sample_count);
  applyCapturedCalibration(calibration_capture.target, average_mm);
  resetCalibrationCapture();
}

bool applySetDistanceCommand(const String& sensor_name, const String& bound_name, int value) {
  if (value <= 0) {
    return false;
  }

  resetCalibrationCapture();

  if (sensor_name == "pitch") {
    if (bound_name == "near") {
      calibration_settings.pitch_near_mm = static_cast<uint16_t>(value);
    } else {
      calibration_settings.pitch_far_mm = static_cast<uint16_t>(value);
    }
  } else if (sensor_name == "volume") {
    if (bound_name == "near") {
      calibration_settings.volume_near_mm = static_cast<uint16_t>(value);
    } else {
      calibration_settings.volume_far_mm = static_cast<uint16_t>(value);
    }
  } else {
    return false;
  }

  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetSmoothingCommand(const String& sensor_name, float value) {
  resetCalibrationCapture();

  if (sensor_name == "pitch") {
    calibration_settings.pitch_smoothing_alpha = value;
  } else if (sensor_name == "volume") {
    calibration_settings.volume_smoothing_alpha = value;
  } else {
    return false;
  }

  applyCalibrationSettings();
  printCalibrationSettings();
  return true;
}

bool applySetPitchCurveCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.pitch_curve_gamma = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetVolumeGateCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.volume_silence_gate = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetMaxVolumeCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.max_output_volume = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool parsePitchScaleType(const String& value, PitchScaleType& scale_type) {
  if (value == "chromatic") {
    scale_type = PitchScaleType::kChromatic;
    return true;
  }
  if (value == "major") {
    scale_type = PitchScaleType::kMajor;
    return true;
  }
  if (value == "minor") {
    scale_type = PitchScaleType::kMinor;
    return true;
  }
  if (value == "pmajor" || value == "pentatonicmajor" || value == "pentatonic-major") {
    scale_type = PitchScaleType::kPentatonicMajor;
    return true;
  }
  if (value == "pminor" || value == "pentatonicminor" || value == "pentatonic-minor") {
    scale_type = PitchScaleType::kPentatonicMinor;
    return true;
  }

  return false;
}

bool parsePitchRoot(const String& value, uint8_t& root_pitch_class) {
  bool numeric = !value.isEmpty();
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isDigit(value[i])) {
      numeric = false;
      break;
    }
  }

  if (numeric) {
    const int numeric_root = value.toInt();
    if (numeric_root >= 0 && numeric_root <= 11) {
      root_pitch_class = static_cast<uint8_t>(numeric_root);
      return true;
    }
  }

  if (value == "c") {
    root_pitch_class = 0;
    return true;
  }
  if (value == "c#" || value == "db") {
    root_pitch_class = 1;
    return true;
  }
  if (value == "d") {
    root_pitch_class = 2;
    return true;
  }
  if (value == "d#" || value == "eb") {
    root_pitch_class = 3;
    return true;
  }
  if (value == "e") {
    root_pitch_class = 4;
    return true;
  }
  if (value == "f") {
    root_pitch_class = 5;
    return true;
  }
  if (value == "f#" || value == "gb") {
    root_pitch_class = 6;
    return true;
  }
  if (value == "g") {
    root_pitch_class = 7;
    return true;
  }
  if (value == "g#" || value == "ab") {
    root_pitch_class = 8;
    return true;
  }
  if (value == "a") {
    root_pitch_class = 9;
    return true;
  }
  if (value == "a#" || value == "bb") {
    root_pitch_class = 10;
    return true;
  }
  if (value == "b") {
    root_pitch_class = 11;
    return true;
  }

  return false;
}

bool applySetPitchSnapWidthCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.pitch_snap_width_semitones = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetPitchSnapStrengthCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.pitch_snap_max_strength = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetPitchSnapSmoothingCommand(float value) {
  resetCalibrationCapture();
  calibration_settings.pitch_snap_smoothing_alpha = value;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetPitchSnapScaleCommand(const String& value) {
  PitchScaleType scale_type = PitchScaleType::kMajor;
  if (!parsePitchScaleType(value, scale_type)) {
    return false;
  }

  resetCalibrationCapture();
  calibration_settings.pitch_snap_scale = scale_type;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

bool applySetPitchSnapRootCommand(const String& value) {
  uint8_t root_pitch_class = 0;
  if (!parsePitchRoot(value, root_pitch_class)) {
    return false;
  }

  resetCalibrationCapture();
  calibration_settings.pitch_snap_root = root_pitch_class;
  applyCalibrationSettings();
  applyAudioOutputState();
  printCalibrationSettings();
  return true;
}

void handleSerialCommand(const String& input) {
  String command = input;
  command.trim();
  command.toLowerCase();

  if (command.isEmpty()) {
    return;
  }

  if (command == "sine" || command == "1") {
    setPresetFromWaveform(Waveform::kSine, true);
    return;
  }

  if (command == "square" || command == "2") {
    setPresetFromWaveform(Waveform::kSquare, true);
    return;
  }

  if (command == "triangle" || command == "3") {
    setPresetFromWaveform(Waveform::kTriangle, true);
    return;
  }

  if (command == "warm" || command == "4") {
    setPresetFromWaveform(Waveform::kWarm, true);
    return;
  }

  if (command == "hollow" || command == "5") {
    setPresetFromWaveform(Waveform::kHollow, true);
    return;
  }

  if (command == "bright" || command == "6") {
    setPresetFromWaveform(Waveform::kBright, true);
    return;
  }

  if (command == "sample" || command == "7") {
    applyPresetSelectionCommand(7);
    return;
  }

  if (command == "sample2" || command == "8") {
    applyPresetSelectionCommand(8);
    return;
  }

  if (command == "twinkle" || command == "9") {
    applyPresetSelectionCommand(9);
    return;
  }

  if (command == "twinkle2" || command == "10") {
    applyPresetSelectionCommand(10);
    return;
  }

  if (command == "next") {
    advancePreset("Serial next:");
    return;
  }

  if (command == "status") {
    printAudioStatus();
    return;
  }

  if (command == "testtone on") {
    diagnostic_tone_enabled = true;
    applyAudioOutputState();
    Serial.println("Diagnostic test tone enabled.");
    return;
  }

  if (command == "testtone off") {
    diagnostic_tone_enabled = false;
    applyAudioOutputState();
    Serial.println("Diagnostic test tone disabled.");
    return;
  }

  if (command == "stream on") {
    serial_stream_enabled = true;
    Serial.println("Status stream enabled.");
    return;
  }

  if (command == "stream off") {
    serial_stream_enabled = false;
    Serial.println("Status stream disabled.");
    return;
  }

  if (command == "snap debug on") {
    pitch_snap_debug_enabled = true;
    Serial.println("Pitch snap debug enabled.");
    return;
  }

  if (command == "snap debug off") {
    pitch_snap_debug_enabled = false;
    Serial.println("Pitch snap debug disabled.");
    return;
  }

  if (command == "save") {
    if (calibration_store.save(calibration_settings)) {
      Serial.println("Calibration saved.");
    } else {
      Serial.println("Calibration save failed.");
    }
    return;
  }

  if (command == "load") {
    if (calibration_store.load(calibration_settings)) {
      resetCalibrationCapture();
      applyCalibrationSettings();
      applyAudioOutputState();
      Serial.println("Calibration loaded.");
      printCalibrationSettings();
    } else {
      Serial.println("No saved calibration found.");
    }
    return;
  }

  if (command == "defaults") {
    resetCalibrationCapture();
    calibration_settings = CalibrationSettings::defaults();
    applyCalibrationSettings();
    applyAudioOutputState();
    Serial.println("Calibration reset to defaults.");
    printCalibrationSettings();
    return;
  }

  if (command == "mute") {
    audio_muted = true;
    applyAudioOutputState();
    Serial.println("Audio muted.");
    return;
  }

  if (command == "unmute") {
    audio_muted = false;
    applyAudioOutputState();
    Serial.println("Audio unmuted.");
    return;
  }

  if (command == "help") {
    printSerialCommandHelp();
    return;
  }

  if (command == "cal pitch near") {
    beginCalibrationCapture(CalibrationTarget::kPitchNear);
    return;
  }

  if (command == "cal pitch far") {
    beginCalibrationCapture(CalibrationTarget::kPitchFar);
    return;
  }

  if (command == "cal volume near") {
    beginCalibrationCapture(CalibrationTarget::kVolumeNear);
    return;
  }

  if (command == "cal volume far") {
    beginCalibrationCapture(CalibrationTarget::kVolumeFar);
    return;
  }

  int distance_value = 0;
  if (sscanf(command.c_str(), "set pitch near %d", &distance_value) == 1) {
    if (!applySetDistanceCommand("pitch", "near", distance_value)) {
      Serial.println("Invalid pitch near value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set pitch far %d", &distance_value) == 1) {
    if (!applySetDistanceCommand("pitch", "far", distance_value)) {
      Serial.println("Invalid pitch far value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set volume near %d", &distance_value) == 1) {
    if (!applySetDistanceCommand("volume", "near", distance_value)) {
      Serial.println("Invalid volume near value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set volume far %d", &distance_value) == 1) {
    if (!applySetDistanceCommand("volume", "far", distance_value)) {
      Serial.println("Invalid volume far value.");
    }
    return;
  }

  float smoothing_value = 0.0f;
  if (sscanf(command.c_str(), "set smooth pitch %f", &smoothing_value) == 1) {
    if (!applySetSmoothingCommand("pitch", smoothing_value)) {
      Serial.println("Invalid pitch smoothing value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set smooth volume %f", &smoothing_value) == 1) {
    if (!applySetSmoothingCommand("volume", smoothing_value)) {
      Serial.println("Invalid volume smoothing value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set filter pitch %f", &smoothing_value) == 1) {
    if (!applySetSmoothingCommand("pitch", smoothing_value)) {
      Serial.println("Invalid pitch filter value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set snap smooth %f", &smoothing_value) == 1) {
    if (!applySetPitchSnapSmoothingCommand(smoothing_value)) {
      Serial.println("Invalid pitch snap smoothing value.");
    }
    return;
  }

  float curve_value = 0.0f;
  if (sscanf(command.c_str(), "set curve pitch %f", &curve_value) == 1) {
    if (!applySetPitchCurveCommand(curve_value)) {
      Serial.println("Invalid pitch curve value.");
    }
    return;
  }

  float gate_value = 0.0f;
  if (sscanf(command.c_str(), "set gate volume %f", &gate_value) == 1) {
    if (!applySetVolumeGateCommand(gate_value)) {
      Serial.println("Invalid volume gate value.");
    }
    return;
  }

  float max_volume_value = 0.0f;
  if (sscanf(command.c_str(), "set max volume %f", &max_volume_value) == 1) {
    if (!applySetMaxVolumeCommand(max_volume_value)) {
      Serial.println("Invalid max volume value.");
    }
    return;
  }

  float snap_value = 0.0f;
  if (sscanf(command.c_str(), "set snap width %f", &snap_value) == 1) {
    if (!applySetPitchSnapWidthCommand(snap_value)) {
      Serial.println("Invalid snap width value.");
    }
    return;
  }

  if (sscanf(command.c_str(), "set snap strength %f", &snap_value) == 1) {
    if (!applySetPitchSnapStrengthCommand(snap_value)) {
      Serial.println("Invalid snap strength value.");
    }
    return;
  }

  if (command.startsWith("set snap scale ")) {
    const String value = command.substring(String("set snap scale ").length());
    if (!applySetPitchSnapScaleCommand(value)) {
      Serial.println("Invalid snap scale value.");
    }
    return;
  }

  if (command.startsWith("set snap root ")) {
    const String value = command.substring(String("set snap root ").length());
    if (!applySetPitchSnapRootCommand(value)) {
      Serial.println("Invalid snap root value.");
    }
    return;
  }

  int preset_number = 0;
  if (sscanf(command.c_str(), "preset %d", &preset_number) == 1) {
    if (!applyPresetSelectionCommand(preset_number)) {
      Serial.println("Invalid preset number.");
    }
    return;
  }

  Serial.printf("Unknown command: %s\n", command.c_str());
  printSerialCommandHelp();
}

void pollSerialCommands() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      handleSerialCommand(serial_command);
      serial_command = "";
      continue;
    }

    serial_command += ch;
    if (serial_command.length() > 96) {
      serial_command = "";
      Serial.println("Command too long, buffer cleared.");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(app_config::kSerialBaudRate);
  delay(300);

  Serial.println();
  Serial.println("Laser Theremin M5 boot");
  Serial.printf("Board: %s\n", board_profile::kBoardName);
  Serial.printf("Test tone: %.1f Hz\n", app_config::kTestToneFrequencyHz);

  Wire.begin(
      static_cast<int>(board_profile::kPitchI2cSda),
      static_cast<int>(board_profile::kPitchI2cScl),
      app_config::kI2cFrequencyHz);
  Serial.printf(
      "Pitch I2C ready on SDA=%d SCL=%d XSHUT=%d\n",
      board_profile::kPitchI2cSda,
      board_profile::kPitchI2cScl,
      board_profile::kPitchSensorXshut);

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  if (board_profile::kHasDualI2c) {
    volume_wire.begin(
        static_cast<int>(board_profile::kVolumeI2cSda),
        static_cast<int>(board_profile::kVolumeI2cScl),
        app_config::kI2cFrequencyHz);
    Serial.printf(
        "Volume I2C ready on SDA=%d SCL=%d XSHUT=%d\n",
        board_profile::kVolumeI2cSda,
        board_profile::kVolumeI2cScl,
        board_profile::kVolumeSensorXshut);
  }
#endif

  audio_engine.setFrequency(app_config::kTestToneFrequencyHz);
  audio_engine.setVolume(0.0f);
  applyCalibrationSettings();

  if (!audio_engine.begin()) {
    Serial.println("I2S init failed, halting.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("I2S init ok, streaming sine wave.");

  if (!startAudioTask()) {
    Serial.println("Audio task start failed, halting.");
    while (true) {
      delay(1000);
    }
  }

  if (!board_profile::kHasDualI2c) {
    pitch_sensor.powerOff();
    volume_sensor.powerOff();
  }

  if (initializePitchSensor()) {
    Serial.printf("Pitch VL53L1X init ok at 0x%02X.\n", pitch_sensor.address());
  } else {
    Serial.println("Pitch VL53L1X init failed.");
  }

  if (initializeVolumeSensor()) {
    Serial.printf("Volume VL53L1X init ok at 0x%02X.\n", volume_sensor.address());
  } else {
    Serial.println("Volume VL53L1X init failed.");
  }

  initializeOledStatusDisplay();

  if (calibration_store.load(calibration_settings)) {
    applyCalibrationSettings();
    Serial.println("Calibration loaded from storage.");
  } else {
    calibration_settings = CalibrationSettings::defaults();
    applyCalibrationSettings();
    Serial.println("Using default calibration.");
  }

  applyPresetIndex(presetIndexForWaveform(audio_engine.waveform()));
  configurePresetButton();
  Serial.printf(
      "Waveform default: %s (preset %u/%u %s)\n",
      AudioEngine::waveformName(audio_engine.waveform()),
      static_cast<unsigned>(current_preset_index + 1),
      static_cast<unsigned>(kPresetSlotCount),
      currentPreset().name);
  updateOledStatusDisplay(millis());
  Serial.println("Audio output default: low volume");
  printCalibrationSettings();
  printSerialCommandHelp();
}

void loop() {
  pollSerialCommands();
  pollPresetButton();

  const unsigned long now = millis();
  if (score_player.isActive()) {
    score_player.setPlaybackRate(currentScorePlaybackRate());
  }
  score_player.update(now);
  updateOledStatusDisplay(now);

  if (diagnostic_tone_enabled) {
    return;
  }

  if (now - last_sensor_poll_ms >= app_config::kSensorPollIntervalMs) {
    last_sensor_poll_ms = now;
    bool pitch_updated = false;
    bool volume_updated = false;

    if (pitch_sensor.isOnline() && pitch_sensor.update()) {
      const uint16_t pitch_distance_mm = pitch_sensor.lastDistanceMm();
      if (pitch_distance_mm <= calibration_settings.pitch_far_mm) {
        const float filtered_pitch_distance =
            pitch_distance_smoother.update(static_cast<float>(pitch_distance_mm));
        applyPitchSnapFromDistance(filtered_pitch_distance);
        if (!diagnostic_tone_enabled && !score_player.isActive()) {
          audio_engine.setFrequency(currentPitchHz());
        }
        pitch_updated = true;
      }
    }

    if (volume_sensor.isOnline()) {
      if (volume_sensor.update()) {
        const uint16_t volume_distance_mm = volume_sensor.lastDistanceMm();
        if (volume_distance_mm >= calibration_settings.volume_far_mm) {
          volume_distance_smoother.reset();
        } else if (volume_distance_mm >= calibration_settings.volume_near_mm) {
          const float volume_distance = static_cast<float>(volume_distance_mm);
          const bool releasing =
              volume_distance_smoother.initialized() &&
              volume_distance > volume_distance_smoother.value();
          if (releasing) {
            volume_distance_smoother.updateWithAlpha(
                volume_distance,
                kVolumeReleaseAlpha);
          } else {
            volume_distance_smoother.update(volume_distance);
          }
          volume_updated = true;
        }
      } else if (!volume_sensor.hasValidReading()) {
        volume_distance_smoother.reset();
      }
    }

    updateCalibrationCapture(pitch_updated, volume_updated);
    applyAudioOutputState();
  }

  if (serial_stream_enabled && now - last_status_ms >= app_config::kSerialSensorReportIntervalMs) {
    last_status_ms = now;
    if (!pitch_sensor.isOnline() && !volume_sensor.isOnline()) {
      Serial.println("Audio running without sensors.");
      return;
    }

    if (pitch_sensor.isOnline() && pitch_sensor.lastReadTimedOut()) {
      Serial.println("Pitch VL53L1X timeout.");
      return;
    }

    if (volume_sensor.isOnline() && volume_sensor.lastReadTimedOut()) {
      Serial.println("Volume VL53L1X timeout.");
      return;
    }

    const float pitch_distance = pitch_distance_smoother.initialized()
                                     ? pitch_distance_smoother.value()
                                     : static_cast<float>(pitch_sensor.lastDistanceMm());
    const float volume_distance = volume_distance_smoother.initialized()
                                      ? volume_distance_smoother.value()
                                      : static_cast<float>(volume_sensor.lastDistanceMm());

    if (score_player.isActive()) {
      Serial.printf(
          "wave=%s preset=%s | muted=%s | pitch_in=%u mm smoothed=%.1f => rate=%.2fx | out=%.1f Hz | volume_in=%u mm smoothed=%.1f => %.2f\n",
          AudioEngine::waveformName(audio_engine.waveform()),
          currentPreset().name,
          audio_muted ? "yes" : "no",
          pitch_sensor.lastDistanceMm(),
          pitch_distance,
          score_player.playbackRate(),
          currentOutputPitchHz(),
          volume_sensor.lastDistanceMm(),
          volume_distance,
          currentOutputVolumeLevel());
      return;
    }

    if (pitch_snap_debug_enabled && pitch_snap_telemetry.valid) {
      Serial.printf(
          "wave=%s preset=%s | muted=%s | pitch_in=%u mm smoothed=%.1f raw=%.2f snap=%.2f mix=%.2f str=%.2f => %.1f Hz | volume_in=%u mm smoothed=%.1f => %.2f\n",
          AudioEngine::waveformName(audio_engine.waveform()),
          currentPreset().name,
          audio_muted ? "yes" : "no",
          pitch_sensor.lastDistanceMm(),
          pitch_distance,
          pitch_snap_telemetry.raw_note,
          pitch_snap_telemetry.snapped_note,
          pitch_snap_telemetry.mixed_note,
          pitch_snap_telemetry.strength,
          currentOutputPitchHz(),
          volume_sensor.lastDistanceMm(),
          volume_distance,
          currentOutputVolumeLevel());
      return;
    }

    Serial.printf(
        "wave=%s preset=%s | muted=%s | pitch_in=%u mm smoothed=%.1f => %.1f Hz | volume_in=%u mm smoothed=%.1f => %.2f\n",
        AudioEngine::waveformName(audio_engine.waveform()),
        currentPreset().name,
        audio_muted ? "yes" : "no",
        pitch_sensor.lastDistanceMm(),
        pitch_distance,
        currentOutputPitchHz(),
        volume_sensor.lastDistanceMm(),
        volume_distance,
        currentOutputVolumeLevel());
  }
}
