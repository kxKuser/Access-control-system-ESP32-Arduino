/**
 * @file key_input.h
 * @brief 按键输入模块 - 头文件
 *
 * 3个物理按键 (INPUT_PULLUP, LOW 有效):
 *   - 开门键 (GPIO33): 未开锁则先开锁再开门, 已开锁则直接开门
 *   - 关门键 (GPIO26): 执行关门操作
 *   - 停止键 (GPIO25): 短按=停止电机, 长按=反锁模式
 *
 * 非阻塞 millis() 实现, 含消抖和长按检测
 */

#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include "app_config.h"

class KeyInput {
public:
    void init();
    void loop();

    // 按键事件回调 (由 loop 内部触发)
    void onOpenPress();
    void onClosePress();
    void onStopShortPress();
    void onStopLongPress();

private:
    // 单个按键状态
    struct KeyState {
        uint8_t pin;
        bool lastLevel;         // 上一次读取电平
        bool stableLevel;       // 消抖后稳定电平
        unsigned long lastEdgeTime;
        unsigned long pressStartTime;
        bool pressed;           // 当前是否按下
        bool longPressFired;    // 长按事件是否已触发
    };

    KeyState keyOpen;
    KeyState keyClose;
    KeyState keyStop;

    void initKey(KeyState& key, uint8_t pin);
    bool updateKey(KeyState& key);
    bool isPressed(KeyState& key);
    bool isLongPress(KeyState& key);
};

extern KeyInput g_keyInput;

#endif // KEY_INPUT_H
