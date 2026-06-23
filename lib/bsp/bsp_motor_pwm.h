/**
 * @file bsp_motor_pwm.h
 * @brief BSP 层 - 开门电机 PWM + FG 转速反馈封装
 *
 * 封装五线无刷电机的硬件控制:
 *   - 方向引脚 (PIN_MOTOR_DIR)
 *   - LEDC PWM 调速 (PIN_MOTOR_PWM)
 *   - FG 转速反馈中断 (PIN_MOTOR_SPEED_FB)
 *
 * 测试模式集成:
 *   - injectFgPulses() 允许测试模式注入虚拟 FG 脉冲
 *   - getFgPulseCount() 返回真实 + 虚拟脉冲总和
 */

#ifndef BSP_MOTOR_PWM_H
#define BSP_MOTOR_PWM_H

#include <cstdint>

namespace bsp {
namespace motor_pwm {

/**
 * @brief 初始化电机硬件:
 *        - 方向引脚 → OUTPUT
 *        - PWM 引脚 → LEDC 通道
 *        - FG 转速反馈引脚 → INPUT_PULLUP + 中断
 */
void init();

/**
 * @brief 设置电机方向
 * @param forward true=正转开门(HIGH), false=反转关门(LOW)
 */
void setDirection(bool forward);

/**
 * @brief 设置 PWM 输出值
 *        内置保护: 低于 MOTOR_MIN_PWM 的非零值自动钳位
 * @param value 0-255
 */
void setPwm(uint8_t value);

/**
 * @brief 停止 PWM 输出 (立即切断)
 */
void stop();

/**
 * @brief 获取当前 PWM 值
 */
uint8_t getPwm();

// ============================================================
//  FG 转速反馈
// ============================================================

/**
 * @brief 获取 FG 脉冲累计总数 (真实 + 虚拟)
 *        由中断 ISR 递增，isr-safe volatile 变量
 */
uint32_t getFgPulseCount();

/**
 * @brief 清零脉冲计数器 (开门/关门启动时调用)
 */
void resetFgPulseCount();

/**
 * @brief 注入虚拟 FG 脉冲 (测试模式专用)
 *        虚拟脉冲累积在独立计数器，getFgPulseCount() 会合并
 */
void injectFgPulses(uint32_t count);

} // namespace motor_pwm
} // namespace bsp

#endif // BSP_MOTOR_PWM_H
