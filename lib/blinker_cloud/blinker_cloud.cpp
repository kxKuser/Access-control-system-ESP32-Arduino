/**
 * @file blinker_cloud.cpp
 * @brief 点灯科技 Blinker 云接入模块 - 实现
 */

// Blinker 库需手动安装到 lib/ 目录, 安装后取消下方注释
// #define BLINKER_PRINT(Serial)
// #include <Blinker.h>

#include "blinker_cloud.h"

BlinkerCloud g_blinker;

void BlinkerCloud::init(const char* wifiSsid, const char* wifiPassword, const char* authKey) {
    remoteOpenPending = false;
    wifiConnected = false;

    // TODO: 初始化 Blinker
    // Blinker.begin(authKey, wifiSsid, wifiPassword);
    // 注册数据回调、心跳回调

    Serial.println(F("[BLINKER] Initializing..."));
}

void BlinkerCloud::loop() {
    // TODO: Blinker 非阻塞轮询
    // Blinker.run();
}

bool BlinkerCloud::hasRemoteOpenRequest() {
    return remoteOpenPending;
}

void BlinkerCloud::consumeRemoteOpenRequest() {
    remoteOpenPending = false;
}

void BlinkerCloud::onWifiConnect() {
    wifiConnected = true;
    Serial.println(F("[BLINKER] WiFi connected"));
}

void BlinkerCloud::onWifiDisconnect() {
    wifiConnected = false;
    Serial.println(F("[BLINKER] WiFi disconnected"));
}

void BlinkerCloud::heartbeat() {
    // TODO: 上报设备状态到 Blinker
    // 包含门状态、锁状态、系统模式
}
