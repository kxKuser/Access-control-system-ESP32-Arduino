/**
 * @file main.cpp
 * @brief ESP32 智能门锁主控板 - 主程序入口
 *
 * 功能概述:
 *   1. 上电后自动连接WiFi, 接入点灯科技Blinker云
 *   2. 监听UART串口, 接收门外门禁板的开门指令
 *   3. 4个核心操作: 开锁、开门、关门、锁复位
 *   4. 3个按键: 开门(开锁+开门)、关门、停止(短按停电机/长按反锁)
 *   5. 支持Blinker APP远程开门、查看设备在线状态
 *   6. 所有逻辑使用非阻塞 millis() 实现, 禁止使用 delay()
 *   7. 基于 FreeRTOS 多核架构 (xTaskCreatePinnedToCore)
 *
 * 硬件平台: ESP32-WROOM-32UE-N4
 * 开发框架: PlatformIO + Arduino (arduino-esp32)
 *
 * ============================================================
 *  DoorMotor / DoorLock 模块使用示例:
 *
 *  // 1. 完整开门流程 (先开锁再开门)
 *  if (!g_doorLock.isUnlocked()) {
 *      g_doorLock.unlock();       // 触发开锁
 *  }
 *  // 等待开锁完成 (非阻塞)
 *  if (g_doorLock.isUnlocked() && !g_doorMotor.isRunning()) {
 *      g_doorMotor.open();        // 触发开门
 *  }
 *
 *  // 2. 完整关门流程 (先关门再复位)
 *  if (!g_doorMotor.isRunning()) {
 *      g_doorMotor.close();       // 触发关门
 *  }
 *  // 等待关门完成后复位
 *  if (g_doorMotor.isDoorClosed() && g_doorLock.isUnlocked()) {
 *      g_doorLock.relock();       // 触发复位(上锁)
 *  }
 *
 *  // 3. 紧急停止
 *  g_doorMotor.emergencyStop();   // 立即停止开门电机
 *  g_doorLock.emergencyStop();    // 立即停止门锁电机
 *
 *  // 4. 状态查询
 *  bool opening   = g_doorMotor.isDoorOpening();
 *  bool closing   = g_doorMotor.isDoorClosing();
 *  bool doorOpen  = g_doorMotor.isDoorOpen();
 *  bool doorClose = g_doorMotor.isDoorClosed();
 *  bool locked    = g_doorLock.isLocked();
 *  bool unlocked  = g_doorLock.isUnlocked();
 *  bool lockRun   = g_doorLock.isLockRunning();
 * ============================================================
 */

#include <Arduino.h>
#include "app_config.h"

// 核心驱动模块 (高内聚, 低耦合)
#include "door_lock.h"
#include "door_motor.h"

// 测试模式模块 (PC 串口工具联调)
#include "test_mode.h"

// 通信与控制模块 (依赖核心驱动)
#include "rs232_comm.h"
#include "key_input.h"
#include "blinker_cloud.h"

// ============================================================
//  FreeRTOS 任务句柄
// ============================================================
TaskHandle_t taskControlHandle = NULL;
TaskHandle_t taskComHandle     = NULL;

// ============================================================
//  前向声明: FreeRTOS 任务函数
// ============================================================
void taskControl(void* pvParameters);
void taskCom(void* pvParameters);

// ============================================================
//  辅助函数: 完整开门流程 (开锁 -> 开门)
// ============================================================
static void executeFullOpen() {
    // 步骤1: 如果未开锁, 先触发开锁
    if (!g_doorLock.isUnlocked()) {
        if (!g_doorLock.isLockRunning()) {
            g_doorLock.unlock();
            Serial.println(F("[CMD] Unlocking..."));
        }
        return;  // 等待开锁完成
    }

    // 步骤2: 已开锁, 触发开门电机
    if (!g_doorMotor.isRunning()) {
        g_doorMotor.open();
        Serial.println(F("[CMD] Opening door..."));
    }
}

// ============================================================
//  辅助函数: 完整关门流程 (关门 -> 复位)
// ============================================================
static void executeFullClose() {
    // 步骤1: 如果门未关闭, 先触发关门
    if (!g_doorMotor.isDoorClosed()) {
        if (!g_doorMotor.isRunning()) {
            g_doorMotor.close();
            Serial.println(F("[CMD] Closing door..."));
        }
        return;  // 等待关门完成
    }

    // 步骤2: 门已关闭且锁未复位, 触发复位
    if (g_doorLock.isUnlocked() && !g_doorLock.isLockRunning()) {
        g_doorLock.relock();
        Serial.println(F("[CMD] Relocking..."));
    }
}

// ============================================================
//  Arduino setup()
// ============================================================
void setup() {
    // USB 调试串口
    Serial.begin(115200);
    // 等待串口就绪 (最长 3 秒, 避免无 USB 时永久阻塞)
    unsigned long serialTimeout = millis() + 3000;
    while (!Serial && millis() < serialTimeout) { ; }
    Serial.println(F("========================================"));
    Serial.println(F(" ESP32 Smart Door Controller"));
    Serial.println(F(" Hardware: ESP32-WROOM-32UE-N4"));
    Serial.println(F("========================================"));

    // 初始化核心驱动模块
    g_doorLock.init();
    g_doorMotor.init();

    // 初始化测试模式 (PC 串口工具联调)
    g_testMode.init();

    // 初始化通信与控制模块
    g_rs232.init();
    g_keyInput.init();
    g_blinker.init(WIFI_SSID, WIFI_PASSWORD, BLINKER_AUTH_KEY);

    // 打印初始状态
    Serial.print(F("[SYS] Initial state: "));
    Serial.print(g_doorLock.isLocked() ? F("LOCKED") : F("UNLOCKED"));
    Serial.print(F(", "));
    Serial.println(g_doorMotor.isDoorClosed() ? F("CLOSED") : F("OPEN"));

    // 创建 FreeRTOS 任务
    xTaskCreatePinnedToCore(
        taskControl,
        "TaskControl",
        TASK_CONTROL_STACK,
        NULL,
        TASK_CONTROL_PRIORITY,
        &taskControlHandle,
        1   // Core 1 (控制循环)
    );

    xTaskCreatePinnedToCore(
        taskCom,
        "TaskCom",
        TASK_COM_STACK,
        NULL,
        TASK_COM_PRIORITY,
        &taskComHandle,
        0   // Core 0 (通信循环)
    );

    Serial.println(F("[SYS] Initialization complete"));
}

// ============================================================
//  Arduino loop() - 保持空闲, 实际工作由 FreeRTOS 任务承担
// ============================================================
void loop() {
    vTaskDelay(portMAX_DELAY);
}

// ============================================================
//  FreeRTOS 任务1: 主控制循环 (Core 1)
//  负责: 门锁状态机、开门电机状态机、按键输入
// ============================================================
void taskControl(void* pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(10); // 10ms 周期

    for (;;) {
        // 核心驱动轮询
        g_doorLock.loop();
        g_doorMotor.loop();

        // 按键输入处理
        g_keyInput.loop();

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ============================================================
//  FreeRTOS 任务2: 通信循环 (Core 0)
//  负责: RS232 串口接收、Blinker 云平台
// ============================================================
void taskCom(void* pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(20); // 20ms 周期

    for (;;) {
        // 测试模式轮询 (USB Serial 命令处理)
        g_testMode.loop(30000);  // 30秒无命令自动退出测试模式

        // RS232 串口接收处理
        g_rs232.loop();

        // 处理 RS232 开门指令
        if (g_rs232.hasOpenCommand()) {
            g_rs232.consumeOpenCommand();
            Serial.println(F("[SYS] RS232 open command received"));
            executeFullOpen();
        }

        // Blinker 云平台轮询
        g_blinker.loop();

        // 处理 Blinker 远程命令
        RemoteCmd cmd = g_blinker.getRemoteCmd();
        if (cmd != RemoteCmd::NONE) {
            g_blinker.clearRemoteCmd();
            switch (cmd) {
                case RemoteCmd::OPEN_DOOR:
                    Serial.println(F("[SYS] Blinker remote OPEN"));
                    executeFullOpen();
                    break;
                case RemoteCmd::CLOSE_DOOR:
                    Serial.println(F("[SYS] Blinker remote CLOSE"));
                    executeFullClose();
                    break;
                case RemoteCmd::STOP:
                    Serial.println(F("[SYS] Blinker remote STOP"));
                    g_doorMotor.emergencyStop();
                    g_doorLock.emergencyStop();
                    break;
                default:
                    break;
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
