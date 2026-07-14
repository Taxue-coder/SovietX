/**
 * @file config.cpp
 * @brief SovietX 统一配置管理实现
 */

#include "config.h"

namespace soviet {

static PlatformConfig* g_configInstance = nullptr;

PlatformConfig* GetConfig() {
    return g_configInstance;
}

void SetConfig(PlatformConfig* config) {
    g_configInstance = config;
}

} // namespace soviet
