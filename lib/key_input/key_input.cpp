/**
 * @file key_input.cpp
 * @brief 按键输入模块 - 实现
 */

#include "key_input.h"
#include "door_lock.h"
#include "door_motor.h"

KeyInput g_keyInput;

void KeyInput::init() {
    initKey(keyOpen,  PIN_KEY_OPEN);
    initKey(keyClose, PIN_KEY_CLOSE);
    initKey(keyStop,  PIN_KEY_STOP);

    Serial.println(F("[KEY] Key input initialized"));
}

void KeyInput::loop() {
    // 更新各按键状态, 检测事件
    if (updateKey(keyOpen) && isPressed(keyOpen)) {
        onOpenPress();
    }

    if (updateKey(keyClose) && isPressed(keyClose)) {
        onClosePress();
    }

    if (updateKey(keyStop)) {
        if (isLongPress(keyStop)) {
            onStopLongPress();
        } else if (isPressed(keyStop)) {
            onStopShortPress();
        }
    }
}

// ============================================================
//  按键事件处理 - 调用核心驱动模块
// ============================================================

void KeyInput::onOpenPress() {
    Serial.println(F("[KEY] Open key pressed"));
    if (!g_doorLock.isUnlocked()) {
        g_doorLock.unlock();
    } else if (!g_doorMotor.isRunning()) {
        g_doorMotor.open();
    }
}

void KeyInput::onClosePress() {
    Serial.println(F("[KEY] Close key pressed"));
    if (!g_doorMotor.isRunning()) {
        g_doorMotor.close();
    }
}

void KeyInput::onStopShortPress() {
    Serial.println(F("[KEY] Stop key short press - motor stop"));
    g_doorMotor.emergencyStop();
}

void KeyInput::onStopLongPress() {
    Serial.println(F("[KEY] Stop key long press - lockout"));
    // TODO: 实现反锁逻辑
}

// ============================================================
//  按键底层: 消抖 + 长按检测
// ============================================================

void KeyInput::initKey(KeyState& key, uint8_t pin) {
    key.pin = pin;
    key.lastLevel = HIGH;
    key.stableLevel = HIGH;
    key.lastEdgeTime = 0;
    key.pressStartTime = 0;
    key.pressed = false;
    key.longPressFired = false;
    pinMode(pin, INPUT_PULLUP);
}

bool KeyInput::updateKey(KeyState& key) {
    bool changed = false;
    uint8_t level = digitalRead(key.pin);
    unsigned long now = millis();

    if (level != key.lastLevel) {
        key.lastEdgeTime = now;
        key.lastLevel = level;
    }

    if ((now - key.lastEdgeTime) >= KEY_DEBOUNCE_MS) {
        if (level != key.stableLevel) {
            key.stableLevel = level;
            changed = true;

            if (level == LOW) {
                // 按下
                key.pressed = false;       // 等待释放后触发
                key.pressStartTime = now;
                key.longPressFired = false;
            } else {
                // 释放
                if (!key.longPressFired) {
                    key.pressed = true;    // 标记为短按事件
                }
            }
        }
    }

    // 长按检测 (按下期间持续检查)
    if (key.stableLevel == LOW && !key.longPressFired) {
        if ((now - key.pressStartTime) >= KEY_LONG_PRESS_MS) {
            key.longPressFired = true;
            key.pressed = false;
            changed = true;
        }
    }

    return changed;
}

bool KeyInput::isPressed(KeyState& key) {
    if (key.pressed) {
        key.pressed = false;
        return true;
    }
    return false;
}

bool KeyInput::isLongPress(KeyState& key) {
    return key.longPressFired;
}
