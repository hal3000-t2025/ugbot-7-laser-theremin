#include "audio/audio_engine.h"

#include <Arduino.h>
#include <cmath>

#include "app_config.h"
#include "board_profile.h"

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kHalfPi = 1.57079632679489661923f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEmbeddedSampleRateHz = 10000.0f;
constexpr float kEmbeddedSampleBaseFrequencyHz = 302.61f;
constexpr float kEmbeddedSampleGain = 1.65f;

struct StereoFrame {
  int16_t left;
  int16_t right;
};

struct Wavetable {
  float samples[app_config::kWavetableSize];
};

StereoFrame sample_buffer[app_config::kAudioFramesPerChunk];
Wavetable warm_table = {};
Wavetable hollow_table = {};
Wavetable bright_table = {};
bool wavetables_initialized = false;

constexpr float kWarmHarmonics[] = {1.00f, 0.42f, 0.20f, 0.10f, 0.05f};
constexpr float kHollowHarmonics[] = {1.00f, 0.00f, 0.60f, 0.00f, 0.32f, 0.00f, 0.18f};
constexpr float kBrightHarmonics[] = {1.00f, 0.72f, 0.55f, 0.42f, 0.32f, 0.24f, 0.18f, 0.12f};

extern const uint8_t embedded_sample_start[] asm("_binary_assets_audio_samples_dream_tides_full_mono10k_pcm_start");
extern const uint8_t embedded_sample_end[] asm("_binary_assets_audio_samples_dream_tides_full_mono10k_pcm_end");

template <size_t kCount>
void fillHarmonicTable(Wavetable& table, const float (&harmonics)[kCount]) {
  float peak = 0.0f;

  for (int sample = 0; sample < app_config::kWavetableSize; ++sample) {
    const float phase = (kTwoPi * static_cast<float>(sample)) /
                        static_cast<float>(app_config::kWavetableSize);
    float value = 0.0f;

    for (size_t harmonic = 0; harmonic < kCount; ++harmonic) {
      const float amplitude = harmonics[harmonic];
      if (amplitude == 0.0f) {
        continue;
      }

      value += amplitude * sinf(phase * static_cast<float>(harmonic + 1));
    }

    table.samples[sample] = value;
    peak = fmaxf(peak, fabsf(value));
  }

  if (peak <= 0.0f) {
    return;
  }

  for (float& value : table.samples) {
    value /= peak;
  }
}

const Wavetable& wavetableFor(Waveform waveform) {
  switch (waveform) {
    case Waveform::kWarm:
      return warm_table;
    case Waveform::kHollow:
      return hollow_table;
    case Waveform::kBright:
      return bright_table;
    default:
      return warm_table;
  }
}

const int16_t* embeddedSampleData() {
  return reinterpret_cast<const int16_t*>(embedded_sample_start);
}

size_t embeddedSampleCount() {
  return static_cast<size_t>(embedded_sample_end - embedded_sample_start) / sizeof(int16_t);
}

}  // namespace

bool AudioEngine::begin() {
  initializeWavetables();

  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = app_config::kAudioSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = app_config::kAudioFramesPerChunk,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) {
    return false;
  }

  const i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = static_cast<int>(board_profile::kI2sBclk),
      .ws_io_num = static_cast<int>(board_profile::kI2sWs),
      .data_out_num = static_cast<int>(board_profile::kI2sDout),
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    return false;
  }

  if (i2s_set_clk(
          I2S_NUM_0,
          app_config::kAudioSampleRate,
          I2S_BITS_PER_SAMPLE_16BIT,
          I2S_CHANNEL_STEREO) != ESP_OK) {
    return false;
  }

  return true;
}

void AudioEngine::update() {
  fillBuffer();

  size_t bytes_written = 0;
  i2s_write(I2S_NUM_0, sample_buffer, sizeof(sample_buffer), &bytes_written, portMAX_DELAY);
}

void AudioEngine::setFrequency(float frequency_hz) {
  taskENTER_CRITICAL(&parameter_lock_);
  // Keep phase continuity: only the increment changes, phase_ itself is never reset here.
  frequency_hz_ = frequency_hz;
  taskEXIT_CRITICAL(&parameter_lock_);
}

void AudioEngine::setVolume(float volume) {
  taskENTER_CRITICAL(&parameter_lock_);
  volume_ = volume;
  taskEXIT_CRITICAL(&parameter_lock_);
}

void AudioEngine::setWaveform(Waveform waveform) {
  taskENTER_CRITICAL(&parameter_lock_);
  if (waveform == target_waveform_) {
    taskEXIT_CRITICAL(&parameter_lock_);
    return;
  }

  target_waveform_ = waveform;
  taskEXIT_CRITICAL(&parameter_lock_);
}

Waveform AudioEngine::waveform() const {
  taskENTER_CRITICAL(&parameter_lock_);
  const Waveform waveform = target_waveform_;
  taskEXIT_CRITICAL(&parameter_lock_);
  return waveform;
}

const char* AudioEngine::waveformName(Waveform waveform) {
  switch (waveform) {
    case Waveform::kSine:
      return "sine";
    case Waveform::kSquare:
      return "square";
    case Waveform::kTriangle:
      return "triangle";
    case Waveform::kWarm:
      return "warm";
    case Waveform::kHollow:
      return "hollow";
    case Waveform::kBright:
      return "bright";
    case Waveform::kSample:
      return "sample";
  }

  return "unknown";
}

void AudioEngine::initializeWavetables() {
  if (wavetables_initialized) {
    return;
  }

  fillHarmonicTable(warm_table, kWarmHarmonics);
  fillHarmonicTable(hollow_table, kHollowHarmonics);
  fillHarmonicTable(bright_table, kBrightHarmonics);
  wavetables_initialized = true;
}

float AudioEngine::sampleWavetable(Waveform waveform, float phase) {
  const Wavetable& table = wavetableFor(waveform);
  const float normalized_phase = phase / kTwoPi;
  const float position = normalized_phase * static_cast<float>(app_config::kWavetableSize);
  const int index = static_cast<int>(position) % app_config::kWavetableSize;
  const int next_index = (index + 1) % app_config::kWavetableSize;
  const float fraction = position - static_cast<float>(index);
  const float current = table.samples[index];
  const float next = table.samples[next_index];
  return current + (next - current) * fraction;
}

float AudioEngine::sampleEmbeddedLoop(float sample_position) {
  const size_t sample_count = embeddedSampleCount();
  if (sample_count == 0) {
    return 0.0f;
  }

  while (sample_position >= static_cast<float>(sample_count)) {
    sample_position -= static_cast<float>(sample_count);
  }

  while (sample_position < 0.0f) {
    sample_position += static_cast<float>(sample_count);
  }

  const size_t index = static_cast<size_t>(sample_position) % sample_count;
  const size_t next_index = (index + 1) % sample_count;
  const float fraction = sample_position - static_cast<float>(index);
  const float current = static_cast<float>(embeddedSampleData()[index]) / 32768.0f;
  const float next = static_cast<float>(embeddedSampleData()[next_index]) / 32768.0f;
  return (current + (next - current) * fraction) * kEmbeddedSampleGain;
}

float AudioEngine::samplePositionStep(float frequency_hz) {
  return (kEmbeddedSampleRateHz / static_cast<float>(app_config::kAudioSampleRate)) *
         (frequency_hz / kEmbeddedSampleBaseFrequencyHz);
}

float AudioEngine::renderWaveformSample(Waveform waveform, float phase, float sample_position) {
  switch (waveform) {
    case Waveform::kSine:
      return sinf(phase);
    case Waveform::kSquare:
      return phase < kPi ? 1.0f : -1.0f;
    case Waveform::kTriangle: {
      if (phase < kHalfPi) {
        return phase / kHalfPi;
      }
      if (phase < kPi + kHalfPi) {
        return 2.0f - (phase / kHalfPi);
      }
      return (phase / kHalfPi) - 4.0f;
    }
    case Waveform::kWarm:
    case Waveform::kHollow:
    case Waveform::kBright:
      return sampleWavetable(waveform, phase);
    case Waveform::kSample:
      return sampleEmbeddedLoop(sample_position);
  }

  return 0.0f;
}

void AudioEngine::fillBuffer() {
  float frequency_hz = 0.0f;
  float volume = 0.0f;
  Waveform target_waveform = Waveform::kSine;

  taskENTER_CRITICAL(&parameter_lock_);
  frequency_hz = frequency_hz_;
  volume = volume_;
  target_waveform = target_waveform_;
  taskEXIT_CRITICAL(&parameter_lock_);

  if (target_waveform != waveform_) {
    if (waveform_ == Waveform::kSample) {
      previous_sample_position_ = sample_position_;
    } else {
      previous_sample_position_ = 0.0f;
    }

    previous_waveform_ = waveform_;
    waveform_ = target_waveform;
    waveform_blend_ = 0.0f;

    if (waveform_ == Waveform::kSample) {
      sample_position_ = 0.0f;
    }
  }

  const float phase_step = kTwoPi * frequency_hz / static_cast<float>(app_config::kAudioSampleRate);
  const float sample_step = samplePositionStep(frequency_hz);
  const float sample_wrap = static_cast<float>(embeddedSampleCount());
  float phase = phase_;
  float sample_position = sample_position_;
  float previous_sample_position = previous_sample_position_;

  for (int i = 0; i < app_config::kAudioFramesPerChunk; ++i) {
    const float current_sample = renderWaveformSample(waveform_, phase, sample_position);
    const float previous_sample =
        renderWaveformSample(previous_waveform_, phase, previous_sample_position);
    const float mixed_sample =
        previous_sample + (current_sample - previous_sample) * waveform_blend_;
    const float sample = mixed_sample * volume;
    const int16_t pcm = static_cast<int16_t>(sample * 32767.0f);

    sample_buffer[i].left = pcm;
    sample_buffer[i].right = pcm;

    if (waveform_blend_ < 1.0f) {
      waveform_blend_ += app_config::kWaveformBlendStep;
      if (waveform_blend_ >= 1.0f) {
        waveform_blend_ = 1.0f;
        previous_waveform_ = waveform_;
      }
    }

    phase += phase_step;
    if (phase >= kTwoPi) {
      phase -= kTwoPi;
    }

    if (waveform_ == Waveform::kSample && sample_wrap > 0.0f) {
      sample_position += sample_step;
      while (sample_position >= sample_wrap) {
        sample_position -= sample_wrap;
      }
    }

    if (previous_waveform_ == Waveform::kSample && sample_wrap > 0.0f) {
      previous_sample_position += sample_step;
      while (previous_sample_position >= sample_wrap) {
        previous_sample_position -= sample_wrap;
      }
    }
  }

  phase_ = phase;
  sample_position_ = sample_position;
  previous_sample_position_ = previous_sample_position;
}
