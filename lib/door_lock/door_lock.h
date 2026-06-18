/**
 * @file door_lock.h
 * @brief 门锁控制模块 - 头文件
 *
 * 核心职责:
 *   - 驱动 L9110S 双 H 桥门锁电机 (正转开锁 / 反转复位)
 *   - 硬件互锁: PIN_LOCK_OPEN_MOTOR 和 PIN_LOCK_RESET_MOTOR 不能同时为 HIGH
 *   - 开锁流程: 检测复位限位 -> 驱动开锁电机 -> 等待开锁限位 -> 停止
 *   - 复位流程: 检测开锁限位 -> 延时3秒或门关闭 -> 驱动复位电机 -> 等待复位限位 -> 停止
 *   - 紧急停止: 立即切断两个电机输出
 *
 * 硬件连接 (L9110S 驱动):
 *   - PIN_LOCK_OPEN_MOTOR  (GPIO5):  A-1, HIGH = 正转开锁
 *   - PIN_LOCK_RESET_MOTOR (GPIO17): A-2, HIGH = 反转复位
 *   - 两个引脚互锁, 不能同时为 HIGH
 *
 * 限位开关:
 *   - PIN_LOCK_OPEN_LIMIT  (GPIO19): LOW = 门锁已开到位
 *   - PIN_LOCK_CLOSE_LIMIT (GPIO18): LOW = 门已锁到位 (复位完成)
 *
 * 外部调用方: key_input, rs232_comm, blinker_cloud (通过统一接口触发开锁/复位/停止)
 */

#ifndef DOOR_LOCK_H
#define DOOR_LOCK_H

#include "app_config.h"

class DoorLock {
public:
    // ==================== 生命周期 ====================
    void init();
    void loop();  // 非阻塞轮询: 状态机 + 限位检查 + 超时保护

    // ==================== 运动控制接口 ====================
    /**
     * @brief 触发开锁流程
     *        前提: 门已锁 (复位限位触发)
     *        行为: 驱动开锁电机 (PIN_LOCK_OPEN_MOTOR=HIGH)
     *              检测到开锁限位后自动停止, 进入 UNLOCKED 状态
     */
    void unlock();

    /**
     * @brief 触发复位(上锁)流程
     *        前提: 门锁已开 (开锁限位触发)
     *        行为: 延时3秒或检测到门关闭 -> 驱动复位电机 (PIN_LOCK_RESET_MOTOR=HIGH)
     *              检测到复位限位后自动停止, 进入 LOCKED 状态
     */
    void relock();

    /**
     * @brief 紧急停止
     *        行为: 立即切断两个电机输出, 清除互锁状态
     */
    void emergencyStop();

    // ==================== 状态查询接口 ====================
    bool isLocked() const;        // 是否已锁 (复位限位触发)
    bool isUnlocked() const;      // 是否已开 (开锁限位触发)
    bool isLockRunning() const;   // 门锁电机是否在运行
    bool isUnlocking() const;     // 是否正在开锁
    bool isRelocking() const;     // 是否正在复位

private:
    // ==================== 内部状态机 ====================
    enum class State : uint8_t {
        IDLE,           // 空闲
        UNLOCKING,      // 开锁电机运行中
        WAIT_RELOCK,    // 等待复位条件 (3秒延时或门关闭)
        RELICKING       // 复位电机运行中
    };

    State state;
    unsigned long actionStartTime;

    // ==================== 电机控制 ====================
    void startUnlockMotor();   // PIN_LOCK_OPEN_MOTOR=HIGH, PIN_LOCK_RESET_MOTOR=LOW
    void stopAllMotors();      // 两个电机都=LOW (互锁安全状态)
    void startRelockMotor();   // PIN_LOCK_RESET_MOTOR=HIGH, PIN_LOCK_OPEN_MOTOR=LOW

    // ==================== 限位开关读取 ====================
    bool isOpenLimit() const;   // 开锁限位 (GPIO19, LOW=已开)
    bool isCloseLimit() const;  // 复位限位 (GPIO18, LOW=已锁)
};

// 全局单例
extern DoorLock g_doorLock;

#endif // DOOR_LOCK_H
