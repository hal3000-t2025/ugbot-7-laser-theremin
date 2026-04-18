#include "sensors/tof_sensor.h"

#include "app_config.h"

namespace {

bool isUsableRangeStatus(VL53L1X::RangeStatus status) {
  return status == VL53L1X::RangeValid ||
         status == VL53L1X::RangeValidMinRangeClipped ||
         status == VL53L1X::RangeValidNoWrapCheckFail;
}

}  // namespace

TofSensor::TofSensor(uint8_t xshut_pin) : xshut_pin_(xshut_pin) {}

void TofSensor::powerOff() {
  pinMode(xshut_pin_, OUTPUT);
  digitalWrite(xshut_pin_, LOW);
  delay(10);
  online_ = false;
  has_valid_reading_ = false;
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
  has_valid_reading_ = false;
  return true;
}

bool TofSensor::update() {
  if (!online_) {
    has_valid_reading_ = false;
    return false;
  }

  if (!sensor_.dataReady()) {
    return false;
  }

  const uint16_t measured_distance_mm = sensor_.read(false);
  last_timeout_ = sensor_.timeoutOccurred();
  if (last_timeout_) {
    has_valid_reading_ = false;
    return false;
  }

  if (!isUsableRangeStatus(sensor_.ranging_data.range_status) || measured_distance_mm == 0) {
    has_valid_reading_ = false;
    return false;
  }

  last_distance_mm_ = measured_distance_mm;
  has_valid_reading_ = true;
  return true;
}

bool TofSensor::isOnline() const {
  return online_;
}

bool TofSensor::hasValidReading() const {
  return has_valid_reading_;
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
