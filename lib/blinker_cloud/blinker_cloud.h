/**
 * @file blinker_cloud.h
 * @brief 点灯科技 Blinker 云接入模块 - 头文件
 *
 * 负责:
 *   - WiFi 连接管理 (自动重连)
 *   - Blinker 云平台接入
 *   - APP 远程开门指令处理
 *   - 设备在线状态上报
 */

#ifndef BLINKER_CLOUD_H
#define BLINKER_CLOUD_H

#include "app_config.h"

// 前向声明, 避免在头文件中包含 Blinker.h
class BlinkerTimer;

class BlinkerCloud {
public:
    void init(const char* wifiSsid, const char* wifiPassword, const char* authKey);
    void loop();

    // 查询是否有远程开门请求
    bool hasRemoteOpenRequest();
    void consumeRemoteOpenRequest();

private:
    BlinkerTimer* timer;
    bool remoteOpenPending;
    bool wifiConnected;

    void onWifiConnect();
    void onWifiDisconnect();
    void heartbeat();
};

extern BlinkerCloud g_blinker;

#endif // BLINKER_CLOUD_H
