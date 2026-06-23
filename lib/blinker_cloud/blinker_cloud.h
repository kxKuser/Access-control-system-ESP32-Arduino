/**
 * @file blinker_cloud.h
 * @brief 点灯科技 Blinker 云接入模块 - 头文件
 *
 * 职责:
 *   - WiFi 自动连接 / 断线自动重连 (5秒间隔, 非阻塞)
 *   - Blinker 云平台接入 (APP 远程控制)
 *   - APP 远程开门 / 关门 / 停止按钮
 *   - 设备状态实时上报 (在线状态、门锁状态、门位置、故障信息)
 *   - 远程日志查看 (设备运行日志同步到 APP)
 *
 * 依赖:
 *   - door_motor.h / door_lock.h (状态读取 + 运动控制)
 *
 * 条件编译:
 *   - 若 __has_include("Blinker.h")=true 则启用完整 Blinker 功能
 *   - 否则降级为模拟模式 (无云端连接, 仅本地串口日志)
 */

#ifndef BLINKER_CLOUD_H
#define BLINKER_CLOUD_H

#include "app_config.h"

// ============================================================
//  远程命令类型
// ============================================================
enum class RemoteCmd : uint8_t {
    NONE,
    OPEN_DOOR,       // 远程开门
    CLOSE_DOOR,      // 远程关门
    STOP,            // 远程停止
};

// ============================================================
//  BlinkerCloud 类
// ============================================================
class BlinkerCloud {
public:
    // ==================== 生命周期 ====================
    void init(const char* wifiSsid, const char* wifiPwd, const char* authKey);
    void loop();

    // ==================== 远程命令接口 ====================
    /// 获取待处理远程命令, 消费后自动清除
    RemoteCmd getRemoteCmd();
    void     clearRemoteCmd();

    // ==================== WiFi 状态 ====================
    bool isConnected() const;
    const char* getWifiSSID() const;

    // ==================== 状态聚合 ====================
    struct DeviceStatus {
        bool doorOpen;       // 门已开
        bool doorClosed;     // 门已关
        bool doorOpening;    // 开门中
        bool doorClosing;    // 关门中
        bool lockLocked;     // 已锁
        bool lockUnlocked;   // 已开
        bool lockRunning;    // 锁运行中
        uint8_t motorPwm;    // 当前 PWM
        bool stalled;        // 堵转
        bool wifiOk;         // WiFi 已连接
        bool lockout;        // 反锁模式
    };
    DeviceStatus getStatus() const;

private:
    // ==================== 配置 ====================
    char wifiSSID[33];
    char wifiPassword[65];
    char authKey[33];

    // ==================== 远程命令队列 ====================
    RemoteCmd pendingCmd;

    // ==================== WiFi 管理 ====================
    bool wifiConnected;
    unsigned long lastWiFiCheckTime;
    unsigned long wifiDisconnectTime;
    static const unsigned long WIFI_RECONNECT_INTERVAL = 5000;  // 5秒重连

    void connectWiFi();
    void checkWiFi();
    void onWiFiConnected();
    void onWiFiDisconnected();

    // ==================== 状态上报 ====================
    unsigned long lastStatusReportTime;
    static const unsigned long STATUS_REPORT_INTERVAL = 2000;   // 2秒上报
    void reportStatus();

    // ==================== Blinker 回调 (仅在 Blinker 可用时注册) ====================
#if __has_include("Blinker.h")
    static void onBtnOpen(const String& state);
    static void onBtnClose(const String& state);
    static void onBtnStop(const String& state);
    static void onDataRead(const String& data);
#endif
};

// 全局单例
extern BlinkerCloud g_blinker;

#endif // BLINKER_CLOUD_H
