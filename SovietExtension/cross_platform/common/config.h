/**
 * @file config.h
 * @brief SovietX 统一配置管理接口
 * 
 * 抽象配置读写接口，平台层提供具体实现：
 * - macOS: NSUserDefaults 封装
 * - Windows: JSON 文件或注册表
 */

#ifndef SOVIET_CONFIG_H
#define SOVIET_CONFIG_H

#include <string>
#include <memory>

namespace soviet {

/**
 * 平台配置抽象接口。
 * 所有功能模块通过此接口读写配置，不直接依赖平台原生 API。
 */
class PlatformConfig {
public:
    virtual ~PlatformConfig() = default;

    // 布尔值
    virtual bool GetBool(const std::string& key, bool defaultVal) = 0;
    virtual void SetBool(const std::string& key, bool val) = 0;

    // 整数值
    virtual int GetInt(const std::string& key, int defaultVal) = 0;
    virtual void SetInt(const std::string& key, int val) = 0;

    // 浮点值
    virtual double GetDouble(const std::string& key, double defaultVal) = 0;
    virtual void SetDouble(const std::string& key, double val) = 0;

    // 字符串值
    virtual std::string GetString(const std::string& key, const std::string& defaultVal) = 0;
    virtual void SetString(const std::string& key, const std::string& val) = 0;

    // 持久化（同步到磁盘/注册表）
    virtual void Save() = 0;

    // 检查键是否存在
    virtual bool HasKey(const std::string& key) = 0;

    // 删除键
    virtual void Remove(const std::string& key) = 0;
};

/**
 * 获取全局配置实例（平台层实现初始化）。
 * 首次调用时应由平台层 SetConfig() 注入具体实现。
 */
PlatformConfig* GetConfig();

/**
 * 设置全局配置实例（由平台层在插件加载时调用）。
 * 所有权不归此类，调用方负责生命周期。
 */
void SetConfig(PlatformConfig* config);

} // namespace soviet

#endif // SOVIET_CONFIG_H
