#pragma once

#include "control/calibration_profile.h"

class CalibrationStore {
 public:
  bool load(CalibrationSettings& settings);
  bool save(const CalibrationSettings& settings);
};
