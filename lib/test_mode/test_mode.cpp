/**
 * @file test_mode.cpp
 * @brief 测试模式模块 - 实现
 */

#include "test_mode.h"
#include "door_lock.h"
#include "door_motor.h"

TestMode g_testMode;

void TestMode::init() {
    vDoorFullOpenLimit = false;
    vDoorClosedLimit   = true;   // 初始: 门已关闭
    vLockOpenLimit     = false;
    vLockCloseLimit    = true;   // 初始: 门已锁
    vFgPulseCount      = 0;
    active             = false;
    lastCmdTime        = 0;

    Serial.println(F("[TEST] Test mode module ready"));
    Serial.println(F("[TEST] Send any command to activate test mode"));
    Serial.println(F("[TEST] Commands: O=Open C=Close S=Stop d1/d0 c1/c0 l1/l0 r1/r0 fN ?=Status"));
}

void TestMode::loop(unsigned long serialDisableTimeout) {
    // 检查超时自动退出
    if (active && serialDisableTimeout > 0) {
        if (millis() - lastCmdTime > serialDisableTimeout) {
            active = false;
            Serial.println(F("[TEST] Test mode auto-disabled (timeout)"));
        }
    }

    processSerial();
}

bool TestMode::isActive() const {
    return active;
}

// ==================== 虚拟限位开关读取 ====================

bool TestMode::getDoorFullOpenLimit() const {
    if (!active) return digitalRead(PIN_DOOR_FULL_OPEN) == LOW;
    return vDoorFullOpenLimit;
}

bool TestMode::getDoorClosedLimit() const {
    if (!active) return digitalRead(PIN_DOOR_CLOSED) == LOW;
    return vDoorClosedLimit;
}

bool TestMode::getLockOpenLimit() const {
    if (!active) return digitalRead(PIN_LOCK_OPEN_LIMIT) == LOW;
    return vLockOpenLimit;
}

bool TestMode::getLockCloseLimit() const {
    if (!active) return digitalRead(PIN_LOCK_CLOSE_LIMIT) == LOW;
    return vLockCloseLimit;
}

// ==================== 虚拟 FG 脉冲 ====================

uint32_t TestMode::getFgPulseCount() const {
    return vFgPulseCount;
}

void TestMode::consumeFgPulses() {
    vFgPulseCount = 0;
}

// ==================== 串口命令处理 ====================

void TestMode::processSerial() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    // 任何有效命令都激活测试模式
    if (!active) {
        active = true;
        Serial.println(F("[TEST] Test mode ACTIVATED"));
    }
    lastCmdTime = millis();

    // 单字符命令
    if (cmd.length() == 1) {
        char ch = cmd[0];
        switch (ch) {
            case 'O':
                // 触发完整开门流程 (先开锁再开门)
                if (!g_doorLock.isUnlocked()) {
                    g_doorLock.unlock();
                } else if (!g_doorMotor.isRunning()) {
                    g_doorMotor.open();
                }
                Serial.println(F("OK OPEN"));
                break;

            case 'C':
                if (!g_doorMotor.isRunning()) {
                    g_doorMotor.close();
                }
                Serial.println(F("OK CLOSE"));
                break;

            case 'S':
                g_doorMotor.emergencyStop();
                g_doorLock.emergencyStop();
                Serial.println(F("OK STOP"));
                break;

            case '?':
                sendStatus();
                break;

            default:
                Serial.println(F("ERR Unknown command"));
                break;
        }
        return;
    }

    // 双字符命令
    if (cmd.length() == 2) {
        char prefix = cmd[0];
        char value  = cmd[1];

        switch (prefix) {
            case 'd': // 门全开限位
                vDoorFullOpenLimit = (value == '1');
                Serial.print(F("OK DOOR_OPEN_LIMIT="));
                Serial.println(vDoorFullOpenLimit ? F("ON") : F("OFF"));
                break;

            case 'c': // 门关闭限位
                vDoorClosedLimit = (value == '1');
                Serial.print(F("OK DOOR_CLOSE_LIMIT="));
                Serial.println(vDoorClosedLimit ? F("ON") : F("OFF"));
                break;

            case 'l': // 锁已开限位
                vLockOpenLimit = (value == '1');
                Serial.print(F("OK LOCK_OPEN_LIMIT="));
                Serial.println(vLockOpenLimit ? F("ON") : F("OFF"));
                break;

            case 'r': // 锁复位限位
                vLockCloseLimit = (value == '1');
                Serial.print(F("OK LOCK_CLOSE_LIMIT="));
                Serial.println(vLockCloseLimit ? F("ON") : F("OFF"));
                break;

            case 'f': // 模拟 FG 脉冲
                if (value >= '0' && value <= '9') {
                    vFgPulseCount += (uint32_t)(value - '0');
                    Serial.print(F("OK FG_PULSES+="));
                    Serial.println(value - '0');
                } else {
                    Serial.println(F("ERR Invalid FG count"));
                }
                break;

            default:
                Serial.println(F("ERR Unknown command"));
                break;
        }
        return;
    }

    Serial.println(F("ERR Invalid command format"));
}

void TestMode::sendStatus() {
    Serial.print(F("STATUS:"));
    Serial.print(F(" doorOpen="));
    Serial.print(g_doorMotor.isDoorOpen() ? F("1") : F("0"));
    Serial.print(F(" doorClosed="));
    Serial.print(g_doorMotor.isDoorClosed() ? F("1") : F("0"));
    Serial.print(F(" isOpening="));
    Serial.print(g_doorMotor.isDoorOpening() ? F("1") : F("0"));
    Serial.print(F(" isClosing="));
    Serial.print(g_doorMotor.isDoorClosing() ? F("1") : F("0"));

    Serial.print(F(" lockLocked="));
    Serial.print(g_doorLock.isLocked() ? F("1") : F("0"));
    Serial.print(F(" lockUnlocked="));
    Serial.print(g_doorLock.isUnlocked() ? F("1") : F("0"));
    Serial.print(F(" lockRunning="));
    Serial.print(g_doorLock.isLockRunning() ? F("1") : F("0"));

    Serial.print(F(" motorPwm="));
    Serial.print(g_doorMotor.getCurrentPwm());

    Serial.print(F(" doorLimitOpen="));
    Serial.print(vDoorFullOpenLimit ? F("ON") : F("OFF"));
    Serial.print(F(" doorLimitClose="));
    Serial.print(vDoorClosedLimit ? F("ON") : F("OFF"));
    Serial.print(F(" lockLimitOpen="));
    Serial.print(vLockOpenLimit ? F("ON") : F("OFF"));
    Serial.print(F(" lockLimitClose="));
    Serial.println(vLockCloseLimit ? F("ON") : F("OFF"));
}
