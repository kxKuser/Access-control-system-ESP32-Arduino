/**
 * @file bsp_uart.cpp
 * @brief BSP 层 - 串口 UART 实现
 *
 * 唯一允许调用 Serial.begin / Serial2.begin 的模块。
 */

#include "bsp_uart.h"

namespace bsp {
namespace uart {

// ============================================================
//  USB 调试串口
// ============================================================

void initDebug(unsigned long baud) {
    Serial.begin(baud);
}

void waitDebugReady(unsigned long timeoutMs) {
    if (timeoutMs == 0) return;
    unsigned long serialTimeout = millis() + timeoutMs;
    while (!Serial && millis() < serialTimeout) { ; }
}

// ============================================================
//  RS232 通信串口
// ============================================================

void initRs232(unsigned long baud, int8_t rxPin, int8_t txPin) {
    Serial2.begin(baud, SERIAL_8N1, rxPin, txPin);
}

int rs232Available() {
    return Serial2.available();
}

uint8_t rs232Read() {
    return Serial2.read();
}

} // namespace uart
} // namespace bsp
