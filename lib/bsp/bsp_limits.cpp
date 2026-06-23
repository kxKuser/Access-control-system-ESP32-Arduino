/**
 * @file bsp_limits.cpp
 * @brief BSP 层 - 限位开关实现
 *
 * 统一管理限位开关读取 + 测试模式虚拟限位注入。
 * 测试模式相关逻辑在此集中处理，不再渗透到 door_lock / door_motor 等业务模块。
 */

#include "bsp_limits.h"
#include "bsp_gpio.h"
#include "app_config.h"
#include "test_mode.h"

namespace bsp {
namespace limits {

void init() {
    bsp::gpio::initInputPullup(PIN_LOCK_OPEN_LIMIT);
    bsp::gpio::initInputPullup(PIN_LOCK_CLOSE_LIMIT);
    bsp::gpio::initInputPullup(PIN_DOOR_FULL_OPEN);
    bsp::gpio::initInputPullup(PIN_DOOR_CLOSED);
}

bool isLockOpenTriggered() {
    if (g_testMode.isActive()) return g_testMode.getLockOpenLimit();
    return bsp::gpio::read(PIN_LOCK_OPEN_LIMIT) == false;  // LOW = 触发
}

bool isLockCloseTriggered() {
    if (g_testMode.isActive()) return g_testMode.getLockCloseLimit();
    return bsp::gpio::read(PIN_LOCK_CLOSE_LIMIT) == false;
}

bool isDoorFullOpenTriggered() {
    if (g_testMode.isActive()) return g_testMode.getDoorFullOpenLimit();
    return bsp::gpio::read(PIN_DOOR_FULL_OPEN) == false;
}

bool isDoorClosedTriggered() {
    if (g_testMode.isActive()) return g_testMode.getDoorClosedLimit();
    return bsp::gpio::read(PIN_DOOR_CLOSED) == false;
}

} // namespace limits
} // namespace bsp
