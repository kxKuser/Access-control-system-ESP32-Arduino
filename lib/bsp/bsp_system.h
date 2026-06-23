/**
 * @file bsp_system.h
 * @brief BSP 层 - 系统初始化统一入口
 *
 * 职责: 上电后打印系统横幅 + 一次性调用所有硬件模块的 init()。
 * 所有硬件初始化都应在此触发，确保初始化顺序可控。
 */

#ifndef BSP_SYSTEM_H
#define BSP_SYSTEM_H

namespace bsp {
namespace system {

/**
 * @brief 系统统一初始化入口
 *
 * 执行顺序:
 *   1. 打印系统横幅 (芯片型号/固件名)
 *   2. 初始化 GPIO 限位开关 (bsp::limits::init)
 *   3. 初始化电机 PWM 硬件 (bsp::motor_pwm::init)
 */
void init();

/**
 * @brief 仅打印系统横幅信息 (不初始化任何硬件)
 */
void printBanner();

} // namespace system
} // namespace bsp

#endif // BSP_SYSTEM_H
