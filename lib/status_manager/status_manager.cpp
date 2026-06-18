/**
 * @file status_manager.cpp
 * @brief 系统状态管理模块 - 实现
 */

#include "status_manager.h"

// 全局状态实例
StatusManager g_status;

void StatusManager::init() {
    // 配置限位开关引脚 (INPUT_PULLUP, LOW 有效)
    pinMode(PIN_LOCK_OPEN_LIMIT,  INPUT_PULLUP);
    pinMode(PIN_LOCK_CLOSE_LIMIT, INPUT_PULLUP);
    pinMode(PIN_DOOR_CLOSED,      INPUT_PULLUP);
    pinMode(PIN_DOOR_FULL_OPEN,   INPUT_PULLUP);

    // 配置转速反馈引脚
    pinMode(PIN_MOTOR_SPEED_FB, INPUT);

    // 初始状态
    doorState  = DoorState::CLOSED;
    lockState  = LockState::LOCKED;
    motorDir   = MotorDir::STOP;
    currentPwm = 0;
    systemMode = SystemMode::NORMAL;
    lockoutStartTime = 0;

    Serial.println(F("[STATUS] Status manager initialized"));
}

bool StatusManager::isDoorClosed() {
    return digitalRead(PIN_DOOR_CLOSED) == LOW;
}

bool StatusManager::isDoorFullOpen() {
    return digitalRead(PIN_DOOR_FULL_OPEN) == LOW;
}

bool StatusManager::isLockLocked() {
    return digitalRead(PIN_LOCK_CLOSE_LIMIT) == LOW;
}

bool StatusManager::isLockUnlocked() {
    return digitalRead(PIN_LOCK_OPEN_LIMIT) == LOW;
}

void StatusManager::setLockout(bool enabled) {
    if (enabled) {
        systemMode = SystemMode::LOCKOUT;
        lockoutStartTime = millis();
        Serial.println(F("[STATUS] LOCKOUT mode enabled"));
    } else {
        systemMode = SystemMode::NORMAL;
        lockoutStartTime = 0;
        Serial.println(F("[STATUS] LOCKOUT mode disabled"));
    }
}

bool StatusManager::isLockedOut() {
    if (systemMode != SystemMode::LOCKOUT) {
        return false;
    }
    // 反锁超时自动解除
    if (lockoutStartTime > 0 && (millis() - lockoutStartTime >= LOCKOUT_TIMEOUT)) {
        setLockout(false);
        return false;
    }
    return true;
}

void StatusManager::updateDoorState() {
    if (isDoorClosed()) {
        doorState = DoorState::CLOSED;
    } else if (isDoorFullOpen()) {
        doorState = DoorState::OPEN;
    }
}

void StatusManager::updateLockState() {
    if (isLockLocked()) {
        lockState = LockState::LOCKED;
    } else if (isLockUnlocked()) {
        lockState = LockState::UNLOCKED;
    }
}
