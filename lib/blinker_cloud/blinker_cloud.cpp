/**
 * @file blinker_cloud.cpp
 * @brief 点灯科技 Blinker 云接入模块 - 实现
 *
 * ============================================================
 *  条件编译说明:
 *    - 若 PlatformIO 已安装 Blinker 库, 启用完整云功能
 *    - 否则降级为模拟模式, 仅本地串口运行
 *
 *  安装 Blinker 库:
 *    pio lib install https://gitee.com/mirrors/blinker-library.git
 *    (或已在 platformio.ini lib_deps 中配置)
 * ============================================================
 */

#include "blinker_cloud.h"
#include "door_lock.h"
#include "door_motor.h"
#include "rs232_comm.h"

// ---- Blinker 库检测 ----
#if __has_include("Blinker.h")
    #define BLINKER_WIFI
    #define BLINKER_PRINT Serial
    #include <Blinker.h>
    #define HAS_BLINKER 1
#else
    #define HAS_BLINKER 0
    #warning "Blinker library not found, cloud features disabled. Run: pio lib install https://gitee.com/mirrors/blinker-library.git"
#endif

// ==================== 全局单例 ====================
BlinkerCloud g_blinker;

// ==================== Blinker 组件 (仅在库可用时实例化) ====================
#if HAS_BLINKER
    BlinkerButton g_btnOpen("btn-open");
    BlinkerButton g_btnClose("btn-close");
    BlinkerButton g_btnStop("btn-stop");
#endif

// ==================== 生命周期 ====================

void BlinkerCloud::init(const char* wifiSsid, const char* wifiPwd,
                        const char* key) {
    // 保存配置
    strncpy(wifiSSID, wifiSsid, sizeof(wifiSSID) - 1);
    strncpy(wifiPassword, wifiPwd, sizeof(wifiPassword) - 1);
    strncpy(authKey, key, sizeof(authKey) - 1);

    // 初始化状态
    pendingCmd = RemoteCmd::NONE;
    wifiConnected = false;
    lastWiFiCheckTime = 0;
    wifiDisconnectTime = 0;
    lastStatusReportTime = 0;

#if HAS_BLINKER
    // 注册 Blinker 按钮回调
    g_btnOpen.attach(BlinkerCloud::onBtnOpen);
    g_btnClose.attach(BlinkerCloud::onBtnClose);
    g_btnStop.attach(BlinkerCloud::onBtnStop);

    // 注册数据上报回调
    Blinker.attachData(BlinkerCloud::onDataRead);

    // 初始化 Blinker (内部管理 WiFi 连接, 无需外部 connectWiFi)
    Blinker.begin(authKey, wifiSSID, wifiPassword);
    Serial.println(F("[BLINKER] Blinker cloud initialized"));
#else
    // 无 Blinker 库时, 自行连接 WiFi
    connectWiFi();
#endif

    Serial.print(F("[BLINKER] WiFi SSID: "));
    Serial.println(wifiSSID);
    Serial.print(F("[BLINKER] Blinker key: "));
    Serial.println(strlen(authKey) > 5 ? F("***configured***") : F("NOT SET"));

#if !HAS_BLINKER
    Serial.println(F("[BLINKER] Running in SIMULATION mode (no cloud)"));
#endif
}

void BlinkerCloud::loop() {
#if HAS_BLINKER
    Blinker.run();
#endif

    // WiFi 连接检查与重连
    checkWiFi();

    // 状态上报
    reportStatus();
}

// ==================== 远程命令接口 ====================

RemoteCmd BlinkerCloud::getRemoteCmd() {
    return pendingCmd;
}

void BlinkerCloud::clearRemoteCmd() {
    pendingCmd = RemoteCmd::NONE;
}

// ==================== WiFi 管理 ====================

void BlinkerCloud::connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);
    lastWiFiCheckTime = millis();
    Serial.print(F("[BLINKER] Connecting WiFi: "));
    Serial.println(wifiSSID);
}

void BlinkerCloud::checkWiFi() {
    unsigned long now = millis();

    // 限流: 每 500ms 检查一次
    if (now - lastWiFiCheckTime < 500) {
        return;
    }
    lastWiFiCheckTime = now;

    bool wasConnected = wifiConnected;
    wifiConnected = (WiFi.status() == WL_CONNECTED);

    if (wifiConnected && !wasConnected) {
        onWiFiConnected();
    }

    if (!wifiConnected && wasConnected) {
        onWiFiDisconnected();
    }

    // 断线重连 (5秒间隔)
    if (!wifiConnected) {
        if (now - wifiDisconnectTime >= WIFI_RECONNECT_INTERVAL) {
            wifiDisconnectTime = now;
            Serial.println(F("[BLINKER] Reconnecting WiFi..."));
            WiFi.reconnect();
        }
    }
}

void BlinkerCloud::onWiFiConnected() {
    wifiDisconnectTime = 0;
    Serial.print(F("[BLINKER] WiFi connected, IP: "));
    Serial.println(WiFi.localIP());
}

void BlinkerCloud::onWiFiDisconnected() {
    wifiDisconnectTime = millis();
    Serial.println(F("[BLINKER] WiFi disconnected, will reconnect in 5s"));
}

bool BlinkerCloud::isConnected() const {
    return wifiConnected;
}

const char* BlinkerCloud::getWifiSSID() const {
    return wifiSSID;
}

// ==================== 状态聚合 ====================

BlinkerCloud::DeviceStatus BlinkerCloud::getStatus() const {
    DeviceStatus s;
    s.doorOpen       = g_doorMotor.isDoorOpen();
    s.doorClosed     = g_doorMotor.isDoorClosed();
    s.doorOpening    = g_doorMotor.isDoorOpening();
    s.doorClosing    = g_doorMotor.isDoorClosing();
    s.lockLocked     = g_doorLock.isLocked();
    s.lockUnlocked   = g_doorLock.isUnlocked();
    s.lockRunning    = g_doorLock.isLockRunning();
    s.motorPwm       = g_doorMotor.getCurrentPwm();
    s.stalled        = g_doorMotor.isStalled();
    s.wifiOk         = wifiConnected;
    s.lockout        = g_rs232.isLockedOut();
    return s;
}

// ==================== 状态上报 (每2秒) ====================

void BlinkerCloud::reportStatus() {
    unsigned long now = millis();
    if (now - lastStatusReportTime < STATUS_REPORT_INTERVAL) {
        return;
    }
    lastStatusReportTime = now;

    DeviceStatus s = getStatus();

#if HAS_BLINKER
    // 虚拟数据同步到 APP (Blinker.print = virtualWrite)
    Blinker.print("v-online", s.wifiOk ? "online" : "offline");

    // 门状态
    const char* doorState = s.doorOpening ? "opening" :
                            s.doorClosing ? "closing" :
                            s.doorOpen    ? "open" : "closed";
    Blinker.print("v-door", doorState);

    // 锁状态
    const char* lockState = s.lockRunning ? "running" :
                            s.lockUnlocked ? "unlocked" : "locked";
    Blinker.print("v-lock", lockState);

    // PWM 值
    Blinker.print("v-pwm", s.motorPwm);

    // 故障状态
    Blinker.print("v-fault", s.stalled ? "stall" : "ok");

    // 反锁状态
    Blinker.print("v-lockout", s.lockout ? "lockout" : "normal");

    // 文本状态 (综合)
    char buf[128];
    snprintf(buf, sizeof(buf),
             "门:%s | 锁:%s | PWM:%u | %s%s | WiFi:%s",
             doorState, lockState, s.motorPwm,
             s.stalled ? "堵转! " : "",
             s.lockout ? "反锁 " : "",
             s.wifiOk ? "OK" : "断");
    Blinker.print("v-summary", buf);

    // 远程日志同步
    static uint32_t lastLogSeq = 0;
    if (s.stalled && lastLogSeq != 1) {
        Serial.println(F("[BLINKER] MOTOR STALL detected!"));
        lastLogSeq = 1;
    }
    if (s.doorOpening || s.doorClosing) {
        Serial.printf("[BLINKER] Motor running: PWM=%u dir=%s\n",
                      s.motorPwm, s.doorOpening ? "OPEN" : "CLOSE");
    }
#endif
}

// ==================== Blinker 按钮回调 (仅在 HAS_BLINKER 时编译) ====================

#if HAS_BLINKER

void BlinkerCloud::onBtnOpen(const String& state) {
    if (state == BLINKER_CMD_BUTTON_TAP) {
        Serial.println(F("[BLINKER] APP: Open door requested"));
        g_blinker.pendingCmd = RemoteCmd::OPEN_DOOR;
    }
}

void BlinkerCloud::onBtnClose(const String& state) {
    if (state == BLINKER_CMD_BUTTON_TAP) {
        Serial.println(F("[BLINKER] APP: Close door requested"));
        g_blinker.pendingCmd = RemoteCmd::CLOSE_DOOR;
    }
}

void BlinkerCloud::onBtnStop(const String& state) {
    if (state == BLINKER_CMD_BUTTON_TAP) {
        Serial.println(F("[BLINKER] APP: Emergency stop requested"));
        g_blinker.pendingCmd = RemoteCmd::STOP;
    }
}

void BlinkerCloud::onDataRead(const String& data) {
    Serial.print(F("[BLINKER] APP data request: "));
    Serial.println(data);
    // 立即上报一次状态
    g_blinker.lastStatusReportTime = 0;
    g_blinker.reportStatus();
}

#endif // HAS_BLINKER
