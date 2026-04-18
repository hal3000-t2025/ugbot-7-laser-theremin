#include "control/score_player.h"

#include <Arduino.h>
#include <cmath>

void ScorePlayer::play(const ScoreDefinition* score) {
  score_ = score;
  current_event_index_ = 0;
  event_progress_ms_ = 0.0f;
  current_is_rest_ = true;
  current_volume_scale_ = 0.0f;
  target_volume_scale_ = 0.0f;

  if (score_ == nullptr || score_->events == nullptr || score_->event_count == 0) {
    stop();
    return;
  }

  const unsigned long now_ms = millis();
  last_update_ms_ = now_ms;
  applyCurrentEvent(now_ms);
  current_note_ = target_note_;
  current_frequency_hz_ = midiNoteToFrequency(current_note_);
}

void ScorePlayer::stop() {
  score_ = nullptr;
  current_event_index_ = 0;
  event_progress_ms_ = 0.0f;
  current_is_rest_ = true;
  target_volume_scale_ = 0.0f;
  current_volume_scale_ = 0.0f;
}

void ScorePlayer::setPlaybackRate(float playback_rate) {
  if (playback_rate < app_config::kScorePlaybackRateMin) {
    playback_rate_ = app_config::kScorePlaybackRateMin;
    return;
  }

  if (playback_rate > app_config::kScorePlaybackRateMax) {
    playback_rate_ = app_config::kScorePlaybackRateMax;
    return;
  }

  playback_rate_ = playback_rate;
}

void ScorePlayer::update(unsigned long now_ms) {
  if (score_ == nullptr || score_->events == nullptr || score_->event_count == 0) {
    return;
  }

  if (last_update_ms_ == 0) {
    last_update_ms_ = now_ms;
  }

  const unsigned long elapsed_ms = now_ms - last_update_ms_;
  last_update_ms_ = now_ms;
  updateContinuousState(static_cast<float>(elapsed_ms));
  event_progress_ms_ += static_cast<float>(elapsed_ms) * playback_rate_;

  while (event_progress_ms_ >= static_cast<float>(score_->events[current_event_index_].duration_ms)) {
    event_progress_ms_ -= static_cast<float>(score_->events[current_event_index_].duration_ms);
    ++current_event_index_;
    if (current_event_index_ >= score_->event_count) {
      if (!score_->loop) {
        stop();
        return;
      }
      current_event_index_ = 0;
    }

    applyCurrentEvent(now_ms);
  }
}

float ScorePlayer::currentFrequencyHz() const {
  return current_frequency_hz_;
}

float ScorePlayer::currentVolumeLevel(float base_volume) const {
  if (score_ == nullptr) {
    return 0.0f;
  }

  return base_volume * current_volume_scale_;
}

void ScorePlayer::applyCurrentEvent(unsigned long now_ms) {
  if (score_ == nullptr || score_->events == nullptr || current_event_index_ >= score_->event_count) {
    return;
  }

  const ScoreEvent& event = score_->events[current_event_index_];
  last_update_ms_ = now_ms;
  current_is_rest_ = event.midi_note < 0;

  if (current_is_rest_) {
    target_note_ = last_note_;
    target_volume_scale_ = 0.0f;
    return;
  }

  target_note_ = static_cast<float>(event.midi_note);
  last_note_ = target_note_;
  target_volume_scale_ = score_->volume_scale;
}

void ScorePlayer::updateContinuousState(float elapsed_ms) {
  if (elapsed_ms <= 0.0f) {
    return;
  }

  const float glide_alpha =
      smoothingAlphaForElapsed(app_config::kScorePitchGlideAlpha, elapsed_ms);
  const float volume_attack_alpha =
      smoothingAlphaForElapsed(app_config::kScoreVolumeAttackAlpha, elapsed_ms);
  const float volume_release_alpha =
      smoothingAlphaForElapsed(app_config::kScoreVolumeReleaseAlpha, elapsed_ms);

  current_note_ += (target_note_ - current_note_) * glide_alpha;
  current_frequency_hz_ = midiNoteToFrequency(current_note_);

  const float volume_alpha =
      target_volume_scale_ >= current_volume_scale_ ? volume_attack_alpha : volume_release_alpha;
  current_volume_scale_ += (target_volume_scale_ - current_volume_scale_) * volume_alpha;
  if (current_volume_scale_ < 0.0005f) {
    current_volume_scale_ = 0.0f;
  }
}

float ScorePlayer::midiNoteToFrequency(float midi_note) {
  return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

float ScorePlayer::smoothingAlphaForElapsed(float base_alpha, float elapsed_ms) {
  if (base_alpha <= 0.0f) {
    return 0.0f;
  }

  if (base_alpha >= 1.0f) {
    return 1.0f;
  }

  const float frame_ratio = elapsed_ms / app_config::kScoreSmoothingReferenceMs;
  return 1.0f - powf(1.0f - base_alpha, frame_ratio);
}
