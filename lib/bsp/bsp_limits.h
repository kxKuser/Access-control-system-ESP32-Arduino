/**
 * @file bsp_limits.h
 * @brief BSP 层 - 限位开关统一封装
 *
 * 封装 4 个限位开关的读取逻辑，内建测试模式虚拟限位支持。
 *
 * ┌────────────────────┬──────────┬──────────────────┐
 * │ 函数                │ 引脚     │ 含义 (LOW=触发)  │
 * ├────────────────────┼──────────┼──────────────────┤
 * │ isLockOpenTriggered │ GPIO19   │ 门锁已开到位     │
 * │ isLockCloseTriggered│ GPIO18   │ 门锁已锁到位     │
 * │ isDoorFullOpenTriggered│ GPIO4│ 开门到位(全开)   │
 * │ isDoorClosedTriggered  │GPIO16│ 关门到位(锁门)   │
 * └────────────────────┴──────────┴──────────────────┘
 *
 * 测试模式集成:
 *   当 g_testMode.isActive() 为 true 时，读取虚拟限位值;
 *   否则读取真实硬件 GPIO 引脚。
 *   业务模块无需关心当前是真实硬件还是测试模式。
 */

#ifndef BSP_LIMITS_H
#define BSP_LIMITS_H

namespace bsp {
namespace limits {

/**
 * @brief 初始化所有限位开关引脚
 *        (配置为 INPUT_PULLUP)
 */
void init();

/**
 * @brief 门锁开锁限位是否触发 (GPIO19)
 *        LOW = 门锁已打开到位
 */
bool isLockOpenTriggered();

/**
 * @brief 门锁复位限位是否触发 (GPIO18)
 *        LOW = 门锁已锁到位
 */
bool isLockCloseTriggered();

/**
 * @brief 门全开限位是否触发 (GPIO4)
 *        LOW = 门已开到最大位置
 */
bool isDoorFullOpenTriggered();

/**
 * @brief 门关闭限位是否触发 (GPIO16)
 *        LOW = 门已完全关闭
 */
bool isDoorClosedTriggered();

} // namespace limits
} // namespace bsp

#endif // BSP_LIMITS_H
