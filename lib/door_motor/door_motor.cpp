/**
 * @file door_motor.cpp
 * @brief 开门电机控制模块 - 实现
 *
 * 核心驱动逻辑:
 *   - 状态机驱动开门/关门流程
 *   - PWM 渐变 (软启动/软停止)
 *   - FG 脉冲中断计数 + 堵转检测
 *   - 限位开关到位停机
 *   - 超时保护
 */

#include "door_motor.h"
#include "test_mode.h"

// ==================== 全局单例 ====================
DoorMotor g_doorMotor;

// ==================== 生命周期 ====================
void DoorMotor::init() {
    // 配置 GPIO
    pinMode(PIN_MOTOR_DIR, OUTPUT);
    pinMode(PIN_MOTOR_SPEED_FB, INPUT_PULLUP);
    digitalWrite(PIN_MOTOR_DIR, LOW);

    // 配置 LEDC PWM
    ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttachPin(PIN_MOTOR_PWM, MOTOR_PWM_CHANNEL);
    ledcWrite(MOTOR_PWM_CHANNEL, 0);

    // 初始化状态
    state = State::IDLE;
    currentPwm = 0;
    targetPwm = 0;
    lastRampTime = 0;
    actionStartTime = 0;
    stalled = false;

    // 初始化 FG 中断
    fgPulseCount = 0;
    prevFgPulseCount = 0;
    lastFgCheckTime = 0;
    lastFgPulseTime = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_SPEED_FB), fgIsrStub, FALLING);

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
    if (state == State::CLOSING_SLOW && currentPwm >= MOTOR_SLOW_PWM) {
        // 慢速阶段持续一段时间或一定角度后切换到快速
        // 简化: 当 PWM 达到 MOTOR_SLOW_PWM 后继续 ramp 到 MOTOR_FAST_PWM
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
    fgPulseCount = 0;
    prevFgPulseCount = 0;
    lastFgPulseTime = millis();
    actionStartTime = millis();
    currentPwm = 0;

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
    fgPulseCount = 0;
    prevFgPulseCount = 0;
    lastFgPulseTime = millis();
    actionStartTime = millis();
    currentPwm = 0;

    // 设置方向 = 反转关门
    setDirection(false);
    targetPwm = MOTOR_SLOW_PWM;  // 先慢速启动
    state = State::CLOSING_SLOW;
    lastRampTime = millis();

    Serial.println(F("[MOTOR] Closing..."));
}

void DoorMotor::emergencyStop() {
    // 立即切断 PWM
    ledcWrite(MOTOR_PWM_CHANNEL, 0);
    currentPwm = 0;
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

// ==================== 内部方法 ====================

void DoorMotor::setPwm(uint8_t value) {
    // 确保 PWM 不低于启动阈值 (除非完全停止)
    if (value > 0 && value < MOTOR_MIN_PWM) {
        value = MOTOR_MIN_PWM;
    }
    ledcWrite(MOTOR_PWM_CHANNEL, value);
    currentPwm = value;
}

void DoorMotor::rampStep() {
    unsigned long now = millis();
    if ((now - lastRampTime) < PWM_RAMP_INTERVAL) {
        return;  // 未到步进时间
    }
    lastRampTime = now;

    if (currentPwm < targetPwm) {
        // 上升
        uint16_t newVal = (uint16_t)currentPwm + PWM_RAMP_STEP;
        if (newVal > targetPwm) newVal = targetPwm;
        if (newVal > 255) newVal = 255;
        setPwm((uint8_t)newVal);
    } else if (currentPwm > targetPwm) {
        // 下降
        int16_t newVal = (int16_t)currentPwm - PWM_RAMP_STEP;
        if (newVal < targetPwm) newVal = targetPwm;
        if (newVal < 0) newVal = 0;
        setPwm((uint8_t)newVal);
    }
}

void DoorMotor::setDirection(bool forward) {
    digitalWrite(PIN_MOTOR_DIR, forward ? HIGH : LOW);
}

// ==================== FG 堵转检测 ====================

void IRAM_ATTR DoorMotor::fgIsrStub() {
    g_doorMotor.fgIsr();
}

void IRAM_ATTR DoorMotor::fgIsr() {
    fgPulseCount++;
    // 不在此调用 millis(), 由 loop() 中的 checkStall() 记录时间戳
}

void DoorMotor::checkStall() {
    unsigned long now = millis();

    // 仅在电机运行时检测
    if (state == State::IDLE) {
        lastFgCheckTime = now;
        return;
    }

    // 刷新 FG 脉冲时间戳 (在 loop 中安全调用 millis)
    // 通过对比上一个周期和当前周期的脉冲计数判断是否有新脉冲
    if (fgPulseCount != prevFgPulseCount) {
        lastFgPulseTime = now;
        prevFgPulseCount = fgPulseCount;
    }

    // 测试模式下使用虚拟 FG 脉冲
    if (g_testMode.isActive()) {
        fgPulseCount += g_testMode.getFgPulseCount();
        if (g_testMode.getFgPulseCount() > 0) {
            lastFgPulseTime = now;  // 模拟脉冲更新最后脉冲时间
        }
        g_testMode.consumeFgPulses();
    }

    // 检查 FG 是否有脉冲
    unsigned long sinceLastPulse = now - lastFgPulseTime;
    if (sinceLastPulse > MOTOR_STALL_TIMEOUT) {
        stalled = true;
    }
}

// ==================== 限位开关读取 ====================

bool DoorMotor::isFullOpenLimit() const {
    if (g_testMode.isActive()) return g_testMode.getDoorFullOpenLimit();
    return digitalRead(PIN_DOOR_FULL_OPEN) == LOW;
}

bool DoorMotor::isClosedLimit() const {
    if (g_testMode.isActive()) return g_testMode.getDoorClosedLimit();
    return digitalRead(PIN_DOOR_CLOSED) == LOW;
}
