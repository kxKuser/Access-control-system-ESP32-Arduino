/**
 * @file rs232_comm.h
 * @brief RS232 串口通信模块 - 头文件
 *
 * 负责 UART2 串口通信:
 *   - 接收门外门禁板发送的开门指令
 *   - 帧格式: [0xAA][0x55][指令码][校验和]
 *   - 校验和 = 前3字节异或
 */

#ifndef RS232_COMM_H
#define RS232_COMM_H

#include "app_config.h"

class Rs232Comm {
public:
    void init();
    void loop();

    // 查询是否有新的开门指令
    bool hasOpenCommand();
    // 消费开门指令 (读取后清除标志)
    void consumeOpenCommand();

    // 反锁控制 (由 key_input 调用)
    void setLockout(bool enabled);
    bool isLockedOut() const;

private:
    bool openCommandPending;
    bool lockoutActive;
    unsigned long lockoutStartTime;

    // 协议解析状态机
    enum class RxState : uint8_t {
        WAIT_HEADER1,
        WAIT_HEADER2,
        WAIT_CMD,
        WAIT_CHECKSUM
    };
    RxState rxState;
    void processByte(uint8_t byte);
    bool validateChecksum(uint8_t* frame);
    void resetRx();

    // 接收缓冲区
    uint8_t rxBuffer[PROTOCOL_FRAME_LEN];
    uint8_t rxIndex;
};

extern Rs232Comm g_rs232;

#endif // RS232_COMM_H
