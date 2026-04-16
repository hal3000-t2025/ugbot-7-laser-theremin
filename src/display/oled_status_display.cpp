#include "display/oled_status_display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <cstdio>

#include "app_config.h"

namespace {

constexpr int16_t kTopLineY = 0;
constexpr int16_t kPrimaryBarY = 11;
constexpr int16_t kSecondaryBarY = 22;
constexpr int16_t kBarHeight = 8;
constexpr int16_t kLabelWidth = 18;
constexpr int16_t kBarX = kLabelWidth;
constexpr int16_t kBarWidth = app_config::kOledWidth - kBarX;

float clampUnit(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }

  if (value > 1.0f) {
    return 1.0f;
  }

  return value;
}

}  // namespace

bool OledStatusDisplay::begin(TwoWire& wire, uint8_t address) {
  if (display_ != nullptr) {
    delete display_;
    display_ = nullptr;
  }

  display_ = new Adafruit_SSD1306(
      app_config::kOledWidth,
      app_config::kOledHeight,
      &wire,
      -1,
      app_config::kI2cFrequencyHz,
      app_config::kI2cFrequencyHz);
  if (display_ == nullptr) {
    ready_ = false;
    return false;
  }

  if (!display_->begin(SSD1306_SWITCHCAPVCC, address, true, false)) {
    delete display_;
    display_ = nullptr;
    ready_ = false;
    return false;
  }

  display_->clearDisplay();
  display_->setTextSize(1);
  display_->setTextColor(SSD1306_WHITE);
  display_->cp437(true);
  display_->setCursor(0, kTopLineY);
  display_->println(F("OLED ready"));
  display_->display();
  ready_ = true;
  last_update_ms_ = 0;
  return true;
}

void OledStatusDisplay::update(const OledStatusSnapshot& snapshot, unsigned long now_ms) {
  if (!ready_ || display_ == nullptr) {
    return;
  }

  if (last_update_ms_ != 0 &&
      now_ms - last_update_ms_ < app_config::kOledRefreshIntervalMs) {
    return;
  }

  last_update_ms_ = now_ms;

  char title[24];
  snprintf(
      title,
      sizeof(title),
      "%u/%u %s",
      static_cast<unsigned>(snapshot.preset_index),
      static_cast<unsigned>(snapshot.preset_count),
      snapshot.preset_name != nullptr ? snapshot.preset_name : "");

  display_->clearDisplay();
  display_->setCursor(0, kTopLineY);
  display_->print(title);
  drawBar(kPrimaryBarY, snapshot.primary_label, snapshot.primary_level);
  drawBar(kSecondaryBarY, snapshot.secondary_label, snapshot.secondary_level);
  display_->display();
}

bool OledStatusDisplay::ready() const {
  return ready_;
}

void OledStatusDisplay::drawBar(int16_t y, const char* label, float level) {
  if (display_ == nullptr) {
    return;
  }

  const float clamped_level = clampUnit(level);
  const int16_t fill_width = static_cast<int16_t>((kBarWidth - 2) * clamped_level);

  display_->setCursor(0, y);
  display_->print(label != nullptr ? label : "");
  display_->drawRect(kBarX, y, kBarWidth, kBarHeight, SSD1306_WHITE);
  if (fill_width > 0) {
    display_->fillRect(kBarX + 1, y + 1, fill_width, kBarHeight - 2, SSD1306_WHITE);
  }
}
