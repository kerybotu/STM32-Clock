# STM32L010F4 时钟接线

本接线表对应当前 `main.c`，芯片为 **STM32L010F4Px，TSSOP20**，所有逻辑电平使用 3.3 V。

## 芯片供电与封装引脚

| 芯片引脚 | 信号 | 连接 |
|---:|---|---|
| 1 | PB9 | 未使用 |
| 2 | PC14 / OSC32_IN | LSE 晶振一端 |
| 3 | PC15 / OSC32_OUT | LSE 晶振另一端 |
| 4 | NRST | 烧录器 RST；可接 10 kΩ 上拉到 3V3 |
| 5 | VDDA | 3V3 |
| 6 | PA0 | 未使用 |
| 7 | PA1 | LED 控制输出 |
| 8 | PA2 | 未使用 |
| 9 | PA3 | 未使用 |
| 10 | PA4 | 未使用 |
| 11 | PA5 | 未使用 |
| 12 | PA6 | 未使用 |
| 13 | PA7 | 未使用 |
| 14 | PB1 | 未使用 |
| 15 | VSS | GND |
| 16 | VDD | 3V3 |
| 17 | PA9 | OLED SCK/SCL |
| 18 | PA10 | OLED SDA |
| 19 | PA13 | SWDIO/SWD |
| 20 | PA14 | SWCLK/CLK |

芯片的 VDD、VDDA 各自就近放置 100 nF 去耦电容到 GND。所有 GND 必须共地。

## OLED 四线接口

当前程序使用软件 I²C，OLED 接线如下：

| OLED | 接到 |
|---|---|
| GND | GND（芯片 VSS） |
| VDD | 3V3 |
| SCK（或 SCL） | PA9（芯片 17 脚） |
| SDA | PA10（芯片 18 脚） |

OLED 的 I²C 地址在代码中使用 `0x78`（写地址），对应常见 0x3C 的 7 位地址。PA9、PA10 被配置为开漏并启用内部上拉；若波形上升沿过慢，给 SDA/SCL 各增加约 4.7 kΩ 到 3V3 的外部上拉。

## 32.768 kHz 外部晶振（LSE）

| 器件端 | 接到 |
|---|---|
| 晶振端 1 | PC14 / OSC32_IN（芯片 2 脚） |
| 晶振端 2 | PC15 / OSC32_OUT（芯片 3 脚） |
| 负载电容 1 | PC14 到 GND |
| 负载电容 2 | PC15 到 GND |

晶振两端不要接 3V3。负载电容值按晶振规格书的 CL 和 PCB 寄生电容计算，通常从 6.8–15 pF 范围评估。当前程序在 `RCC_CSR` 中选择 **LSE 外部晶振**，不是 LSI 内部振荡器。

## LED

代码在 PA1（芯片 7 脚）上执行翻转输出。推荐接法：

```text
PA1 ── 1 kΩ～2.2 kΩ ── LED 阳极
LED 阴极 ── GND
```

当前逻辑为 PA1 输出翻转一次，LED 状态改变一次；正常运行时每秒翻转一次，因此完整亮灭周期约两秒。

## CMSIS-DAP 烧录器

| 烧录器标记 | STM32L010F4 |
|---|---|
| 3V3 | VDD（16 脚）和 VDDA（5 脚） |
| GND | VSS（15 脚） |
| SWD | PA13 / SWDIO（19 脚） |
| CLK | PA14 / SWCLK（20 脚） |
| RST | NRST（4 脚） |

烧录器的 3V3 与目标板 3V3 必须确认电压兼容；SWD、CLK、RST 使用 3.3 V 电平。烧录时保留目标板 GND 共地，连接线尽量短。

## 快速核对

```text
OLED:  GND─GND   VDD─3V3   SCK─PA9    SDA─PA10
LSE:   晶振─PC14/PC15，两端各接负载电容到 GND
LED:   PA1─限流电阻─LED─GND
SWD:   3V3─VDD/VDDA  GND─VSS  SWD─PA13  CLK─PA14  RST─NRST
```
