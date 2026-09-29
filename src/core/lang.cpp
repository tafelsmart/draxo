#include "pch.h"
#include "core/strcrypt.h"
#include "core/lang.h"
#include "core/config.h"
#include <fstream>
#include <filesystem>

static std::unordered_map<std::string, std::string> s_translations;
static std::string s_currentLang = "en";

static std::string langDir() {
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) return "assets/lang/";
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring wpath(path, n);
    auto slash = wpath.find_last_of(L"\/");
    if (slash != std::wstring::npos) wpath = wpath.substr(0, slash + 1);
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], len, nullptr, nullptr);
    while (!result.empty() && result.back() == '\0') result.pop_back();
    return result + "assets/lang/";
}

static bool loadJsonFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Simple JSON parser: {"key":"value","key2":"value2",...}
    size_t pos = 0;
    while (pos < content.size()) {
        auto keyStart = content.find('"', pos);
        if (keyStart == std::string::npos) break;
        auto keyEnd = content.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;
        std::string key = content.substr(keyStart + 1, keyEnd - keyStart - 1);
        auto valStart = content.find('"', keyEnd + 1);
        if (valStart == std::string::npos) break;
        auto valEnd = content.find('"', valStart + 1);
        if (valEnd == std::string::npos) break;
        std::string val = content.substr(valStart + 1, valEnd - valStart - 1);
        s_translations[key] = val;
        pos = valEnd + 1;
    }
    printf(STR_C("[Draxo] Language loaded: %s (%zu entries)\n"), path.c_str(), s_translations.size());
    return true;
}

void Lang::init() {
    s_currentLang = Config::getString("Client", "lang", "en");
    setLanguage(s_currentLang.c_str());
}

void Lang::shutdown() { s_translations.clear(); }

const char* Lang::get(const char* key) {
    auto it = s_translations.find(key);
    return it != s_translations.end() ? it->second.c_str() : key;
}

void Lang::setLanguage(const char* lang) {
    s_translations.clear();
    s_currentLang = lang;
    std::string dir = langDir();
    // Try exact lang file first, then fallback to en
    if (!loadJsonFile(dir + "lang_" + lang + ".json"))
        loadJsonFile(dir + "lang_en.json");
    Config::setString("Client", "lang", lang);
}

const char* Lang::currentLanguage() { return s_currentLang.c_str(); }

std::vector<std::string> Lang::availableLanguages() {
    std::vector<std::string> out;
    out.push_back("en");
    out.push_back("de");
    return out;
}

void Lang::add(const char* key, const char* value) {
    s_translations[key] = value;
}
