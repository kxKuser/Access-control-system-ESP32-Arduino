/**
 * @file door_motor.cpp
 * @brief 开门电机控制模块 - 实现
 *
 * 核心驱动逻辑:
 *   - 状态机驱动开门/关门流程
 *   - PWM 渐变 (软启动/软停止)
 *   - FG 堵转检测 (FG 无脉冲超过阈值则紧急停机)
 *   - 限位开关到位停机
 *   - 超时保护
 *
 * 硬件依赖: 全部通过 BSP 层访问
 *   - PWM/方向/FG → bsp::motor_pwm
 *   - 限位开关     → bsp::limits
 */

#include "door_motor.h"
#include "bsp_motor_pwm.h"
#include "bsp_limits.h"

// ==================== 全局单例 ====================
DoorMotor g_doorMotor;

// ==================== 生命周期 ====================
void DoorMotor::init() {
    // 电机硬件统一由 BSP 初始化 (方向引脚 + PWM + FG 中断)
    // 已在 bsp::system::init() -> bsp::motor_pwm::init() 中完成

    // 初始化业务状态
    state = State::IDLE;
    targetPwm = 0;
    lastRampTime = 0;
    actionStartTime = 0;
    stalled = false;
    prevFgPulseCount = 0;
    lastFgPulseTime = 0;

    Serial.println(F("[MOTOR] Door motor initialized"));
}

/**
 * @brief 主循环轮询 (10ms 周期调用)
 *        职责: 状态机推进 + PWM 渐变 + 堵转检测 + 限位检查 + 超时保护
 */
void DoorMotor::loop() {
    if (state == State::IDLE) {
        return;
    }

    unsigned long now = millis();

    // 1. 堵转检测
    checkStall();
    if (stalled) {
        emergencyStop();
        Serial.println(F("[MOTOR] EMERGENCY STOP: Stall detected!"));
        return;
    }

    // 2. 超时保护
    if (actionStartTime > 0) {
        unsigned long elapsed = now - actionStartTime;
        if ((state == State::OPENING_RAMP || state == State::OPENING) && elapsed > DOOR_OPEN_TIMEOUT) {
            emergencyStop();
            Serial.println(F("[MOTOR] EMERGENCY STOP: Open timeout!"));
            return;
        }
        if ((state == State::CLOSING_SLOW || state == State::CLOSING_FAST) && elapsed > DOOR_CLOSE_TIMEOUT) {
            emergencyStop();
            Serial.println(F("[MOTOR] EMERGENCY STOP: Close timeout!"));
            return;
        }
    }

    // 3. 限位开关检查
    if (state == State::OPENING_RAMP || state == State::OPENING) {
        if (isFullOpenLimit()) {
            emergencyStop();
            Serial.println(F("[MOTOR] Full open limit reached"));
            return;
        }
    }
    if (state == State::CLOSING_SLOW || state == State::CLOSING_FAST) {
        if (isClosedLimit()) {
            emergencyStop();
            Serial.println(F("[MOTOR] Closed limit reached"));
            return;
        }
    }

    // 4. PWM 渐变控制
    rampStep();

    // 5. 关门慢速->快速阶段切换
    if (state == State::CLOSING_SLOW && bsp::motor_pwm::getPwm() >= MOTOR_SLOW_PWM) {
        targetPwm = MOTOR_FAST_PWM;
        state = State::CLOSING_FAST;
    }
}

// ==================== 运动控制接口 ====================

void DoorMotor::open() {
    if (state != State::IDLE) {
        Serial.println(F("[MOTOR] Busy, cannot open"));
        return;
    }

    // 重置状态
    stalled = false;
    bsp::motor_pwm::resetFgPulseCount();
    prevFgPulseCount = 0;
    lastFgPulseTime = millis();
    actionStartTime = millis();

    // 设置方向 = 正转开门
    setDirection(true);
    targetPwm = MOTOR_FAST_PWM;
    state = State::OPENING_RAMP;
    lastRampTime = millis();

    Serial.println(F("[MOTOR] Opening..."));
}

void DoorMotor::close() {
    if (state != State::IDLE) {
        Serial.println(F("[MOTOR] Busy, cannot close"));
        return;
    }

    // 重置状态
    stalled = false;
    bsp::motor_pwm::resetFgPulseCount();
    prevFgPulseCount = 0;
    lastFgPulseTime = millis();
    actionStartTime = millis();

    // 设置方向 = 反转关门
    setDirection(false);
    targetPwm = MOTOR_SLOW_PWM;  // 先慢速启动
    state = State::CLOSING_SLOW;
    lastRampTime = millis();

    Serial.println(F("[MOTOR] Closing..."));
}

void DoorMotor::emergencyStop() {
    // 立即切断 PWM
    bsp::motor_pwm::stop();
    targetPwm = 0;
    state = State::IDLE;
    actionStartTime = 0;
    stalled = false;

    Serial.println(F("[MOTOR] Emergency stop executed"));
}

// ==================== 状态查询接口 ====================

bool DoorMotor::isDoorOpening() const {
    return state == State::OPENING_RAMP || state == State::OPENING;
}

bool DoorMotor::isDoorClosing() const {
    return state == State::CLOSING_SLOW || state == State::CLOSING_FAST;
}

bool DoorMotor::isDoorOpen() const {
    return isFullOpenLimit();
}

bool DoorMotor::isDoorClosed() const {
    return isClosedLimit();
}

bool DoorMotor::isRunning() const {
    return state != State::IDLE;
}

bool DoorMotor::isStalled() const {
    return stalled;
}

uint8_t DoorMotor::getCurrentPwm() const {
    return bsp::motor_pwm::getPwm();
}

// ==================== 内部方法 ====================

void DoorMotor::setPwm(uint8_t value) {
    bsp::motor_pwm::setPwm(value);
}

void DoorMotor::rampStep() {
    unsigned long now = millis();
    if ((now - lastRampTime) < PWM_RAMP_INTERVAL) {
        return;  // 未到步进时间
    }
    lastRampTime = now;

    uint8_t currentPwm = bsp::motor_pwm::getPwm();

    if (currentPwm < targetPwm) {
        // 上升
        uint16_t newVal = (uint16_t)currentPwm + PWM_RAMP_STEP;
        if (newVal > targetPwm) newVal = targetPwm;
        if (newVal > 255) newVal = 255;
        bsp::motor_pwm::setPwm((uint8_t)newVal);
    } else if (currentPwm > targetPwm) {
        // 下降
        int16_t newVal = (int16_t)currentPwm - PWM_RAMP_STEP;
        if (newVal < targetPwm) newVal = targetPwm;
        if (newVal < 0) newVal = 0;
        bsp::motor_pwm::setPwm((uint8_t)newVal);
    }
}

void DoorMotor::setDirection(bool forward) {
    bsp::motor_pwm::setDirection(forward);
}

// ==================== FG 堵转检测 ====================

void DoorMotor::checkStall() {
    unsigned long now = millis();

    // 仅在电机运行时检测
    if (state == State::IDLE) {
        return;
    }

    // 从 BSP 层获取 FG 脉冲总数 (真实 + 虚拟)
    uint32_t totalPulses = bsp::motor_pwm::getFgPulseCount();

    // Delta 检测: 对比上一个周期
    if (totalPulses != prevFgPulseCount) {
        lastFgPulseTime = now;
        prevFgPulseCount = totalPulses;
    }

    // 检查 FG 是否有脉冲超过堵转阈值
    unsigned long sinceLastPulse = now - lastFgPulseTime;
    if (sinceLastPulse > MOTOR_STALL_TIMEOUT) {
        stalled = true;
    }
}

// ==================== 限位开关读取 (通过 BSP 层) ====================

bool DoorMotor::isFullOpenLimit() const {
    return bsp::limits::isDoorFullOpenTriggered();
}

bool DoorMotor::isClosedLimit() const {
    return bsp::limits::isDoorClosedTriggered();
}
