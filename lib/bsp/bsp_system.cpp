/**
 * @file bsp_system.cpp
 * @brief BSP 层 - 系统初始化实现
 */

#include "bsp_system.h"
#include "bsp_limits.h"
#include "bsp_motor_pwm.h"
#include <Arduino.h>

namespace bsp {
namespace system {

void printBanner() {
    Serial.println(F("========================================"));
    Serial.println(F(" ESP32 Smart Door Controller"));
    Serial.println(F(" Hardware: ESP32-WROOM-32UE-N4"));
    Serial.println(F("========================================"));
}

void init() {
    printBanner();

    // 硬件资源初始化
    bsp::limits::init();
    bsp::motor_pwm::init();
}

} // namespace system
} // namespace bsp
