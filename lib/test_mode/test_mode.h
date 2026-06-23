/**
 * @file test_mode.h
 * @brief 测试模式模块 - 头文件
 *
 * 用于 PC 端串口工具联调测试, 通过 USB Serial 模拟:
 *   - 门限位开关 (开门到位 / 关门到位)
 *   - 门锁电机限位开关 (开锁到位 / 复位到位)
 *   - 开门电机 FG 转速反馈脉冲
 *
 * 协议 (PC -> ESP32, 通过 USB Serial):
 *   O      - 触发开门 (等同于 RS232 开门指令)
 *   C      - 触发关门
 *   S      - 紧急停止
 *   d1/d0  - 门全开限位 (1=触发LOW, 0=释放HIGH)
 *   c1/c0  - 门关闭限位 (1=触发LOW, 0=释放HIGH)
 *   l1/l0  - 锁已开限位 (1=触发LOW, 0=释放HIGH)
 *   r1/r0  - 锁复位限位 (1=触发LOW, 0=释放HIGH)
 *   fN     - 模拟 FG 脉冲 (N=脉冲数, 如 f5=发送5个脉冲)
 *   ?      - 查询状态
 *
 * 协议 (ESP32 -> PC):
 *   OK      - 命令执行成功
 *   STATUS: door=X lock=X motor=X pwm=X doorL=X closeL=X lockOL=X lockCL=X
 */

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include "app_config.h"

class TestMode {
public:
    void init();

    /**
     * @brief 主循环轮询, 处理 PC 端串口命令
     * @param serialDisableTimeout 无通信超时自动退出测试模式 (ms), 0=不超时
     */
    void loop(unsigned long serialDisableTimeout = 0);

    // 测试模式是否激活
    bool isActive() const;

    // ==================== 虚拟限位开关状态 ====================
    // (供 door_motor / door_lock 模块在测试模式下读取)
    bool getDoorFullOpenLimit() const;   // 门全开限位
    bool getDoorClosedLimit() const;     // 门关闭限位
    bool getLockOpenLimit() const;       // 锁已开限位
    bool getLockCloseLimit() const;      // 锁复位限位

    // ==================== 虚拟 FG 脉冲 ====================
    uint32_t getFgPulseCount() const;    // 累计脉冲数
    void     consumeFgPulses();          // 消费脉冲 (读取后清除)

private:
    // 虚拟限位开关状态 (true=触发, LOW)
    bool vDoorFullOpenLimit;
    bool vDoorClosedLimit;
    bool vLockOpenLimit;
    bool vLockCloseLimit;

    // 虚拟 FG 脉冲
    uint32_t vFgPulseCount;

    // 通信状态
    bool active;
    unsigned long lastCmdTime;

    // 串口命令处理
    void processSerial();
    void sendStatus();
};

extern TestMode g_testMode;

#endif // TEST_MODE_H
