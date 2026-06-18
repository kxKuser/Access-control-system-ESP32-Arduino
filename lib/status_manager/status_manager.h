/**
 * @file status_manager.h
 * @brief 系统状态管理模块 - 头文件
 *
 * 集中管理门状态、锁状态、电机状态、系统模式
 * 作为全局单例供其他模块读写
 */

#ifndef STATUS_MANAGER_H
#define STATUS_MANAGER_H

#include "app_config.h"

class StatusManager {
public:
    void init();

    // 门状态
    DoorState   doorState;
    // 锁状态
    LockState   lockState;
    // 电机方向
    MotorDir    motorDir;
    // 当前 PWM 值
    uint8_t     currentPwm;
    // 系统模式 (正常/反锁)
    SystemMode  systemMode;

    // 读取门限位开关 (true=已触发)
    bool isDoorClosed();
    bool isDoorFullOpen();

    // 读取门锁限位开关 (true=已触发)
    bool isLockLocked();
    bool isLockUnlocked();

    // 反锁控制
    void setLockout(bool enabled);
    bool isLockedOut();

    // 更新门状态 (根据限位开关)
    void updateDoorState();
    // 更新锁状态 (根据限位开关)
    void updateLockState();

private:
    unsigned long lockoutStartTime;
};

// 全局状态实例
extern StatusManager g_status;

#endif // STATUS_MANAGER_H
