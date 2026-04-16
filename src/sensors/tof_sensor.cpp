#include "sensors/tof_sensor.h"

#include "app_config.h"

TofSensor::TofSensor(uint8_t xshut_pin) : xshut_pin_(xshut_pin) {}

void TofSensor::powerOff() {
  pinMode(xshut_pin_, OUTPUT);
  digitalWrite(xshut_pin_, LOW);
  delay(10);
  online_ = false;
}

bool TofSensor::begin(TwoWire& wire, uint8_t address) {
  pinMode(xshut_pin_, OUTPUT);
  digitalWrite(xshut_pin_, HIGH);
  delay(10);

  sensor_.setBus(&wire);
  sensor_.setTimeout(100);

  if (!sensor_.init()) {
    online_ = false;
    return false;
  }

  if (address != 0x29) {
    sensor_.setAddress(address);
  }

  address_ = address;
  sensor_.setDistanceMode(VL53L1X::Short);
  sensor_.setMeasurementTimingBudget(app_config::kTofMeasurementTimingBudgetUs);
  sensor_.startContinuous(app_config::kTofContinuousPeriodMs);

  online_ = true;
  return true;
}

bool TofSensor::update() {
  if (!online_) {
    return false;
  }

  if (!sensor_.dataReady()) {
    return false;
  }

  last_distance_mm_ = sensor_.read(false);
  last_timeout_ = sensor_.timeoutOccurred();
  return !last_timeout_;
}

bool TofSensor::isOnline() const {
  return online_;
}

uint16_t TofSensor::lastDistanceMm() const {
  return last_distance_mm_;
}

bool TofSensor::lastReadTimedOut() const {
  return last_timeout_;
}

uint8_t TofSensor::address() const {
  return address_;
}
