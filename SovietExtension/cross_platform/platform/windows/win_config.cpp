/**
 * @file win_config.cpp
 * @brief SovietExtension Windows 配置持久化实现
 * 
 * 使用 JSON 文件存储在 %APPDATA%/SovietExtension/config.json。
 * 简单实现，不依赖第三方 JSON 库。
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/log.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <map>
#include <mutex>

namespace soviet {

/**
 * 简易 JSON 配置实现。
 * 使用 key=value 文本格式（非标准 JSON，但足够用）。
 * 格式：每行 key=value，# 开头为注释。
 */
class WindowsConfig : public PlatformConfig {
public:
    WindowsConfig() {
        // 配置文件路径：%APPDATA%/SovietExtension/config.ini
        char appdata[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
            std::string dir = std::string(appdata) + "\\SovietExtension";
            CreateDirectoryA(dir.c_str(), NULL);
            m_filePath = dir + "\\config.ini";
        } else {
            m_filePath = "soviet_config.ini";
        }
        LoadFromFile();
    }

    bool GetBool(const std::string& key, bool defaultVal) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        return it->second == "1" || it->second == "true";
    }

    void SetBool(const std::string& key, bool val) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[key] = val ? "1" : "0";
    }

    int GetInt(const std::string& key, int defaultVal) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        try { return std::stoi(it->second); }
        catch (...) { return defaultVal; }
    }

    void SetInt(const std::string& key, int val) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[key] = std::to_string(val);
    }

    double GetDouble(const std::string& key, double defaultVal) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        try { return std::stod(it->second); }
        catch (...) { return defaultVal; }
    }

    void SetDouble(const std::string& key, double val) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[key] = std::to_string(val);
    }

    std::string GetString(const std::string& key, const std::string& defaultVal) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_data.find(key);
        if (it == m_data.end()) return defaultVal;
        return it->second;
    }

    void SetString(const std::string& key, const std::string& val) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[key] = val;
    }

    void Save() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ofstream out(m_filePath);
        if (!out.is_open()) {
            Log("Failed to save config to %s", m_filePath.c_str());
            return;
        }
        out << "# SovietExtension Windows Config\n";
        for (const auto& kv : m_data) {
            out << kv.first << "=" << kv.second << "\n";
        }
        out.close();
        Log("Config saved to %s (%zu entries)", m_filePath.c_str(), m_data.size());
    }

    bool HasKey(const std::string& key) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.find(key) != m_data.end();
    }

    void Remove(const std::string& key) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.erase(key);
    }

private:
    void LoadFromFile() {
        std::ifstream in(m_filePath);
        if (!in.is_open()) return;

        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            m_data[key] = val;
        }
        in.close();
        Log("Config loaded from %s (%zu entries)", m_filePath.c_str(), m_data.size());
    }

    std::string m_filePath;
    std::map<std::string, std::string> m_data;
    std::mutex m_mutex;
};

static WindowsConfig g_windowsConfig;

void InitWindowsConfig() {
    SetConfig(&g_windowsConfig);
}

} // namespace soviet

#endif // _WIN32
