/**
 * @file bsp_gpio.h
 * @brief BSP 层 - GPIO 统一封装
 *
 * 所有 GPIO 操作必须通过此模块，禁止业务模块直接调用
 * Arduino 的 pinMode/digitalWrite/digitalRead 等函数。
 *
 * 职责: 封装 GPIO 的输入输出、电平读取
 * 使用: 业务模块通过 bsp::gpio::xxx() 间接操作 GPIO
 *
 * 引脚变更加载:
 *   所有 GPIO 引脚宏定义在 include/app_config.h 中统一管理。
 *   若硬件改版换引脚，只需修改 app_config.h 的 PIN_xxx 宏，
 *   无需修改任何业务代码。
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <Arduino.h>

namespace bsp {
namespace gpio {

/**
 * @brief 初始化输出引脚 (默认 LOW 电平)
 * @param pin GPIO 引脚号
 */
void initOutput(uint8_t pin);

/**
 * @brief 初始化输入引脚 (带内部上拉)
 * @param pin GPIO 引脚号
 */
void initInputPullup(uint8_t pin);

/**
 * @brief 写高电平
 * @param pin GPIO 引脚号 (必须已初始化为 OUTPUT)
 */
void writeHigh(uint8_t pin);

/**
 * @brief 写低电平
 * @param pin GPIO 引脚号 (必须已初始化为 OUTPUT)
 */
void writeLow(uint8_t pin);

/**
 * @brief 读取引脚电平
 * @param pin GPIO 引脚号
 * @return true=HIGH, false=LOW
 */
bool read(uint8_t pin);

} // namespace gpio
} // namespace bsp

#endif // BSP_GPIO_H
