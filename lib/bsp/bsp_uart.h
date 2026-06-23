/**
 * @file bsp_uart.h
 * @brief BSP 层 - 串口 UART 统一封装
 *
 * 封装两类串口:
 *   1. USB 调试串口 (Serial  / UART0)
 *   2. RS232 通信串口 (Serial2 / UART2, 经 MAX232 电平转换)
 *
 * 外部模块不应直接调用 Serial / Serial2 的 begin/available/read 方法。
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <Arduino.h>

namespace bsp {
namespace uart {

// ============================================================
//  USB 调试串口 (UART0)
// ============================================================

/**
 * @brief 初始化调试串口
 * @param baud 波特率 (默认 115200)
 */
void initDebug(unsigned long baud);

/**
 * @brief 等待调试串口就绪
 * @param timeoutMs 超时时间 (ms), 0 表示不等待
 */
void waitDebugReady(unsigned long timeoutMs);

// ============================================================
//  RS232 通信串口 (UART2, 接 MAX232)
// ============================================================

/**
 * @brief 初始化 RS232 通信串口
 * @param baud 波特率 (默认 9600)
 * @param rxPin RX 引脚
 * @param txPin TX 引脚
 */
void initRs232(unsigned long baud, int8_t rxPin, int8_t txPin);

/**
 * @brief RS232 接收缓冲区可用字节数
 */
int rs232Available();

/**
 * @brief 从 RS232 读取一个字节
 */
uint8_t rs232Read();

} // namespace uart
} // namespace bsp

#endif // BSP_UART_H
