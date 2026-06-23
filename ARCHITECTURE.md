# 智能门锁主控板 - 软件架构文档

> 硬件平台: ESP32-WROOM-32UE-N4  
> 框架: PlatformIO + Arduino-ESP32 + FreeRTOS  
> 最后更新: 2026-06-23

---

## 1. 架构层级图

```
┌─────────────────────────────────────────────────────────────────┐
│  第 0 层: 应用编排层 (src/main.cpp)                               │
│  · FreeRTOS 双核任务调度                                          │
│  · 业务流程: executeFullOpen() / executeFullClose()               │
│  · 模块间协调                                                    │
├─────────────────────────────────────────────────────────────────┤
│  第 1 层: 业务模块层 (lib/xxx/)                                    │
│  ┌─────────────┬──────────────┬───────────┬───────────┬─────────┐│
│  │  DoorLock   │  DoorMotor   │  KeyInput │ Rs232Comm │Blinker  ││
│  │  .h/.cpp    │  .h/.cpp     │  .h/.cpp  │  .h/.cpp  │Cloud    ││
│  │  门锁控制   │  开门电机控制 │  按键处理  │ 协议通信  │  .h/.cpp││
│  └──────┬──────┴──────┬───────┴─────┬─────┴─────┬─────┴────┬────┘│
│         │             │             │           │          │     │
│         ▼             ▼             ▼           ▼          │     │
├─────────────────────────────────────────────────────────────────┤
│  第 2 层: BSP 硬件抽象层 (lib/bsp/)        ◄── 新增层            │
│  ┌──────────┬───────────┬──────────────┬───────────┬──────────┐ │
│  │ bsp_gpio │ bsp_limits│ bsp_motor_pwm│ bsp_uart  │bsp_system│ │
│  │ .h/.cpp  │ .h/.cpp   │ .h/.cpp      │ .h/.cpp   │ .h/.cpp  │ │
│  │ GPIO读写 │ 限位开关   │ 电机PWM+FG   │ 串口封装  │ 系统初始化│ │
│  └────┬─────┴─────┬─────┴──────┬───────┴─────┬─────┴────┬─────┘ │
│       │           │            │             │          │       │
│       ▼           ▼            ▼             ▼          │       │
├─────────────────────────────────────────────────────────────────┤
│  第 3 层: 框架层 (Arduino-ESP32 + FreeRTOS)                      │
│  pinMode · digitalWrite · Serial · WiFi · ledc · attachInterrupt │
├─────────────────────────────────────────────────────────────────┤
│  第 4 层: 物理硬件 (ESP32-WROOM-32UE-N4)                           │
│  GPIO · LEDC PWM · UART0/UART2 · WiFi · 中断                    │
└─────────────────────────────────────────────────────────────────┘
```

**核心原则**: 只有 BSP 层可以直接调用 Arduino API (`digitalWrite`, `pinMode`, `ledcWrite`...)，业务模块层不得触碰任何硬件寄存器或 Arduino 底层函数。

---

## 2. BSP 层模块详解

### 2.1 文件清单

| 文件 | 职责 |
|------|------|
| `lib/bsp/bsp_gpio.h/cpp` | GPIO 统一读写封装 (pinMode, digitalWrite, digitalRead) |
| `lib/bsp/bsp_limits.h/cpp` | 4 个限位开关 + 测试模式虚拟限位注入 |
| `lib/bsp/bsp_motor_pwm.h/cpp` | 开门电机 PWM (LEDC) + 方向 + FG 转速反馈中断 |
| `lib/bsp/bsp_uart.h/cpp` | 调试串口 (UART0) + RS232 通信串口 (UART2) |
| `lib/bsp/bsp_system.h/cpp` | 系统初始化统一入口 + 横幅打印 |

### 2.2 接口速查

#### bsp_gpio (GPIO 统一封装)

```cpp
namespace bsp::gpio {
    void initOutput(uint8_t pin);        // 初始化输出引脚 (默认 LOW)
    void initInputPullup(uint8_t pin);   // 初始化输入引脚 (内部上拉)
    void writeHigh(uint8_t pin);         // 写 HIGH
    void writeLow(uint8_t pin);          // 写 LOW
    bool read(uint8_t pin);              // 读电平 (true=HIGH)
}
```

#### bsp_limits (限位开关)

```cpp
namespace bsp::limits {
    void init();                         // 初始化所有限位开关引脚
    bool isLockOpenTriggered();          // GPIO19 LOW → 门锁已开到位
    bool isLockCloseTriggered();         // GPIO18 LOW → 门锁已锁到位
    bool isDoorFullOpenTriggered();      // GPIO4  LOW → 开门到底
    bool isDoorClosedTriggered();        // GPIO16 LOW → 关门到位
}
// 测试模式下返回虚拟限位值，否则读取真实 GPIO
```

#### bsp_motor_pwm (电机 PWM + FG)

```cpp
namespace bsp::motor_pwm {
    void init();                         // 初始化方向引脚 + LEDC PWM + FG 中断
    void setDirection(bool forward);     // true=正转开门, false=反转关门
    void setPwm(uint8_t value);          // 设 PWM (0-255, 内置最小阈值保护)
    void stop();                         // 立即停止 PWM
    uint8_t getPwm();                    // 当前 PWM 值
    uint32_t getFgPulseCount();          // FG 脉冲总数 (真实 + 测试注入)
    void resetFgPulseCount();            // 清零脉冲计数器
    void injectFgPulses(uint32_t count); // 测试模式虚拟脉冲注入
}
```

#### bsp_uart (串口)

```cpp
namespace bsp::uart {
    void initDebug(unsigned long baud);                    // USB 调试串口
    void waitDebugReady(unsigned long timeoutMs);          // 等待就绪 (0=不等)
    void initRs232(unsigned long baud, int8_t rx, int8_t tx); // RS232
    int rs232Available();                                  // 可用字节数
    uint8_t rs232Read();                                   // 读一个字节
}
```

#### bsp_system (系统初始化)

```cpp
namespace bsp::system {
    void init();        // 打印横幅 + 初始化 bsp::limits + bsp::motor_pwm
    void printBanner(); // 仅打印横幅
}
```

---

## 3. GPIO 引脚 → 对应文件映射表

### 3.1 门锁电机 (L9110S 驱动)

| 引脚 | 宏名称 | 方向 | 功能 | BSP 层操作文件 | 业务使用文件 |
|------|--------|------|------|---------------|-------------|
| GPIO5 | `PIN_LOCK_OPEN_MOTOR` | OUTPUT | 开锁电机 (HIGH=开锁) | `bsp_gpio.cpp` | `door_lock.cpp` |
| GPIO17 | `PIN_LOCK_RESET_MOTOR` | OUTPUT | 复位电机 (HIGH=复位) | `bsp_gpio.cpp` | `door_lock.cpp` |
| GPIO19 | `PIN_LOCK_OPEN_LIMIT` | INPUT_PULLUP | 开锁限位 (LOW=已开) | `bsp_limits.cpp` | `door_lock.cpp` |
| GPIO18 | `PIN_LOCK_CLOSE_LIMIT` | INPUT_PULLUP | 复位限位 (LOW=已锁) | `bsp_limits.cpp` | `door_lock.cpp` |

### 3.2 开门电机 (五线无刷)

| 引脚 | 宏名称 | 方向 | 功能 | BSP 层操作文件 | 业务使用文件 |
|------|--------|------|------|---------------|-------------|
| GPIO2 | `PIN_MOTOR_DIR` | OUTPUT | 方向 (HIGH=开门, LOW=关门) | `bsp_motor_pwm.cpp` | `door_motor.cpp` |
| GPIO15 | `PIN_MOTOR_PWM` | LEDC Ch0 | PWM 调速 | `bsp_motor_pwm.cpp` | `door_motor.cpp` |
| GPIO13 | `PIN_MOTOR_SPEED_FB` | INPUT_PULLUP | FG 转速反馈 (FALLING 中断) | `bsp_motor_pwm.cpp` | `door_motor.cpp` |
| GPIO4 | `PIN_DOOR_FULL_OPEN` | INPUT_PULLUP | 开门到位限位 (LOW=到底) | `bsp_limits.cpp` | `door_motor.cpp` |
| GPIO16 | `PIN_DOOR_CLOSED` | INPUT_PULLUP | 关门到位限位 (LOW=已关) | `bsp_limits.cpp` | `door_motor.cpp` |

### 3.3 通信与按键

| 引脚 | 宏名称 | 方向 | 功能 | BSP 层操作文件 | 业务使用文件 |
|------|--------|------|------|---------------|-------------|
| GPIO14 | `PIN_RS232_RX` | UART2 RX | RS232 接收 (接 MAX232 R1OUT) | `bsp_uart.cpp` | `rs232_comm.cpp` |
| GPIO27 | `PIN_RS232_TX` | UART2 TX | RS232 发送 (接 MAX232 T1IN) | `bsp_uart.cpp` | `rs232_comm.cpp` |
| GPIO33 | `PIN_KEY_OPEN` | INPUT_PULLUP | 开门按键 | `bsp_gpio.cpp` | `key_input.cpp` |
| GPIO25 | `PIN_KEY_STOP` | INPUT_PULLUP | 停止按键 | `bsp_gpio.cpp` | `key_input.cpp` |
| GPIO26 | `PIN_KEY_CLOSE` | INPUT_PULLUP | 关门按键 | `bsp_gpio.cpp` | `key_input.cpp` |

---

## 4. 功能修改指南

### 4.1 引脚变更 (硬件改版换 GPIO)

**只需修改一个文件**: `include/app_config.h`

例如把开门电机的方向引脚从 GPIO2 改为 GPIO21:

```diff
- #define PIN_MOTOR_DIR   2
+ #define PIN_MOTOR_DIR   21
```

所有 BSP 层代码通过宏引用引脚，无需修改任何 `.cpp` 文件。

### 4.2 时序参数调整

**修改文件**: `include/app_config.h`

| 参数 | 含义 | 默认值 |
|------|------|--------|
| `LOCK_RESET_DELAY` | 开锁后等待复位延时 | 3000 ms |
| `PWM_RAMP_INTERVAL` | PWM 渐变步进间隔 | 50 ms |
| `PWM_RAMP_STEP` | PWM 每步增量 | 5 |
| `KEY_DEBOUNCE_MS` | 按键消抖时间 | 50 ms |
| `KEY_LONG_PRESS_MS` | 长按判定时间 | 2000 ms |
| `LOCKOUT_TIMEOUT` | 反锁超时时间 | 30000 ms |
| `MOTOR_STALL_TIMEOUT` | 堵转检测超时 | 3000 ms |
| `MOTOR_MIN_PWM` | 电机启动最小 PWM | 30 |
| `MOTOR_SLOW_PWM` | 慢速阶段 PWM | 80 |
| `MOTOR_FAST_PWM` | 快速阶段 PWM | 200 |
| `DOOR_OPEN_TIMEOUT` | 开门总超时 | 15000 ms |
| `DOOR_CLOSE_TIMEOUT` | 关门总超时 | 15000 ms |

### 4.3 门锁控制逻辑修改

**修改文件**: `lib/door_lock/door_lock.cpp` + `door_lock.h`

涉及：开锁/复位流程、互锁逻辑、状态机逻辑。

### 4.4 开门电机控制逻辑修改

**修改文件**: `lib/door_motor/door_motor.cpp` + `door_motor.h`

涉及：开门/关门流程、PWM 渐变策略、堵转检测算法、状态机。

### 4.5 按键行为修改

**修改文件**: `lib/key_input/key_input.cpp`

涉及：按键事件处理 (`onOpenPress`, `onClosePress`, `onStopShortPress`, `onStopLongPress`)。

### 4.6 RS232 通信协议修改

**修改文件**: `lib/rs232_comm/rs232_comm.cpp`

涉及：帧格式、校验算法、新指令添加。

### 4.7 云平台配置修改 (WiFi / Blinker)

**修改文件**: `include/app_config.h`

```cpp
#define WIFI_SSID        "Your-WiFi-Name"
#define WIFI_PASSWORD    "Your-WiFi-Password"
#define BLINKER_AUTH_KEY "Your-Blinker-Key"
```

云平台业务逻辑在 `lib/blinker_cloud/blinker_cloud.cpp`。

### 4.8 测试模式修改

**修改文件**: `lib/test_mode/test_mode.cpp`

涉及：串口命令解析、虚拟限位/FG 脉冲生成。

### 4.9 更换 MCU (移植到 STM32 等)

**只需重写 BSP 层** (`lib/bsp/`):
- `bsp_gpio.cpp` → 替换为 HAL GPIO 函数
- `bsp_limits.cpp` → 替换 GPIO 读取
- `bsp_motor_pwm.cpp` → 替换为 HAL TIM PWM + 中断
- `bsp_uart.cpp` → 替换为 HAL UART
- `bsp_system.cpp` → 替换系统初始化

上层业务模块 (`door_lock`, `door_motor`, `key_input`, `rs232_comm`, `blinker_cloud`) **一行不改**。

---

## 5. BSP 层与非 BSP 层调用规则

```
                    ┌──────────────────────┐
                    │  main.cpp            │
                    │  (应用编排)           │
                    └──────────┬───────────┘
                               │ 调用业务模块
                               ▼
        ┌──────────────────────────────────────────┐
        │  业务模块层                               │
        │  door_lock.cpp  door_motor.cpp            │
        │  key_input.cpp  rs232_comm.cpp            │
        │  blinker_cloud.cpp  test_mode.cpp         │
        └───────┬────────────────┬─────────────────┘
                │                │
        ┌───────▼────┐   ┌───────▼──────┐
        │  ✅ 允许    │   │  ❌ 禁止      │
        │  调用 BSP   │   │  直接调用     │
        │  API        │   │  Arduino API  │
        └───────┬────┘   └───────────────┘
                │
        ┌───────▼──────────────────────────────┐
        │  BSP 层 (lib/bsp/)                    │
        │  bsp_gpio  bsp_limits  bsp_motor_pwm │
        │  bsp_uart  bsp_system                 │
        │                                       │
        │  ✅ 唯一允许调用 Arduino API 的层      │
        └───────────────┬──────────────────────┘
                        │
                        ▼
        ┌───────────────────────────────────────┐
        │  Arduino-ESP32 框架层                  │
        └───────────────────────────────────────┘
```

---

## 6. 初始化调用链

```
setup()
  │
  ├─ bsp::uart::initDebug(115200)         ─── UART0 调试串口
  ├─ bsp::uart::waitDebugReady(3000)      ─── 等待就绪
  │
  ├─ bsp::system::init()                  ─── 系统统一初始化
  │   ├─ printBanner()                    ─── 打印横幅
  │   ├─ bsp::limits::init()              ─── 4 个限位开关引脚
  │   └─ bsp::motor_pwm::init()           ─── 方向引脚 + LEDC PWM + FG 中断
  │
  ├─ g_doorLock.init()                    ─── 门锁电机引脚 + 初始状态
  ├─ g_doorMotor.init()                   ─── 电机业务状态初始化
  │
  ├─ g_testMode.init()                    ─── 测试模式虚拟变量
  │
  ├─ g_rs232.init()     → bsp::uart::initRs232()  ─── UART2
  ├─ g_keyInput.init()  → bsp::gpio::initInputPullup() ─── 3 个按键引脚
  ├─ g_blinker.init()                     ─── WiFi + Blinker
  │
  └─ xTaskCreatePinnedToCore() × 2        ─── FreeRTOS 任务
```

---

## 7. 目录结构

```
Access-control-system-ESP32-Arduino/
├── include/
│   └── app_config.h              # 全局配置 (引脚/超时/协议/网络)
├── src/
│   └── main.cpp                  # 应用入口 + FreeRTOS 任务
├── lib/
│   ├── bsp/                      # ★ BSP 硬件抽象层 (新增)
│   │   ├── bsp_gpio.h/.cpp       #    GPIO 封装
│   │   ├── bsp_limits.h/.cpp     #    限位开关 + 测试模式注入
│   │   ├── bsp_motor_pwm.h/.cpp  #    电机 PWM + FG 中断
│   │   ├── bsp_uart.h/.cpp       #    串口 UART 封装
│   │   └── bsp_system.h/.cpp     #    系统初始化入口
│   ├── door_lock/                # 门锁控制模块
│   │   ├── door_lock.h
│   │   └── door_lock.cpp
│   ├── door_motor/               # 开门电机控制模块
│   │   ├── door_motor.h
│   │   └── door_motor.cpp
│   ├── key_input/                # 按键输入模块
│   │   ├── key_input.h
│   │   └── key_input.cpp
│   ├── rs232_comm/               # RS232 通信模块
│   │   ├── rs232_comm.h
│   │   └── rs232_comm.cpp
│   ├── blinker_cloud/            # 点灯科技云接入模块
│   │   ├── blinker_cloud.h
│   │   └── blinker_cloud.cpp
│   └── test_mode/                # 测试模式模块
│       ├── test_mode.h
│       └── test_mode.cpp
├── platformio.ini
├── PROJECT_REQUIREMENTS.md
├── README.md
└── ARCHITECTURE.md               # ★ 本文档
```
