#pragma once

class ExponentialSmoother {
 public:
  explicit ExponentialSmoother(float alpha) : alpha_(alpha) {}

  float update(float input) {
    if (!initialized_) {
      value_ = input;
      initialized_ = true;
      return value_;
    }

    value_ += alpha_ * (input - value_);
    return value_;
  }

  float value() const { return value_; }
  bool initialized() const { return initialized_; }
  void setAlpha(float alpha) { alpha_ = alpha; }
  float alpha() const { return alpha_; }
  void reset() { initialized_ = false; value_ = 0.0f; }

 private:
  float alpha_ = 0.25f;
  float value_ = 0.0f;
  bool initialized_ = false;
};
