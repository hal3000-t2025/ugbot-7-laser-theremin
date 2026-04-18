#pragma once

#include <Arduino.h>
#include <VL53L1X.h>

class TofSensor {
 public:
  explicit TofSensor(uint8_t xshut_pin);

  void powerOff();
  bool begin(TwoWire& wire, uint8_t address = 0x29);
  bool update();

  bool isOnline() const;
  bool hasValidReading() const;
  uint16_t lastDistanceMm() const;
  bool lastReadTimedOut() const;
  uint8_t address() const;

 private:
  uint8_t xshut_pin_;
  VL53L1X sensor_;
  uint16_t last_distance_mm_ = 0;
  uint8_t address_ = 0x29;
  bool online_ = false;
  bool last_timeout_ = false;
  bool has_valid_reading_ = false;
};
