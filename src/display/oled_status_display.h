#pragma once

#include <Wire.h>
#include <stdint.h>

struct OledStatusSnapshot {
  uint8_t preset_index = 0;
  uint8_t preset_count = 0;
  const char* preset_name = "";
  const char* primary_label = "PIT";
  const char* secondary_label = "VOL";
  float primary_level = 0.0f;
  float secondary_level = 0.0f;
};

class OledStatusDisplay {
 public:
  OledStatusDisplay() = default;

  bool begin(TwoWire& wire, uint8_t address);
  void update(const OledStatusSnapshot& snapshot, unsigned long now_ms);
  bool ready() const;

 private:
  void drawBar(int16_t y, const char* label, float level);

  class Adafruit_SSD1306* display_ = nullptr;
  bool ready_ = false;
  unsigned long last_update_ms_ = 0;
};
