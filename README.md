# STM32L010F4 时钟

当前固件使用 **DS3231M 作为唯一时间源**；STM32 片内 RTC 和 PC14/PC15 的 LSE 晶振不参与走时。OLED 使用 PA9/PA10 软件 I²C，DS3231M 使用独立的 PA5/PA6 软件 I²C。逻辑电平为 3.3 V。

## STM32L010F4Px（TSSOP20）引脚

| 芯片脚 | 信号 | 用途 |
|---:|---|---|
| 1 | PB9 | 未使用 |
| 2 | PC14 / OSC32_IN | 未使用；不接 LSE |
| 3 | PC15 / OSC32_OUT | 未使用；不接 LSE |
| 4 | NRST | 烧录器 RST |
| 5 | VDDA | 3V3，近端 100 nF 去耦到 GND |
| 6 | PA0 | 休眠/唤醒按键，按下接 GND |
| 7 | PA1 | LED 输出 |
| 8 | PA2 | 未使用 |
| 9 | PA3 | 未使用 |
| 10 | PA4 | 调时按键，按下接 GND |
| 11 | PA5 | DS3231M SCL |
| 12 | PA6 | DS3231M SDA |
| 13 | PA7 | 未使用 |
| 14 | PB1 | 未使用 |
| 15 | VSS | GND |
| 16 | VDD | 3V3，近端 100 nF 去耦到 GND |
| 17 | PA9 | OLED SCK/SCL |
| 18 | PA10 | OLED SDA |
| 19 | PA13 | SWDIO |
| 20 | PA14 | SWCLK |

芯片、OLED、DS3231M、按键、LED 电源和烧录器必须共地。

## DS3231M 8-SO 接线

| DS3231M 8-SO 脚 | 信号 | 连接 |
|---:|---|---|
| 1 | 32KHZ | 悬空（固件关闭 32 kHz 输出） |
| 2 | VCC | 3V3 |
| 3 | INT/SQW | 悬空 |
| 4 | RST | 悬空 |
| 5 | GND | 共地 |
| 6 | VBAT | 3 V 低漏电纽扣电池正极；不用后备电池时接 GND |
| 7 | SDA | PA6（STM32 第 12 脚） |
| 8 | SCL | PA5（STM32 第 11 脚） |

PA5/SCL 和 PA6/SDA 各用 **4.7 kΩ 上拉到 3V3**。在 DS3231M 的 VCC 与 GND 之间、靠近芯片放置 100 nF 去耦电容。VBAT 电池负极接共地；不要把 VBAT 接到 3V3。DS3231M 的 I²C 地址为 7 位 `0x68`。软件读取从秒寄存器 `0x00` 连续读取 7 字节，设时从秒寄存器开始一次突发写入 7 字节。

```text
3V3 ─────────────── STM32 VDD/VDDA、OLED VDD、DS3231M pin 2 VCC
GND ─────────────── STM32 VSS、OLED GND、DS3231M pin 5 GND、电池负极
PA5 (STM32 pin 11) ─ DS3231M pin 8 SCL ── 4.7 kΩ ── 3V3
PA6 (STM32 pin 12) ─ DS3231M pin 7 SDA ── 4.7 kΩ ── 3V3
PA9 (STM32 pin 17) ─ OLED SCK/SCL
PA10(STM32 pin 18) ─ OLED SDA
DS3231M pin 6 VBAT ─ 3 V 纽扣电池正极（可选后备电源）
```

DS3231M 内部带温度补偿时钟基准，外接 LSE 晶体不是此方案的时间基准，因此 PC14/PC15 不需要连接晶振或负载电容。

## OLED、按键与 LED

| 器件端 | 接到 |
|---|---|
| OLED GND / VDD | GND / 3V3 |
| OLED SCK（SCL）/ SDA | PA9 / PA10 |
| PA0 按键 | PA0 与 GND 之间，使用 MCU 内部上拉 |
| PA4 按键 | PA4 与 GND 之间，使用 MCU 内部上拉 |
| LED | PA1 经 1 kΩ～2.2 kΩ 接 LED 阳极，LED 阴极接 GND |

按键不使用外部上拉电阻。按下时将 GPIO 接地，不要接到 3V3。PA1 每当 DS3231M 秒值变化时翻转一次，正常每秒翻转一次，完整亮灭周期约 2 秒。

## CMSIS-DAP 烧录器

| 烧录器信号 | 目标板连接 |
|---|---|
| 3V3 | 目标板 3V3、STM32 VDD 和 VDDA |
| GND | 共地 / STM32 VSS |
| SWD | PA13（STM32 第 19 脚，SWDIO） |
| CLK | PA14（STM32 第 20 脚，SWCLK） |
| RST | NRST（STM32 第 4 脚） |

确认烧录器与目标板的 3V3 电源兼容，SWD 信号使用 3.3 V 电平。

## 启动与时间设置

- 星期编号采用周一为 1、周日为 7；页眉在 `DS` 标记旁显示 `MON` 至 `SUN` 三字母缩写。设置星期时显示闪烁数字。
- 当 DS3231M 的状态寄存器 `OSF`（Oscillator Stop Flag）置位时，固件将日历初始化为 **2026-10-18 16:20:00，星期日（7）**，随后清除 OSF。
- 固件确保 EOSC=0，使振荡器在 VBAT 供电时继续运行，并关闭未使用的 32 kHz 输出。
- PA0 短按关闭 OLED 并进入等待唤醒状态，DS3231M 独立继续走时；再次短按唤醒屏幕。
- PA4 长按约 1.5 秒进入设置，项目顺序为年、月、日、星期、时、分、秒。短按增加当前项目；秒项目短按清零。长按确认并进入下一项，在秒项目长按时将编辑值一次写入 DS3231M 并退出。
- 按键采样使用 SysTick 1 ms 时基、5 ms 采样间隔、20 ms 去抖和 1500 ms 长按判定。

## 构建与调试

运行 `make clean all` 构建 ELF 和 BIN，运行 `make flash` 使用 OpenOCD/CMSIS-DAP 烧录。`ds3231_debug_status` 保存启动时的状态寄存器值；`ds3231_last_error` 为 0 表示最近一次操作成功，非零值用于定位初始化或读取/写入错误。PA1 可用于示波器测量秒变化间隔。接线和软件配置不能替代实机 I²C ACK 与走时测试。
