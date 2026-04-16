# ESP32-S3 迁移说明

## 目标

在 `ESP32-S3 N16R8 44脚 Type-C` 开发板到货后，直接切换到 `ESP32-S3` 目标继续开发，不再重构主工程。

当前状态：

- 工程已支持 `ESP32-C3` 和 `ESP32-S3` 双目标
- 默认开发目标已切到 `ESP32-S3`
- `ESP32-S3` 已完成双传感器、波形切换、wavetable 音色和校准持久化验证

## 如何切换到 S3

编译：

```bash
. .venv/bin/activate
pio run -e esp32-s3-devkitc-1
```

烧录：

```bash
. .venv/bin/activate
pio run -e esp32-s3-devkitc-1 -t upload
```

说明：

- 当前 `platformio.ini` 里没有写死 `S3` 串口
- 板子到货后，先执行 `ls /dev/cu.*`
- 再把实际串口填进 `platformio.ini`

## S3 预设引脚规划

这套规划优先考虑：

- 避开常见启动相关脚
- 避开 `USB D+/D-`
- 避开 `Flash / PSRAM` 常用脚
- 为 `M3` 的双传感器预留两套独立 I2C

### 音频功放 MAX98357A

- `GPIO4` -> `BCLK`
- `GPIO5` -> `LRC / WS`
- `GPIO6` -> `DIN`
- `3V3` -> `VIN`
- `GND` -> `GND`

### Pitch 传感器 VL53L1X

- `GPIO8` -> `SDA`
- `GPIO9` -> `SCL`
- `GPIO13` -> `XSHUT`
- `3V3` -> `VIN / VCC`
- `GND` -> `GND`

### Volume 传感器 VL53L1X

- `GPIO11` -> `SDA`
- `GPIO12` -> `SCL`
- `GPIO14` -> `XSHUT`
- `3V3` -> `VIN / VCC`
- `GND` -> `GND`

### 喇叭

- 红线 -> `MAX98357A SPK+`
- 黑线 -> `MAX98357A SPK-`

## 到货后先确认什么

收到板子后先核对：

1. `GPIO4/5/6/8/9/11/12/13/14` 是否都真的引出到排针
2. 板子丝印是否直接写 `IO4 / IO5 / IO6 ...`
3. USB 转串口枚举出的真实端口名
4. `5V` 和 `3V3` 的排针位置

## 需要特别注意

- 你买的是电商规格名，不是唯一官方板号
- `N16R8` 一般表示 `16MB Flash + 8MB PSRAM`
- 但不同卖家的 `44脚 S3` 板，排针顺序和丝印可能不同
- 所以上面的接线方案是“引脚号方案”，不是“按第几脚数”的方案

## 到货后建议动作

1. 发板子正反面清晰照片
2. 先跑最小串口程序确认板型和串口
3. 再接 `MAX98357A`
4. 最后接两个 `VL53L1X`
