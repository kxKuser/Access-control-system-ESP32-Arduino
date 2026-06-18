/**
 * @file door_motor.h
 * @brief 开门电机控制模块 - 头文件
 *
 * 核心职责:
 *   - 驱动五线无刷开门电机 (CCW/CW方向 + PWM调速 + FG转速反馈)
 *   - PWM 渐变控制 (软启动/软停止, 避免机械冲击)
 *   - FG 堵转检测保护 (FG 无脉冲超过阈值则紧急停机)
 *   - 开门/关门总超时保护 (防止限位开关失效导致电机一直运行)
 *   - 紧急停止接口 (立即切断 PWM 输出)
 *
 * 硬件连接:
 *   - PIN_MOTOR_DIR    (GPIO2):  方向控制 (HIGH=正转开门, LOW=反转关门)
 *   - PIN_MOTOR_PWM    (GPIO15): PWM 输出 (LEDC, 0-255)
 *   - PIN_MOTOR_SPEED_FB (GPIO13): FG 转速反馈 (脉冲输入)
 *
 * 外部调用方: key_input, rs232_comm, blinker_cloud (通过统一接口触发开门/关门/停止)
 */

#ifndef DOOR_MOTOR_H
#define DOOR_MOTOR_H

#include "app_config.h"

class DoorMotor {
public:
    // ==================== 生命周期 ====================
    void init();
    void loop();  // 非阻塞轮询: 状态机 + FG 堵转检测 + PWM 渐变

    // ==================== 运动控制接口 ====================
    /**
     * @brief 触发开门流程
     *        前提: 门锁已开 (由调用方确保)
     *        行为: 设置方向=正转, PWM 从 0 渐变到 MOTOR_FAST_PWM
     *              检测到 PIN_DOOR_FULL_OPEN 限位后自动停止
     */
    void open();

    /**
     * @brief 触发关门流程
     *        行为: 设置方向=反转, PWM 先慢 (MOTOR_SLOW_PWM) 后快 (MOTOR_FAST_PWM)
     *              检测到 PIN_DOOR_CLOSED 限位后自动停止
     */
    void close();

    /**
     * @brief 紧急停止
     *        行为: 立即切断 PWM 输出, 锁定电机, 清除堵转标志
     *        适用: 按键停止、堵转保护、超时保护
     */
    void emergencyStop();

    // ==================== 状态查询接口 ====================
    bool isDoorOpening() const;   // 正在开门中
    bool isDoorClosing() const;   // 正在关门中
    bool isDoorOpen() const;      // 门已开到最大 (开门限位触发)
    bool isDoorClosed() const;    // 门已关闭 (锁门限位触发)
    bool isRunning() const;       // 电机是否在运行 (任何方向)
    bool isStalled() const;       // 是否检测到堵转

private:
    // ==================== 内部状态机 ====================
    enum class State : uint8_t {
        IDLE,
        OPENING_RAMP,     // 开门 PWM 渐变上升阶段
        OPENING,          // 开门全速运行阶段
        CLOSING_SLOW,     // 关门慢速阶段
        CLOSING_FAST,     // 关门快速阶段
        STOPPING          // 减速停止阶段
    };

    State state;

    // ==================== PWM 渐变控制 ====================
    unsigned long lastRampTime;   // 上次 PWM 步进时间
    uint8_t currentPwm;           // 当前 PWM 值
    uint8_t targetPwm;            // 目标 PWM 值

    void setPwm(uint8_t value);
    void rampStep();              // 执行一步 PWM 渐变 (在 loop 中调用)
    void setDirection(bool forward); // true=正转开门, false=反转关门

    // ==================== FG 堵转检测 ====================
    volatile unsigned long fgPulseCount;  // FG 脉冲计数 (中断中递增)
    unsigned long lastFgCheckTime;        // 上次检查 FG 的时间
    unsigned long lastFgPulseTime;        // 上次检测到 FG 脉冲的时间

    static void fgIsrStub();      // 中断服务程序存根 (static)
    void fgIsr();                 // 实际中断处理
    void checkStall();            // 堵转检测逻辑 (在 loop 中调用)

    // ==================== 超时保护 ====================
    unsigned long actionStartTime;  // 当前动作开始时间
    bool stalled;                   // 堵转标志

    // ==================== 限位开关读取 ====================
    bool isFullOpenLimit() const;   // 开门到位限位
    bool isClosedLimit() const;     // 关门到位限位
};

// 全局单例
extern DoorMotor g_doorMotor;

#endif // DOOR_MOTOR_H
