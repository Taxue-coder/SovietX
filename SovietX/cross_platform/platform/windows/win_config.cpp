/**
 * @file win_config.cpp
 * @brief SovietX Windows 配置持久化实现
 * 
 * 使用 INI 风格的文本文件存储在 %APPDATA%/SovietX/config.ini。
 */

#ifdef _WIN32

#include "../../common/config.h"
#include "../../common/log.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <mutex>

namespace soviet {

static std::string PathToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}

/**
 * 简易 INI 配置实现。
 * 使用 key=value 文本格式。
 * 格式：每行 key=value，# 开头为注释。
 */
class WindowsConfig : public PlatformConfig {
public:
    WindowsConfig() {
        PWSTR appdata = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appdata))) {
            m_filePath = std::filesystem::path(appdata) / L"SovietX" / L"config.ini";
            CoTaskMemFree(appdata);
        } else {
            m_filePath = L"soviet_config.ini";
        }

        std::error_code error;
        const auto directory = m_filePath.parent_path();
        if (!directory.empty()) {
            std::filesystem::create_directories(directory, error);
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
            Log("Failed to save config to %s", PathToUtf8(m_filePath).c_str());
            return;
        }
        out << "# SovietX Windows Config\n";
        for (const auto& kv : m_data) {
            out << kv.first << "=" << kv.second << "\n";
        }
        out.close();
        Log("Config saved to %s (%zu entries)", PathToUtf8(m_filePath).c_str(), m_data.size());
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
        Log("Config loaded from %s (%zu entries)", PathToUtf8(m_filePath).c_str(), m_data.size());
    }

    std::filesystem::path m_filePath;
    std::map<std::string, std::string> m_data;
    std::mutex m_mutex;
};

PlatformConfig* CreateWindowsConfig() {
    return new WindowsConfig();
}

} // namespace soviet

#endif // _WIN32
