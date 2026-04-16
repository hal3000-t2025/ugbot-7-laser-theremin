#include "control/score_player.h"

#include <Arduino.h>
#include <cmath>

void ScorePlayer::play(const ScoreDefinition* score) {
  score_ = score;
  current_event_index_ = 0;
  event_progress_ms_ = 0.0f;
  current_is_rest_ = true;
  current_frequency_hz_ = last_note_frequency_hz_;

  if (score_ == nullptr || score_->events == nullptr || score_->event_count == 0) {
    stop();
    return;
  }

  const unsigned long now_ms = millis();
  last_update_ms_ = now_ms;
  applyCurrentEvent(now_ms);
}

void ScorePlayer::stop() {
  score_ = nullptr;
  current_event_index_ = 0;
  event_progress_ms_ = 0.0f;
  current_is_rest_ = true;
  current_frequency_hz_ = last_note_frequency_hz_;
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
  if (score_ == nullptr) {
    return last_note_frequency_hz_;
  }

  return current_is_rest_ ? last_note_frequency_hz_ : current_frequency_hz_;
}

float ScorePlayer::currentVolumeLevel(float base_volume) const {
  if (score_ == nullptr || current_is_rest_) {
    return 0.0f;
  }

  return base_volume * score_->volume_scale;
}

void ScorePlayer::applyCurrentEvent(unsigned long now_ms) {
  if (score_ == nullptr || score_->events == nullptr || current_event_index_ >= score_->event_count) {
    return;
  }

  const ScoreEvent& event = score_->events[current_event_index_];
  last_update_ms_ = now_ms;
  current_is_rest_ = event.midi_note < 0;

  if (current_is_rest_) {
    current_frequency_hz_ = last_note_frequency_hz_;
    return;
  }

  current_frequency_hz_ = midiNoteToFrequency(event.midi_note);
  last_note_frequency_hz_ = current_frequency_hz_;
}

float ScorePlayer::midiNoteToFrequency(int16_t midi_note) {
  return 440.0f * powf(2.0f, static_cast<float>(midi_note - 69) / 12.0f);
}
