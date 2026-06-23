/**
 * @file rs232_comm.cpp
 * @brief RS232 串口通信模块 - 实现
 *
 * 硬件依赖: 全部通过 BSP 层访问
 *   - UART2 → bsp::uart
 */

#include "rs232_comm.h"
#include "bsp_uart.h"

Rs232Comm g_rs232;

void Rs232Comm::init() {
    bsp::uart::initRs232(RS232_BAUDRATE, PIN_RS232_RX, PIN_RS232_TX);
    openCommandPending = false;
    lockoutActive = false;
    lockoutStartTime = 0;
    resetRx();
    Serial.println(F("[RS232] UART2 initialized"));
}

void Rs232Comm::loop() {
    // 反锁超时自动解除
    if (lockoutActive && lockoutStartTime > 0) {
        if (millis() - lockoutStartTime >= LOCKOUT_TIMEOUT) {
            lockoutActive = false;
            lockoutStartTime = 0;
            Serial.println(F("[RS232] Lockout auto-disabled (timeout)"));
        }
    }

    // 非阻塞串口接收, 逐字节解析协议帧
    while (bsp::uart::rs232Available()) {
        uint8_t byte = bsp::uart::rs232Read();
        processByte(byte);
    }
}

bool Rs232Comm::hasOpenCommand() {
    // 反锁模式下不传递开门指令
    if (lockoutActive) return false;
    return openCommandPending;
}

void Rs232Comm::consumeOpenCommand() {
    openCommandPending = false;
}

void Rs232Comm::setLockout(bool enabled) {
    lockoutActive = enabled;
    if (enabled) {
        lockoutStartTime = millis();
        Serial.println(F("[RS232] LOCKOUT enabled - RS232 commands blocked"));
    } else {
        lockoutStartTime = 0;
        Serial.println(F("[RS232] LOCKOUT disabled"));
    }
}

bool Rs232Comm::isLockedOut() const {
    return lockoutActive;
}

// ============================================================
//  协议帧解析状态机
//  帧格式: [0xAA][0x55][指令码][校验和]
//  校验和 = 前3字节异或
// ============================================================
void Rs232Comm::resetRx() {
    rxState = RxState::WAIT_HEADER1;
    rxIndex = 0;
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

void Rs232Comm::processByte(uint8_t byte) {
    switch (rxState) {
        case RxState::WAIT_HEADER1:
            if (byte == PROTOCOL_HEADER_1) {
                rxBuffer[0] = byte;
                rxIndex = 1;
                rxState = RxState::WAIT_HEADER2;
            }
            break;

        case RxState::WAIT_HEADER2:
            if (byte == PROTOCOL_HEADER_2) {
                rxBuffer[1] = byte;
                rxIndex = 2;
                rxState = RxState::WAIT_CMD;
            } else {
                // 帧头不匹配, 重置状态机
                // 但如果这个字节正好是新的帧头, 从它开始重新解析
                if (byte == PROTOCOL_HEADER_1) {
                    rxBuffer[0] = byte;
                    rxIndex = 1;
                    // 保持在 WAIT_HEADER2 状态
                } else {
                    resetRx();
                }
            }
            break;

        case RxState::WAIT_CMD:
            rxBuffer[2] = byte;
            rxIndex = 3;
            rxState = RxState::WAIT_CHECKSUM;
            break;

        case RxState::WAIT_CHECKSUM:
            rxBuffer[3] = byte;
            rxIndex = 4;

            // 校验通过后处理指令
            if (validateChecksum(rxBuffer)) {
                uint8_t cmd = rxBuffer[2];
                if (cmd == CMD_OPEN_DOOR) {
                    openCommandPending = true;
                    Serial.println(F("[RS232] Valid OPEN command received"));
                } else {
                    Serial.print(F("[RS232] Unknown command: 0x"));
                    Serial.println(cmd, HEX);
                }
            } else {
                Serial.println(F("[RS232] Checksum mismatch, frame rejected"));
            }

            // 解析完成, 重置状态机准备下一帧
            resetRx();
            break;
    }
}

bool Rs232Comm::validateChecksum(uint8_t* frame) {
    uint8_t checksum = frame[0] ^ frame[1] ^ frame[2];
    return (checksum == frame[3]);
}
