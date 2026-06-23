/**
 * @file app_config.h
 * @brief ESP32 智能门锁主控板 - 全局配置
 *
 * 硬件平台: ESP32-WROOM-32UE-N4
 * 开发框架: PlatformIO + Arduino + FreeRTOS
 *
 * 包含: 引脚定义、常量、超时时间、通信协议、WiFi/Blinker 配置
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <Arduino.h>

// ============================================================
//  门锁电机引脚
// ============================================================
#define PIN_LOCK_OPEN_LIMIT     19  // 开锁限位开关 (INPUT, LOW=门锁已开)
#define PIN_LOCK_CLOSE_LIMIT    18  // 复位限位开关 (INPUT, LOW=门已锁)
#define PIN_LOCK_OPEN_MOTOR      5  // 门锁电机开锁输出 (OUTPUT, HIGH=开锁)
#define PIN_LOCK_RESET_MOTOR    17  // 门锁电机复位输出 (OUTPUT, HIGH=复位)

// ============================================================
//  门限位开关引脚
// ============================================================
#define PIN_DOOR_CLOSED         16  // 门限位-锁门状态 (INPUT, LOW=锁门)
#define PIN_DOOR_FULL_OPEN       4  // 门限位-开到最大 (INPUT, LOW=开到最大)

// ============================================================
//  开门电机引脚
// ============================================================
#define PIN_MOTOR_DIR            2  // 电机方向 (HIGH=正转开门, LOW=反转关门)
#define PIN_MOTOR_PWM           15  // 电机 PWM 输出 (LEDC)
#define PIN_MOTOR_SPEED_FB      13  // 电机转速反馈 (脉冲输入)

// ============================================================
//  RS232 通信引脚 (UART2, 经 MAX232 电平转换)
// ============================================================
#define PIN_RS232_RX            14  // UART2 RX, 接 MAX232 R1OUT
#define PIN_RS232_TX            27  // UART2 TX, 接 MAX232 T1IN
#define RS232_BAUDRATE          9600

// ============================================================
//  按键引脚 (INPUT_PULLUP, LOW 有效)
// ============================================================
#define PIN_KEY_OPEN            33  // 开门按键
#define PIN_KEY_STOP            25  // 停止按键 (短按=停电机, 长按=反锁)
#define PIN_KEY_CLOSE           26  // 关门按键

// ============================================================
//  PWM 配置 (LEDC)
// ============================================================
#define MOTOR_PWM_FREQ          5000    // PWM 频率 (Hz)
#define MOTOR_PWM_RESOLUTION    8       // PWM 分辨率 (bit), 0-255
#define MOTOR_PWM_CHANNEL       0       // LEDC 通道
#define MOTOR_PWM_MAX           255     // 最大 PWM 值
#define MOTOR_PWM_MIN           0       // 最小 PWM 值

// ============================================================
//  时序参数 (毫秒)
// ============================================================
#define LOCK_RESET_DELAY        3000    // 开锁后等待复位延时
#define PWM_RAMP_INTERVAL       50      // PWM 渐变步进间隔 (ms)
#define PWM_RAMP_STEP           5       // PWM 每步增量
#define KEY_DEBOUNCE_MS         50      // 按键消抖时间 (ms)
#define KEY_LONG_PRESS_MS       2000    // 长按判定时间 (ms)
#define LOCKOUT_TIMEOUT         30000   // 反锁超时时间 (ms)

// ============================================================
//  电机保护参数
// ============================================================
#define MOTOR_STALL_TIMEOUT     3000    // 堵转检测超时 (ms), FG 无脉冲超过此时间则停机
#define MOTOR_MIN_PWM           30      // 电机启动最小 PWM (低于此值电机不转)
#define MOTOR_SLOW_PWM          80      // 慢速阶段 PWM
#define MOTOR_FAST_PWM          200     // 快速阶段 PWM
#define DOOR_OPEN_TIMEOUT       15000   // 开门总超时保护 (ms)
#define DOOR_CLOSE_TIMEOUT      15000   // 关门总超时保护 (ms)

// ============================================================
//  通信协议 (门外门禁板 -> 主控板)
//  帧格式: [0xAA][0x55][指令码][校验和]
//  校验和 = 前3字节异或
// ============================================================
#define PROTOCOL_HEADER_1       0xAA
#define PROTOCOL_HEADER_2       0x55
#define CMD_OPEN_DOOR           0x01
#define PROTOCOL_FRAME_LEN      4

// ============================================================
//
//  ╔══════════════════════════════════════════════════════╗
//  ║     WiFi / Blinker 云平台配置 (★★★★★ 必填 ★★★★★)  ║
//  ╚══════════════════════════════════════════════════════╝
//
//  ⚠️ 请务必修改以下 3 项为你的实际值, 否则无法连接!
//
//  ───────────────────────────────────────────────────────
//  第 1 步: WiFi 配置
//    填写你要连接的 2.4GHz WiFi 名称和密码
//    (ESP32 仅支持 2.4GHz, 不支持 5GHz)
//  ───────────────────────────────────────────────────────
//
#define WIFI_SSID               "Redmi K70 Pro"
#define WIFI_PASSWORD           "bf2T01QOffa"
//
//  ───────────────────────────────────────────────────────
//  第 2 步: Blinker 密钥 (点灯科技 APP)
//    1. 手机下载 "点灯科技" APP
//    2. 注册/登录 → 添加设备 → 独立设备 → WiFi接入
//    3. 复制生成的 Secret Key 填入下方
//  ───────────────────────────────────────────────────────
//
#define BLINKER_AUTH_KEY        "efb3bea62d86"
//
//  ╔══════════════════════════════════════════════════════╗
//  ║     Blinker APP 组件数据键名 (无需修改)              ║
//  ╚══════════════════════════════════════════════════════╝
//
//  APP 界面需要添加以下组件:
//  ┌─────────┬──────────┬─────────────────────────────┐
//  │ 组件类型 │  数据键名  │          说明               │
//  ├─────────┼──────────┼─────────────────────────────┤
//  │  按钮   │ btn-open │ 远程开门 (点按触发)          │
//  │  按钮   │ btn-close│ 远程关门 (点按触发)          │
//  │  按钮   │ btn-stop │ 紧急停止 (点按触发)          │
//  │  文本   │ v-online │ 设备在线状态 (online/offline) │
//  │  文本   │ v-door   │ 门状态 (open/closed/opening) │
//  │  文本   │ v-lock   │ 锁状态 (locked/unlocked)     │
//  │  数值   │ v-pwm    │ 电机 PWM 值 (0-255)          │
//  │  文本   │ v-fault  │ 故障状态 (ok/stall)          │
//  │  文本   │ v-lockout│ 反锁状态 (normal/lockout)    │
//  │  文本   │ v-summary│ 综合状态摘要                  │
//  └─────────┴──────────┴─────────────────────────────┘
//
//  ╔══════════════════════════════════════════════════════╗
//  ║  配置示例 (仅供参考, 不要取消注释):                  ║
//  ║  #define WIFI_SSID         "TP-LINK_Home"           ║
//  ║  #define WIFI_PASSWORD     "abc12345678"            ║
//  ║  #define BLINKER_AUTH_KEY  "a1b2c3d4e5f6"          ║
//  ╚══════════════════════════════════════════════════════╝
//
// ============================================================

// ============================================================
//  门 / 锁 状态枚举
// ============================================================

enum class DoorState : uint8_t {
    CLOSED,     // 门已关闭 (锁门限位触发)
    OPENING,    // 开门中
    OPEN,       // 门已开到最大
    CLOSING     // 关门中
};

enum class LockState : uint8_t {
    LOCKED,     // 已锁 (复位限位触发)
    UNLOCKING,  // 开锁中
    UNLOCKED,   // 已开 (开锁限位触发)
    RELOCKING   // 复位中
};

enum class MotorDir : uint8_t {
    STOP,
    FORWARD,    // 正转开门
    REVERSE     // 反转关门
};

enum class SystemMode : uint8_t {
    NORMAL,     // 正常模式
    LOCKOUT     // 反锁模式 (仅本地按键/远程可操作)
};

// ============================================================
//  FreeRTOS 任务配置
// ============================================================
#define TASK_CONTROL_STACK      4096
#define TASK_CONTROL_PRIORITY   5
#define TASK_COM_STACK          10240   // Blinker+WiFi 联网需要较大栈空间
#define TASK_COM_PRIORITY       4

#endif // APP_CONFIG_H
