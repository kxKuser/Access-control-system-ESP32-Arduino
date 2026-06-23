/**
 * @file bsp_motor_pwm.cpp
 * @brief BSP 层 - 开门电机 PWM + FG 转速反馈实现
 *
 * 唯一允许调用 ledcSetup/ledcAttachPin/ledcWrite/attachInterrupt 的模块。
 */

#include "bsp_motor_pwm.h"
#include "bsp_gpio.h"
#include "app_config.h"

// ============================================================
//  模块级静态变量
// ============================================================

// FG 真实脉冲计数器 (ISR 中递增，必须 volatile)
static volatile uint32_t s_fgPulseCount = 0;

// FG 虚拟脉冲计数器 (测试模式注入)
static uint32_t s_fgInjectedCount = 0;

// 当前 PWM 值
static uint8_t s_currentPwm = 0;

// ============================================================
//  FG 中断服务程序 (IRAM 驻留)
// ============================================================

static void IRAM_ATTR bsp_fg_isr_handler() {
    s_fgPulseCount++;
}

// ============================================================
//  公开接口实现
// ============================================================

namespace bsp {
namespace motor_pwm {

void init() {
    // 方向引脚
    bsp::gpio::initOutput(PIN_MOTOR_DIR);

    // FG 转速反馈引脚 + 中断
    bsp::gpio::initInputPullup(PIN_MOTOR_SPEED_FB);
    attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_SPEED_FB),
                    bsp_fg_isr_handler, FALLING);

    // LEDC PWM 初始化
    ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_PWM, MOTOR_PWM_CHANNEL);
    ledcWrite(MOTOR_PWM_CHANNEL, 0);

    // 清零计数器
    s_fgPulseCount    = 0;
    s_fgInjectedCount = 0;
    s_currentPwm      = 0;
}

void setDirection(bool forward) {
    if (forward) {
        bsp::gpio::writeHigh(PIN_MOTOR_DIR);
    } else {
        bsp::gpio::writeLow(PIN_MOTOR_DIR);
    }
}

void setPwm(uint8_t value) {
    // 硬件保护: 非零但低于启动阈值，钳位到最小启动值
    if (value > 0 && value < MOTOR_MIN_PWM) {
        value = MOTOR_MIN_PWM;
    }
    ledcWrite(MOTOR_PWM_CHANNEL, value);
    s_currentPwm = value;
}

void stop() {
    ledcWrite(MOTOR_PWM_CHANNEL, 0);
    s_currentPwm = 0;
}

uint8_t getPwm() {
    return s_currentPwm;
}

// ============================================================
//  FG 转速反馈
// ============================================================

uint32_t getFgPulseCount() {
    return s_fgPulseCount + s_fgInjectedCount;
}

void resetFgPulseCount() {
    s_fgPulseCount    = 0;
    s_fgInjectedCount = 0;
}

void injectFgPulses(uint32_t count) {
    s_fgInjectedCount += count;
}

} // namespace motor_pwm
} // namespace bsp
