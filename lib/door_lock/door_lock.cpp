/**
 * @file door_lock.cpp
 * @brief 门锁控制模块 - 实现
 *
 * L9110S 双 H 桥驱动:
 *   - PIN_LOCK_OPEN_MOTOR (GPIO5):  A-1, HIGH = 正转开锁
 *   - PIN_LOCK_RESET_MOTOR (GPIO17): A-2, HIGH = 反转复位
 *   - 互锁: 两个引脚不能同时为 HIGH (防止 H 桥短路)
 *
 * 状态机流程:
 *   IDLE -> UNLOCKING (开锁电机运行) -> [开锁限位触发] -> IDLE (已开)
 *   IDLE -> WAIT_RELOCK (等待3秒或门关闭) -> RELICKING (复位电机运行) -> [复位限位触发] -> IDLE (已锁)
 *
 * 硬件依赖: 全部通过 BSP 层访问，不直接调用 Arduino API
 *   - GPIO 控制 → bsp::gpio
 *   - 限位开关  → bsp::limits
 */

#include "door_lock.h"
#include "door_motor.h"
#include "bsp_gpio.h"
#include "bsp_limits.h"

// ==================== 全局单例 ====================
DoorLock g_doorLock;

// ==================== 生命周期 ====================
void DoorLock::init() {
    // 配置 L9110S 驱动引脚
    bsp::gpio::initOutput(PIN_LOCK_OPEN_MOTOR);
    bsp::gpio::initOutput(PIN_LOCK_RESET_MOTOR);

    // 默认安全状态: 两个电机都关闭 (互锁)
    stopAllMotors();

    // 限位开关统一由 BSP 初始化, 此处不再初始化
    state = State::IDLE;
    actionStartTime = 0;

    Serial.println(F("[LOCK] Door lock initialized (L9110S)"));
}

/**
 * @brief 主循环轮询 (10ms 周期调用)
 *        职责: 状态机推进 + 限位检查 + 超时保护
 */
void DoorLock::loop() {
    if (state == State::IDLE) {
        return;
    }

    unsigned long now = millis();

    switch (state) {
        case State::UNLOCKING:
            // 检查开锁限位
            if (isOpenLimit()) {
                emergencyStop();
                Serial.println(F("[LOCK] Unlock complete (open limit reached)"));
            }
            // 超时保护 (5秒)
            if (actionStartTime > 0 && (now - actionStartTime) > 5000) {
                emergencyStop();
                Serial.println(F("[LOCK] EMERGENCY STOP: Unlock timeout!"));
            }
            break;

        case State::WAIT_RELOCK: {
            // 等待条件: 3秒延时 或 门位置限位回到锁门状态
            // 需求: "检测到门限位开关回到锁门状态也门锁电机复位"
            bool timeElapsed = (now - actionStartTime) >= LOCK_RESET_DELAY;
            bool doorClosed = g_doorMotor.isDoorClosed();

            if (timeElapsed || doorClosed) {
                state = State::RELICKING;
                actionStartTime = now;
                startRelockMotor();
                Serial.println(F("[LOCK] Relocking..."));
            }
            break;
        }

        case State::RELICKING:
            // 检查复位限位
            if (isCloseLimit()) {
                emergencyStop();
                Serial.println(F("[LOCK] Relock complete (close limit reached)"));
            }
            // 超时保护 (5秒)
            if (actionStartTime > 0 && (now - actionStartTime) > 5000) {
                emergencyStop();
                Serial.println(F("[LOCK] EMERGENCY STOP: Relock timeout!"));
            }
            break;

        default:
            break;
    }
}

// ==================== 运动控制接口 ====================

void DoorLock::unlock() {
    if (state != State::IDLE) {
        Serial.println(F("[LOCK] Busy, cannot unlock"));
        return;
    }

    // 前提: 门已锁 (复位限位触发)
    if (!isCloseLimit()) {
        Serial.println(F("[LOCK] Door not locked, skip unlock"));
        return;
    }

    state = State::UNLOCKING;
    actionStartTime = millis();
    startUnlockMotor();
    Serial.println(F("[LOCK] Unlocking..."));
}

void DoorLock::relock() {
    if (state != State::IDLE) {
        Serial.println(F("[LOCK] Busy, cannot relock"));
        return;
    }

    // 前提: 门锁已开 (开锁限位触发)
    if (!isOpenLimit()) {
        Serial.println(F("[LOCK] Lock not unlocked, skip relock"));
        return;
    }

    state = State::WAIT_RELOCK;
    actionStartTime = millis();
    Serial.println(F("[LOCK] Waiting for relock condition..."));
}

void DoorLock::emergencyStop() {
    stopAllMotors();
    state = State::IDLE;
    actionStartTime = 0;
    Serial.println(F("[LOCK] Emergency stop executed"));
}

// ==================== 状态查询接口 ====================

bool DoorLock::isLocked() const {
    return isCloseLimit();
}

bool DoorLock::isUnlocked() const {
    return isOpenLimit();
}

bool DoorLock::isLockRunning() const {
    return state != State::IDLE;
}

bool DoorLock::isUnlocking() const {
    return state == State::UNLOCKING;
}

bool DoorLock::isRelocking() const {
    return state == State::RELICKING;
}

// ==================== 内部方法 ====================

void DoorLock::startUnlockMotor() {
    // L9110S 互锁: 先确保复位电机关闭, 再开启开锁电机
    bsp::gpio::writeLow(PIN_LOCK_RESET_MOTOR);
    bsp::gpio::writeHigh(PIN_LOCK_OPEN_MOTOR);
}

void DoorLock::stopAllMotors() {
    // 安全状态: 两个电机都关闭
    bsp::gpio::writeLow(PIN_LOCK_OPEN_MOTOR);
    bsp::gpio::writeLow(PIN_LOCK_RESET_MOTOR);
}

void DoorLock::startRelockMotor() {
    // L9110S 互锁: 先确保开锁电机关闭, 再开启复位电机
    bsp::gpio::writeLow(PIN_LOCK_OPEN_MOTOR);
    bsp::gpio::writeHigh(PIN_LOCK_RESET_MOTOR);
}

// ==================== 限位开关读取 (通过 BSP 层) ====================

bool DoorLock::isOpenLimit() const {
    return bsp::limits::isLockOpenTriggered();
}

bool DoorLock::isCloseLimit() const {
    return bsp::limits::isLockCloseTriggered();
}
