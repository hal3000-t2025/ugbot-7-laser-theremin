#pragma once

#include <stddef.h>

#include "app_config.h"
#include "control/score_types.h"

class ScorePlayer {
 public:
  void play(const ScoreDefinition* score);
  void stop();
  void setPlaybackRate(float playback_rate);
  void update(unsigned long now_ms);

 bool isActive() const { return score_ != nullptr; }
  bool isRest() const { return current_is_rest_; }
  float currentFrequencyHz() const;
  float currentVolumeLevel(float base_volume) const;
  float playbackRate() const { return playback_rate_; }
  const ScoreDefinition* score() const { return score_; }
  size_t currentEventIndex() const { return current_event_index_; }

 private:
  void applyCurrentEvent(unsigned long now_ms);
  void updateContinuousState(float elapsed_ms);
  static float midiNoteToFrequency(float midi_note);
  static float smoothingAlphaForElapsed(float base_alpha, float elapsed_ms);

  const ScoreDefinition* score_ = nullptr;
  size_t current_event_index_ = 0;
  unsigned long last_update_ms_ = 0;
  float event_progress_ms_ = 0.0f;
  bool current_is_rest_ = true;
  float current_note_ = 57.0f;
  float target_note_ = 57.0f;
  float last_note_ = 57.0f;
  float current_frequency_hz_ = app_config::kPitchMinFrequencyHz;
  float current_volume_scale_ = 0.0f;
  float target_volume_scale_ = 0.0f;
  float playback_rate_ = app_config::kScorePlaybackRateMin;
};
