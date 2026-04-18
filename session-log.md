# 开发记录

## 2026-04-07

### 已完成

- 确认主控可用：`ESP32-C3-DevKitM-1`
- 建立 `PlatformIO + Arduino` 工程
- 完成 `M1`
- 实现 `MAX98357A` I2S 发声
- 成功烧录并验证固定测试音输出
- 完成 `M2`
- 接入 `1 x VL53L1X`
- 实现距离读取、指数平滑、距离到音高映射
- 成功烧录并通过串口日志确认传感器数据和音高变化正常

### 当前引脚分配

- `GPIO4` -> `MAX98357A BCLK`
- `GPIO5` -> `MAX98357A LRC / WS`
- `GPIO6` -> `MAX98357A DIN`
- `GPIO0` -> `VL53L1X SDA`
- `GPIO1` -> `VL53L1X SCL`
- `GPIO3` -> 第 1 个 `VL53L1X XSHUT`
- `GPIO7` -> 第 2 个 `VL53L1X XSHUT` 预留

### 当前状态

- `M2` 固件已烧录到板子
- 喇叭发声成功
- 单个 `VL53L1X` 已读通
- 串口能输出 `distance / smoothed / pitch`

### 已知问题

- 功放板电源连接疑似接触不良
- 需要下次优先排查 `VIN / GND / SD` 接触稳定性

### 下次继续

- 先排查并固定功放板供电
- 然后进入 `M3`
- 接入第 2 个 `VL53L1X`
- 完成双传感器：`pitch + volume`

## 2026-04-08

### 已完成

- 将工程迁移为同时支持 `ESP32-C3` 和 `ESP32-S3`
- 新增 `board_profile.h`，按目标芯片切换引脚规划
- 新增 `ESP32-S3` PlatformIO 环境
- 新增 `S3-MIGRATION.md`
- 本地编译验证通过：
  - `esp32-c3-devkitm-1`
  - `esp32-s3-devkitc-1`

### 当前 S3 迁移策略

- 先使用 `esp32-s3-devkitc-1` 作为近似开发板目标
- 等 `ESP32-S3 N16R8 44脚` 到货后，再按实物丝印校正串口和引脚

### 当前 S3 预设引脚

- `GPIO4` -> `MAX98357A BCLK`
- `GPIO5` -> `MAX98357A LRC / WS`
- `GPIO6` -> `MAX98357A DIN`
- `GPIO8` -> Pitch 传感器 `SDA`
- `GPIO9` -> Pitch 传感器 `SCL`
- `GPIO13` -> Pitch 传感器 `XSHUT`
- `GPIO11` -> Volume 传感器 `SDA`
- `GPIO12` -> Volume 传感器 `SCL`
- `GPIO14` -> Volume 传感器 `XSHUT`

### 已知限制

- 你买的 `S3 N16R8 44脚` 不是唯一官方板号
- 当前接线说明属于“按 GPIO 编号”的预设方案
- 板子到货后仍需按实际丝印复核

### 下次继续

- 等 `ESP32-S3` 到货
- 先确认实物丝印和串口设备名
- 再烧录 `esp32-s3-devkitc-1`
- 验证 `MAX98357A`
- 再进入 `M3` 双传感器开发

## 2026-04-12

### 已完成

- 检测到新板串口：`/dev/cu.usbserial-A5069RR4`
- 用 `esptool` 确认芯片为 `ESP32-S3`
- 确认特性：`Embedded PSRAM 8MB`
- 成功烧录 `esp32-s3-devkitc-1` 目标固件
- 抓取串口日志确认：
  - `I2S init ok`
  - 第 1 个 `VL53L1X init ok`
  - 距离和音高映射实时输出正常

### 本次实测通过的 S3 引脚

- `GPIO4` -> `MAX98357A BCLK`
- `GPIO5` -> `MAX98357A LRC / WS`
- `GPIO6` -> `MAX98357A DIN`
- `GPIO8` -> 第 1 个 `VL53L1X SDA`
- `GPIO9` -> 第 1 个 `VL53L1X SCL`
- `GPIO13` -> 第 1 个 `VL53L1X XSHUT`

### 说明

- 当前固件仍是 `M2`
- 所以本次只实测了第 1 个传感器
- 第 2 个传感器预留引脚尚未进入功能验证：
  - `GPIO11` -> SDA
  - `GPIO12` -> SCL
  - `GPIO14` -> XSHUT

### 后续结果

- 已继续完成 `M3`
- 成功烧录 `ESP32-S3` 双传感器版本
- 串口日志确认：
  - `Pitch VL53L1X init ok at 0x29`
  - `Volume VL53L1X init ok at 0x29`
  - 持续输出 `pitch=... | volume=...`

### 当前状态

- `M3` 已完成
- `ESP32-S3` 实机可用
- 双传感器已经同时在线
- `pitch` 与 `volume` 两路数据链路都正常

### 注意

- 当前 volume 传感器读数偏近，日志里长期接近最大音量 `0.28`
- 这更像安装位置或手离得太近，不是初始化失败
- 如果手感不自然，下一步优先调 `volume` 映射范围

### 下次继续

- 做 `M3` 手感微调
- 或直接进入 `M4`
- 支持 `sine / square / triangle` 三种波形切换

### 后续结果

- 已完成 `M4`
- 音频引擎支持：
  - `sine`
  - `square`
  - `triangle`
- 串口命令已验证：
  - `sine`
  - `square`
  - `triangle`
  - `status`
  - `mute`
  - `unmute`

### 当前 M4 行为

- 默认静音启动
- 上电不会直接让喇叭发声
- 需要手动串口输入 `unmute` 才会恢复音频输出
- 波形切换时带短淡变，避免硬切爆音

### 当前状态

- `M4` 已完成
- 板子上的当前固件为“默认静音版 M4”
- 双传感器和波形切换逻辑都已验证

## 2026-04-14 / 2026-04-15

### 已完成

- 确认 `ESP32-S3` 当前实测串口为 `/dev/cu.usbserial-A5069RR4`
- 确认 `MAX98357A` 在这块 `S3` 板上不能直接吃板载 `5V` 引脚供电
- 用户向店铺确认：`5V` 引脚是否输出取决于 `IN-OUT` 焊盘是否打通；未焊时仅支持输入
- 已改为稳定接法：
  - `ESP32 3V3` -> `MAX98357A VIN`
  - `ESP32 GND` -> `MAX98357A GND`
  - `ESP32 3V3` -> `MAX98357A SD`
  - 喇叭红线 -> `SPK+`
  - 喇叭黑线 -> `SPK-`
- 按上述接法，声音已恢复
- `M4` 当前固件已改为“默认低音量输出”，不再默认静音
- 当前音量配置：
  - `kMinimumAudibleVolume = 0.02`
  - `kMaximumVolume = 0.08`

### 新增功能

- 已加入校准参数结构：
  - `src/control/calibration_profile.h`
- 已加入校准参数持久化：
  - `src/control/calibration_store.h`
  - `src/control/calibration_store.cpp`
- 已在 `main.cpp` 接入校准命令与 NVS 存储
- 当前串口支持的校准相关命令：
  - `cal pitch near`
  - `cal pitch far`
  - `cal volume near`
  - `cal volume far`
  - `set pitch near <mm>`
  - `set pitch far <mm>`
  - `set volume near <mm>`
  - `set volume far <mm>`
  - `set smooth pitch <0.01-1.0>`
  - `set smooth volume <0.01-1.0>`
  - `save`
  - `load`
  - `defaults`
  - `status`

### 已验证

- 当前空场基线大约是：
  - `pitch ≈ 1730 mm`
  - `volume ≈ 1580 mm`
- 已通过串口验证以下命令有效：
  - `status`
  - `defaults`
  - `set volume far 500`
  - `save`
- 启动时已能打印校准参数
- 代码已能从 `Preferences` 读取和写入校准参数

### 当前风险和注意点

- 校准仍未完成，今天只完成了功能接入和串口验证
- 做 `cal pitch near/far` 时，如果用户手已经移开，系统会把“空场距离”误记为校准点
- 今天发生过一次误采样：
  - 串口记录成 `pitch near=1736`
  - 这显然是空场值，不可用
- 误采样后已立即恢复到合理范围：
  - `set pitch near 60`
  - `set pitch far 500`
- 目前板子上的 `pitch` 范围应仍为：
  - `near = 60`
  - `far = 500`
- 目前 `volume far` 已被设为 `500`
- 是否已执行 `save` 以持久化当前最终值，需要明天开机后先用 `status` 再确认一次

### 明天继续的建议顺序

1. 上电后先串口输入 `status`
2. 确认当前校准参数是否为：
   - `pitch near=60`
   - `pitch far=500`
   - `volume near=50`
   - `volume far=500`
3. 如果参数不对，先执行：
   - `defaults`
   - `set volume far 500`
   - `save`
4. 再开始正式校准，推荐不要用“发命令瞬时抓拍”的方式
5. 更稳的做法是：
   - 先让用户把手稳定放在目标位置
   - 从实时串口日志读出稳定距离
   - 再用 `set ... <mm>` 手动写入
6. 推荐校准顺序：
   - `pitch near`
   - `pitch far`
   - `volume near`
   - `volume far`
   - 视手感再调整 `smooth`
   - 最后 `save`

### 继续时的目标

- 完成一套可保存的手感校准参数
- 让 `pitch` 有清晰可控的高低音区
- 让 `volume` 不再长期贴着最小值 `0.02`
- 校准稳定后再考虑：
  - 更好的音高映射曲线
  - 按钮切换音色
  - OLED 显示

## 2026-04-15 收尾

### 已完成

- 已完成 `M5`
- `audio_engine` 新增 3 组内置 `wavetable` 音色：
  - `warm`
  - `hollow`
  - `bright`
- 串口命令已扩展为 `1..6`：
  - `1` -> `sine`
  - `2` -> `square`
  - `3` -> `triangle`
  - `4` -> `warm`
  - `5` -> `hollow`
  - `6` -> `bright`
- `cal pitch/volume near/far` 已从“抓上一帧”改为“连续采 8 帧求平均”
- 校准过程中会打印采样完成结果：
  - 平均值
  - 最小值
  - 最大值
  - 样本数
- `pitch` 映射已改为对数频率映射，音区手感比线性频率更自然
- `volume` 映射已加入响应曲线，远端不再那么容易长期贴着最小音量
- 默认 `volume far` 已提升到 `500 mm`
- `platformio.ini` 默认环境已切换到 `esp32-s3-devkitc-1`

### 本地验证

- 已重新编译通过：
  - `esp32-c3-devkitm-1`
  - `esp32-s3-devkitc-1`
- 已成功烧录到：
  - `/dev/cu.usbserial-A5069RR4`
- 已通过串口确认：
  - `Laser Theremin M5 boot`
  - `warm` 命令切换成功
  - 新 `cal ...` 稳态采样逻辑会输出 `Captured ... (min/max/samples)`
  - `stream off` / `stream on` 已生效

## 2026-04-15 微调

### 已完成

- `volume` 最小值已改为真正静音：
  - `0.00`
- 已加入运行时可调的音量静音门限：
  - `set gate volume <0.00-0.80>`
- 该门限已接入：
  - `status`
  - `save`
  - `load`
  - `defaults`

### 用途

- 如果手还没靠近音量传感器就开始出声：
  - 把 `gate` 调大
- 如果必须贴得太近才出声：
  - 把 `gate` 调小

### 新增工具

- 已新增：
  - `tools/theremin_tool.py`
- 特点：
  - 纯 Python 标准库
  - 不依赖 `pyserial`
  - 可直接列串口、发命令、做 smoke test、恢复默认值、引导校准
  - 发送命令前可自动关闭状态流，减少刷屏干扰

### 现在的建议验收顺序

1. 烧录 `ESP32-S3` 最新固件
2. 上电后先执行 `status`
3. 依次测试 6 个音色切换命令
4. 执行：
   - `cal pitch near`
   - `cal pitch far`
   - `cal volume near`
   - `cal volume far`
5. 确认参数合理后执行 `save`
6. 重启后再用 `status` 确认 NVS 已恢复

## 2026-04-15 预设按钮

### 已完成

- `GPIO16` 已接入 `DFRobot DFR0789 LED Switch`
- 固件已加入硬件按钮去抖：
  - `30ms`
- 每次按钮状态翻转一次，会切到下一个预设音色：
  - `1 sine`
  - `2 square`
  - `3 triangle`
  - `4 warm`
  - `5 hollow`
  - `6 bright`
- 串口 `status` 现在会输出：
  - `preset=<n>/6`
  - `name=<preset>`
- 为了验证同一套逻辑，串口新增：
  - `next`
  - `preset <1-6>`

### 说明

- `DFR0789` 是自锁按钮，不是瞬时按键
- 当前实现对每次稳定的高低电平翻转都视为一次“切到下一个预设”
- 这样可以直接兼容这颗按钮的锁存输出行为

## 2026-04-15 音域与最大音量

### 已完成

- 默认音高跨度已从 `2` 个八度扩到 `3` 个八度：
  - `220Hz -> 1760Hz`
- 演奏最大音量不再写死为旧的保守值：
  - 默认已从 `0.08` 提升到 `0.14`
- 新增串口命令：
  - `set max volume <0.05-0.25>`
- 该最大音量参数已接入：
  - `status`
  - `save`
  - `load`
  - `defaults`

### 说明

- 现在静音主要依赖“手不进入检测区”以及 `gate`
- 因此最大音量可以单独放开，不必继续依赖低上限去压常驻底噪
- 现场如果要继续试喇叭承受范围，后续可直接调：
  - `0.14`
  - `0.16`
  - `0.18`

## 2026-04-15 响应速度优化

### 已完成

- `VL53L1X` 距离模式已从偏稳的长距离模式改为中距离模式
- 传感器 timing budget 已收紧到：
  - `20000 us`
- 传感器连续测距周期已收紧到：
  - `20 ms`
- 主循环传感器轮询周期已同步调到：
  - `20 ms`
- `I2C` 频率已从 `100 kHz` 提到：
  - `400 kHz`
- 当前板子已把保存的平滑参数改成：
  - `pitch smooth = 0.18`
  - `volume smooth = 0.16`

### 目标

- 在不重新引入明显颗粒感和脉冲感的前提下，把整体跟手度往更快的方向推一档

## 2026-04-15 响应速度优化第二档

### 已完成

- `VL53L1X` 已进一步切到：
  - `Short`
  - `20000 us`
- 传感器轮询周期已进一步收紧到：
  - `10 ms`
- 传感器读取已改成：
  - 先判断 `dataReady()`
  - 再执行 `read(false)`
- 当前板子已把保存的平滑参数改成：
  - `pitch smooth = 0.24`
  - `volume smooth = 0.20`

### 说明

- 这一档的重点不是继续盲目压测距预算，而是把主循环里的阻塞读去掉
- 因此相对上一档，会更像“更早拿到新数据”，而不是单纯冒险压缩传感器内部积分时间

## 2026-04-15 采样预设原型

### 已完成

- 使用 [正弦波测试.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/正弦波测试.wav) 截取出：
  - [正弦波测试_3s_mono16k.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/正弦波测试_3s_mono16k.wav)
- 已转换生成嵌入固件用的原始 PCM：
  - `assets/audio_samples/sine_test_3s_mono16k.pcm`
- `AudioEngine` 已新增 `sample` 波形类型
- 当前 `sample` 预设行为：
  - 整段循环播放
  - 根据当前 pitch 做变速重采样
  - 通过线性插值读取 PCM
- 已接入当前统一预设系统，作为：
  - `7 sample`
- 串口与硬件按钮都可切到这个采样预设

### 说明

- 这一版先验证“采样能弹起来”
- 还没有做更高级的：
  - loop 点优化
  - crossfade loop
  - ADSR
  - 多 sample bank

## 2026-04-16 乐谱自动播放原型

### 已完成

- 新增 `scores/twinkle_twinkle.abc`
- 新增 `tools/abc_to_score.py`
  - 当前支持最小子集 `ABC`：
    - 单声部旋律
    - 基础音符
    - 休止符
    - 基本时值
    - 基本速度
    - 基本调号
- 新增 `ScorePlayer`
  - 以事件数组驱动自动播放
  - 当前按 `MIDI note + duration_ms` 运行
  - 支持循环播放
- 新增生成文件：
  - `src/control/generated_scores.h`
- `PlatformIO` 已在构建前自动执行：
  - `.abc -> generated_scores.h`
- 统一预设列表已扩展为：
  - `1 sine`
  - `2 square`
  - `3 triangle`
  - `4 warm`
  - `5 hollow`
  - `6 bright`
  - `7 sample`
  - `8 twinkle`
- 切到 `twinkle` 后会自动播放《小星星》
- 当前 `twinkle` 预设绑定的是：
  - `sample` 音源
- `status` 已新增乐谱状态输出：
  - `score=<name> event=<n>/<total> rest=<yes|no>`

### 说明

- 这一版先把“ABC -> 自动播放预设”整条链路打通
- 还没有继续做：
  - 多首乐曲自动汇总
  - 非循环播放
  - 播放控制命令
  - 乐谱和采样 bank 的更细分绑定策略

## 2026-04-16 乐谱模式手势控制

### 已完成

- 在自动播放预设中，`pitch` 传感器不再控制音高
- 当前改为：
  - `pitch` 传感器控制播放速度
  - `volume` 传感器继续控制音量
- 速度映射规则：
  - 不遮挡时保持默认 `1.00x`
  - 手靠近 `pitch` 传感器时保持较慢
  - 手离远时逐步加速
  - 当前上限为 `3.00x`
- `status` 已新增速度回读：
  - `rate=<x.xx>x`

### 说明

- 这样自动播放模式下仍然保留了双手势交互
- 当前这版只对 `score` 预设生效，不影响普通演奏模式

## 2026-04-17 twinkle 预设收口

### 已完成

- `8 twinkle` 预设不再绑定自动播放逻辑
- 当前 `twinkle` 改成普通预设，底层使用第 `4` 个音色 `warm`
- 切到 `twinkle` 后，行为现在和其它音色完全一致：
  - `pitch` 传感器控制音高
  - `volume` 传感器控制音量

### 说明

- `ABC / generated_scores / ScorePlayer` 代码仍然保留，方便后续 `2.0` 继续扩展
- 但 `1.0` 当前对外行为已经收口为“所有预设都按同一套手势逻辑演奏”

## 2026-04-17 twinkle 曲目替换

### 已完成

- 重新启用 `8 twinkle` 的乐谱自动播放逻辑
- 将 [scores/twinkle_twinkle.abc](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/scores/twinkle_twinkle.abc) 的内容从《小星星》替换为：
  - `Lonely People Are Shameful / 孤独的人是可耻的`
- 由于当前 `ABC` 转换脚本只支持标准升号写法，已把乐谱中的 `F#` 统一改写成 `^F`
- `twinkle` 预设仍绑定第 `4` 个音色 `warm`

### 说明

- 这次改动只替换了 `twinkle` 槽位承载的曲目
- 预设名仍然保留 `twinkle`，只是内容已经不是《小星星》

## 2026-04-16 采样响度对齐

### 已完成

- 检查了当前内置 sample 的峰值：
  - 约为满幅的 `59%`
- 为了让 `sample / twinkle` 的音量尺度更接近普通合成音色，已做两处调整：
  - 内置 sample 播放增益上调
  - `ABC -> score` 生成时默认 `volume_scale = 1.00`

### 说明

- 这样在乐谱自动播放模式下，`volume` 传感器靠近时更容易达到足够高的响度
- 同时仍然保留整体最大音量受 `set max volume` 约束

## 2026-04-16 替换采样素材

### 已完成

- 新增并保留了以下裁切文件：
  - [Dream Tides_10s.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/Dream%20Tides_10s.wav)
  - [Dream Tides_10s_mono16k.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/Dream%20Tides_10s_mono16k.wav)
- `sample` 预设和 `twinkle` 当前绑定的内置 sample，已从旧的 `正弦波测试` 替换为：
  - `Dream Tides` 前 `10` 秒
- 新的固件嵌入资源为：
  - `assets/audio_samples/dream_tides_10s_mono16k.pcm`

### 说明

- 新素材来源：
  - [Dream Tides.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/Dream%20Tides.wav)
- 当前仍使用：
  - `mono`
  - `16-bit PCM`
  - `16kHz`
- 采样峰值约为满幅的 `0.596`
- 因此当前 sample 播放增益保持不变，仍处于安全范围内

## 2026-04-16 整首采样版本

### 已完成

- 把 `sample` 预设和当前 `twinkle` 绑定的内置 sample，继续从 `10` 秒版本替换成了整首版本
- 当前整首素材来源：
  - [Dream Tides.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/Dream%20Tides.wav)
- 当前转换规格改为：
  - `mono`
  - `16-bit PCM`
  - `10kHz`
  - 全长约 `131.68s`
- 当前生成文件：
  - [dream_tides_full_mono10k.wav](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/src/audio/dream_tides_full_mono10k.wav)
  - `assets/audio_samples/dream_tides_full_mono10k.pcm`
- 为了让 `ESP32-C3` 继续可编译，工程已改成单应用大分区：
  - [partitions_single_app.csv](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/partitions_single_app.csv)

### 说明

- 这版目标是先验证“整首曲子直接嵌进 sample 音色”是否可接受
- 代价是固件体积会显著增加

## 2026-04-16 当前定型结论

### 已确认

- 当前项目除“手感打磨”之外，主功能链路已基本定型：
  - 双传感器演奏
  - 硬件按钮切预设
  - 内置 wavetable
  - 可演奏 sample 音色
  - `ABC` 转自动播放预设
- `sample` 预设的产品语义已明确：
  - 它不是固定速度播放器
  - 而是“按 pitch 连续变速的可演奏采样音色”
  - 后续即使频繁替换 sample 内容，也保持这套语义

### 下一阶段重点

- 后续不再大改功能方向，主要聚焦：
  - 手感
  - 映射曲线
  - 可演奏性
- 当前用户偏好已经明确：
  - 友好的 pitch 映射不应只是线性手势位置
  - 更适合尝试弧线/曲线型控制律

### 补充确认

- `sample` 预设的长期产品语义再次确认：
  - 保持“按 `pitch` 连续变速的可演奏采样音色”
  - 后续允许频繁替换 sample 内容
  - 默认通过“重新编译 + 重新刷机”更新，不引入 `OTA`
- 当前项目状态判断更新为：
  - 除手感问题外，其余主功能基本可以定型
  - 后续开发重点从“加功能”转为“打磨演奏体验”

### 手感讨论准备

- 当前 `pitch` 手感链路是：
  - 手势距离线性归一化
  - 再映射到 `220Hz - 1760Hz` 的对数频率范围
- 当前 `volume` 手感链路已经带弧线：
  - 先做 `gate`
  - 再做幂函数响应曲线
- 因此下一轮手感优化的主要矛盾已经明确：
  - 重点不再是“有没有对数频率”
  - 而是“线性手势位置是否应该先经过一条更友好的弧线”
- 下一步优先讨论的候选方向：
  - `power/gamma`
  - `smoothstep`
  - 分段曲线
  - 以及是否把曲线参数做成可调

## 2026-04-16 手感 / 状态输出第一轮落地

- 已把 `pitch` 手势映射从“线性手势位置 -> 对数频率”升级到第一版弧线化方案：
  - 当前采用 `power/gamma`
  - 默认 `pitch curve gamma = 0.75`
  - 已接入 `status / save / load / defaults`
  - 已新增串口命令：
    - `set curve pitch <0.30-2.50>`
- 当前含义：
  - 当前方向改为：离 `pitch` 传感器越近音越低，离远音越高
  - `curve < 1.0` 扩展高音区
  - `curve > 1.0` 扩展低音区
- 自动状态刷屏也已修正为“真实输出视角”：
  - 普通演奏模式继续显示 `pitch_in => Hz`
  - `score` 模式改为显示 `pitch_in => rate`，并额外显示 `out=... Hz`
  - 避免把传感器推导值误读成实际输出值

## 2026-04-18 文档与命名同步

- 当前 `master` 已以最新功能状态为准完成文档同步：
  - 预设总数为 `10`
  - 自动播放预设正式命名为 `example / example2`
  - `example2` 当前绑定《天空之城》主旋律，并整体升高 `1` 个八度
- 当前用于复刻的主文档以这两份为准：
  - [README.md](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/README.md)
  - [激光特雷门琴复现指南.md](/Users/houtao/ai/Mcp_Dev/激光特雷门琴/激光特雷门琴复现指南.md)
- 日志里早期出现的 `twinkle`、旧串口名、旧预设数量等内容保留为历史记录，不再代表当前主分支状态
