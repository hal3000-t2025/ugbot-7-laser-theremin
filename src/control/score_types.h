#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio/audio_engine.h"

struct ScoreEvent {
  int16_t midi_note;
  uint16_t duration_ms;
};

struct ScoreDefinition {
  const char* name;
  const char* display_name;
  Waveform waveform;
  const ScoreEvent* events;
  size_t event_count;
  bool loop;
  float volume_scale;
};
