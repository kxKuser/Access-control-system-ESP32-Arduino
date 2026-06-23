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
 * 软件分层架构:
 *   应用编排层 (本文件) → 业务模块层 (lib/door_lock, door_motor, ...)
 *     → BSP 硬件抽象层 (lib/bsp) → Arduino-ESP32 框架 → 物理硬件
 */

#include <Arduino.h>
#include "app_config.h"

// BSP 硬件抽象层
#include "bsp_system.h"
#include "bsp_uart.h"

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
    // USB 调试串口初始化 (通过 BSP 层)
    bsp::uart::initDebug(115200);
    bsp::uart::waitDebugReady(3000);

    // 系统横幅打印 + 硬件初始化 (GPIO/LEDC/限位开关/FG中断)
    bsp::system::init();

    // 初始化核心驱动模块 (业务状态)
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
