#pragma once

#include <driver/gpio.h>

namespace board_profile {

#if defined(CONFIG_IDF_TARGET_ESP32S3)

constexpr const char* kBoardName = "ESP32-S3 DevKitC-1 compatible N16R8";
constexpr bool kHasDualI2c = true;
constexpr bool kHasPresetButton = true;

// 避开 S3 常见启动相关脚、USB D+/D- 和 Flash/PSRAM 占用脚。
constexpr gpio_num_t kI2sBclk = GPIO_NUM_4;
constexpr gpio_num_t kI2sWs = GPIO_NUM_5;
constexpr gpio_num_t kI2sDout = GPIO_NUM_6;

constexpr gpio_num_t kPitchI2cSda = GPIO_NUM_8;
constexpr gpio_num_t kPitchI2cScl = GPIO_NUM_9;
constexpr gpio_num_t kPitchSensorXshut = GPIO_NUM_13;

constexpr gpio_num_t kVolumeI2cSda = GPIO_NUM_11;
constexpr gpio_num_t kVolumeI2cScl = GPIO_NUM_12;
constexpr gpio_num_t kVolumeSensorXshut = GPIO_NUM_14;
constexpr gpio_num_t kPresetButton = GPIO_NUM_16;

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

constexpr const char* kBoardName = "ESP32-C3-DevKitM-1";
constexpr bool kHasDualI2c = false;
constexpr bool kHasPresetButton = false;

// 避开 UART 下载口和常见 strapping pins，给 C3 留一组稳定的 I2S 输出脚。
constexpr gpio_num_t kI2sBclk = GPIO_NUM_4;
constexpr gpio_num_t kI2sWs = GPIO_NUM_5;
constexpr gpio_num_t kI2sDout = GPIO_NUM_6;

constexpr gpio_num_t kPitchI2cSda = GPIO_NUM_0;
constexpr gpio_num_t kPitchI2cScl = GPIO_NUM_1;
constexpr gpio_num_t kPitchSensorXshut = GPIO_NUM_3;

constexpr gpio_num_t kVolumeI2cSda = GPIO_NUM_0;
constexpr gpio_num_t kVolumeI2cScl = GPIO_NUM_1;
constexpr gpio_num_t kVolumeSensorXshut = GPIO_NUM_7;
constexpr gpio_num_t kPresetButton = GPIO_NUM_NC;

#else

#error "Unsupported target. Add a board profile before building."

#endif

}  // namespace board_profile
