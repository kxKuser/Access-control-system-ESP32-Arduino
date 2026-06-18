/**
 * @file rs232_comm.cpp
 * @brief RS232 串口通信模块 - 实现
 */

#include "rs232_comm.h"

Rs232Comm g_rs232;

void Rs232Comm::init() {
    Serial2.begin(RS232_BAUDRATE, SERIAL_8N1, PIN_RS232_RX, PIN_RS232_TX);
    openCommandPending = false;
    rxIndex = 0;
    memset(rxBuffer, 0, sizeof(rxBuffer));
    Serial.println(F("[RS232] UART2 initialized"));
}

void Rs232Comm::loop() {
    // TODO: 非阻塞串口接收, 逐字节解析协议帧
    while (Serial2.available()) {
        uint8_t byte = Serial2.read();
        processByte(byte);
    }
}

bool Rs232Comm::hasOpenCommand() {
    return openCommandPending;
}

void Rs232Comm::consumeOpenCommand() {
    openCommandPending = false;
}

void Rs232Comm::processByte(uint8_t byte) {
    // TODO: 实现协议状态机
    // 状态: WAIT_HEADER1 -> WAIT_HEADER2 -> READ_CMD -> READ_CHECKSUM
    // 匹配帧头 [0xAA][0x55], 读取指令码和校验和
    // 校验通过后设置 openCommandPending = true
}

bool Rs232Comm::validateChecksum(uint8_t* frame) {
    uint8_t checksum = frame[0] ^ frame[1] ^ frame[2];
    return (checksum == frame[3]);
}
