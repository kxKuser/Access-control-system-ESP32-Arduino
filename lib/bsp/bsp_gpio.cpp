/**
 * @file bsp_gpio.cpp
 * @brief BSP 层 - GPIO 统一封装实现
 *
 * 这是整个系统中唯一允许调用 pinMode/digitalWrite/digitalRead 的模块。
 */

#include "bsp_gpio.h"

namespace bsp {
namespace gpio {

void initOutput(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void initInputPullup(uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);
}

void writeHigh(uint8_t pin) {
    digitalWrite(pin, HIGH);
}

void writeLow(uint8_t pin) {
    digitalWrite(pin, LOW);
}

bool read(uint8_t pin) {
    return digitalRead(pin) == HIGH;
}

} // namespace gpio
} // namespace bsp
