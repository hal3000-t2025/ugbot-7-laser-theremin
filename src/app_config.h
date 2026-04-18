#pragma once

#include <stdint.h>

namespace app_config {

constexpr unsigned long kSerialBaudRate = 115200;
constexpr int kAudioSampleRate = 32000;
constexpr float kTestToneFrequencyHz = 440.0f;
constexpr float kDiagnosticToneVolume = 0.05f;
constexpr float kInitialVolume = 0.18f;
constexpr float kMinimumAudibleVolume = 0.0f;
constexpr float kMaximumVolume = 0.14f;
constexpr int kWavetableSize = 128;
constexpr int kAudioFramesPerChunk = 256;
constexpr int kI2cFrequencyHz = 400000;
constexpr unsigned long kSensorPollIntervalMs = 10;
constexpr unsigned long kSerialSensorReportIntervalMs = 120;
constexpr unsigned long kCalibrationCaptureTimeoutMs = 2500;
constexpr uint8_t kCalibrationCaptureSamples = 8;
constexpr unsigned long kTofMeasurementTimingBudgetUs = 20000;
constexpr unsigned long kTofContinuousPeriodMs = 20;
constexpr uint8_t kPitchSensorI2cAddress = 0x2A;
constexpr uint8_t kVolumeSensorI2cAddress = 0x2B;
constexpr float kPitchMinFrequencyHz = 220.0f;
constexpr float kPitchMaxFrequencyHz = 1760.0f;
constexpr uint16_t kPitchNearDistanceMm = 60;
constexpr uint16_t kPitchFarDistanceMm = 500;
constexpr uint16_t kVolumeNearDistanceMm = 50;
constexpr uint16_t kVolumeFarDistanceMm = 500;
constexpr float kDefaultPitchSmoothingAlpha = 0.24f;
constexpr float kDefaultPitchSnapSmoothingAlpha = 0.18f;
constexpr float kDefaultVolumeSmoothingAlpha = 0.20f;
constexpr float kDefaultPitchCurveGamma = 0.75f;
constexpr float kPitchCurveGammaMin = 0.30f;
constexpr float kPitchCurveGammaMax = 2.50f;
constexpr float kDefaultPitchSnapWidthSemitones = 0.35f;
constexpr float kDefaultPitchSnapMaxStrength = 0.85f;
constexpr float kPitchSnapWidthMin = 0.05f;
constexpr float kPitchSnapWidthMax = 1.50f;
constexpr uint8_t kDefaultPitchSnapRoot = 0;
constexpr float kDistanceEmaAlpha = kDefaultPitchSmoothingAlpha;
constexpr float kVolumeSilenceGate = 0.10f;
constexpr float kVolumeResponseExponent = 0.5f;
constexpr float kScorePlaybackRateMin = 1.0f;
constexpr float kScorePlaybackRateMax = 3.0f;
// Score playback keeps phase continuous and glides between target notes.
constexpr float kScorePitchGlideAlpha = 0.18f;
constexpr float kScoreVolumeAttackAlpha = 0.28f;
constexpr float kScoreVolumeReleaseAlpha = 0.36f;
constexpr float kScoreSmoothingReferenceMs = 10.0f;
constexpr float kWaveformBlendStep = 0.02f;
constexpr uint8_t kOledI2cAddress = 0x3C;
constexpr unsigned long kOledRefreshIntervalMs = 250;
constexpr uint8_t kOledWidth = 128;
constexpr uint8_t kOledHeight = 32;

}  // namespace app_config
