#include "pch.h"
#include "core/strcrypt.h"
#include "core/config.h"
#include <windows.h>
#include <algorithm>

// ── Path helpers ────────────────────────────────────────────────────

std::string Config::dir() {
    wchar_t path[MAX_PATH] = {};
    HMODULE self = GetModuleHandleW(L"draxo.dll");
    if (!self) self = GetModuleHandleW(L"null.dll");
    if (!self) return ".";
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring wpath(path, n);
    auto slash = wpath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) wpath = wpath.substr(0, slash + 1);
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], len, nullptr, nullptr);
    while (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}

std::string Config::getPath() {
    return dir() + "draxo_config.ini";
}

std::string Config::sanitize(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ' || c == '.')
            out += c;
        else
            out += '_';
    }
    if (out.empty()) out = "config";
    return out;
}

std::string Config::pathFor(const std::string& name) {
    return dir() + "cfg_" + sanitize(name) + ".ini";
}

// ── File I/O ────────────────────────────────────────────────────────

bool Config::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
        while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
        s_data[key] = val;
    }
    printf(STR_C("[Draxo] Config loaded: %zu entries (%s)\n"), s_data.size(), path.c_str());
    return true;
}

bool Config::writeFile(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    for (auto& [k, v] : s_data) f << k << "=" << v << "\n";
    return true;
}

// ── Named config management ─────────────────────────────────────────

bool Config::load() {
    s_data.clear();
    std::string autoName = getAutoLoad();
    if (!autoName.empty()) {
        s_currentPath = pathFor(autoName);
        if (readFile(s_currentPath)) return true;
    }
    s_currentPath = getPath();
    return readFile(s_currentPath);
}

bool Config::save() {
    if (s_currentPath.empty()) s_currentPath = getPath();
    bool ok = writeFile(s_currentPath);
    printf(STR_C("[Draxo] Config saved: %zu entries (%s)\n"), s_data.size(), s_currentPath.c_str());
    return ok;
}

bool Config::saveAs(const std::string& name) {
    s_currentPath = pathFor(name);
    return save();
}

bool Config::loadNamed(const std::string& name) {
    s_data.clear();
    s_currentPath = pathFor(name);
    if (readFile(s_currentPath)) return true;
    // Config doesn't exist yet → treat as fresh empty config
    printf(STR_C("[Draxo] Config '%s' not found — starting fresh\n"), name.c_str());
    return true;
}

bool Config::saveDefault() {
    s_currentPath = getPath();
    return save();
}

bool Config::loadDefault() {
    s_data.clear();
    s_currentPath = getPath();
    if (readFile(s_currentPath)) return true;
    printf(STR_C("[Draxo] Default config not found — starting fresh\n"));
    return true;
}

bool Config::deleteConfig(const std::string& name) {
    std::string p = pathFor(name);
    if (DeleteFileA(p.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) {
        if (getAutoLoad() == sanitize(name)) setAutoLoad("");
        // Wurde die AKTIVE Config gelöscht? Dann zurück auf Default schalten,
        // sonst würde ein späteres Config::save() die Datei wieder anlegen.
        if (!s_currentPath.empty() && s_currentPath == p)
            s_currentPath = getPath();
        printf(STR_C("[Draxo] Config '%s' deleted\n"), name.c_str());
        return true;
    }
    return false;
}

std::vector<std::string> Config::listConfigs() {
    std::vector<std::string> out;
    std::string pattern = dir() + "cfg_*.ini";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string fn(fd.cFileName);
            if (fn.rfind("cfg_", 0) == 0 && fn.size() > 8 &&
                fn.compare(fn.size() - 4, 4, ".ini") == 0) {
                std::string name = fn.substr(4, fn.size() - 8);
                out.push_back(name);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string Config::getAutoLoad() {
    std::ifstream f(dir() + "draxo_autoload.txt");
    if (!f.is_open()) return "";
    std::string name;
    std::getline(f, name);
    while (!name.empty() && (name.back() == '\r' || name.back() == '\n' || name.back() == ' ')) name.pop_back();
    return sanitize(name);  // konsistent mit listConfigs()-Namen vergleichbar
}

void Config::setAutoLoad(const std::string& name) {
    std::ofstream f(dir() + "draxo_autoload.txt");
    if (f.is_open()) f << sanitize(name);
    printf(STR_C("[Draxo] Auto-load config: '%s'\n"), sanitize(name).c_str());
}

// ── Per-server auto-load ────────────────────────────────────────────

std::string Config::bindsPath() { return dir() + "draxo_server_binds.txt"; }

void Config::bindServer(const std::string& ip, const std::string& name) {
    if (ip.empty() || name.empty()) return;
    std::vector<std::string> lines;
    { std::ifstream f(bindsPath()); std::string l;
      while (std::getline(f, l)) { if (!l.empty()) lines.push_back(l); } }
    std::string key = ip;
    std::string entry = key + "|" + sanitize(name);
    bool found = false;
    for (auto& l : lines) {
        auto bar = l.find('|');
        std::string k = (bar == std::string::npos) ? l : l.substr(0, bar);
        if (k == key) { l = entry; found = true; break; }
    }
    if (!found) lines.push_back(entry);
    std::ofstream f(bindsPath());
    for (auto& l : lines) f << l << "\n";
    printf(STR_C("[Draxo] Server-bind: '%s' -> '%s'\n"), key.c_str(), sanitize(name).c_str());
}

std::string Config::getServerBind(const std::string& ip) {
    std::ifstream f(bindsPath());
    std::string l;
    while (std::getline(f, l)) {
        auto bar = l.find('|');
        if (bar == std::string::npos) continue;
        if (l.substr(0, bar) == ip) return l.substr(bar + 1);
    }
    return "";
}

void Config::unbindServer(const std::string& ip) {
    std::vector<std::string> keep;
    { std::ifstream f(bindsPath()); std::string l;
      while (std::getline(f, l)) {
          auto bar = l.find('|');
          if (bar != std::string::npos && l.substr(0, bar) == ip) continue;
          if (!l.empty()) keep.push_back(l);
      } }
    std::ofstream f(bindsPath());
    for (auto& l : keep) f << l << "\n";
}

std::vector<std::pair<std::string,std::string>> Config::listServerBinds() {
    std::vector<std::pair<std::string,std::string>> out;
    std::ifstream f(bindsPath());
    std::string l;
    while (std::getline(f, l)) {
        auto bar = l.find('|');
        if (bar == std::string::npos) continue;
        out.emplace_back(l.substr(0, bar), l.substr(bar + 1));
    }
    return out;
}

std::string Config::currentName() {
    if (s_currentPath.empty()) return "default";
    std::string p = s_currentPath;
    std::string base = dir() + "cfg_";
    if (p.rfind(base, 0) == 0 && p.size() > base.size() + 4 &&
        p.compare(p.size() - 4, 4, ".ini") == 0)
        return p.substr(base.size(), p.size() - base.size() - 4);
    return "default";
}

// ── Export / Import ────────────────────────────────────────────────

std::string Config::exportAll() {
    std::string out;
    out += "# Draxo Config Export\n";
    out += "# " + currentName() + "\n\n";
    for (auto& [k, v] : s_data)
        out += k + "=" + v + "\n";
    return out;
}

bool Config::importString(const std::string& text, bool apply) {
    if (text.empty()) return false;
    // Minimal validation: at least one line with "="
    if (text.find('=') == std::string::npos) return false;

    std::unordered_map<std::string, std::string> newData;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
        while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
        if (!key.empty()) newData[key] = val;
    }

    if (newData.empty()) return false;

    // Apply: replace in-memory data + persist to current config file
    s_data = std::move(newData);
    save();
    printf(STR_C("[Draxo] Config imported: %zu entries\n"), s_data.size());
    return true;
}

// ── First-run detection ────────────────────────────────────────────

bool Config::isFresh() {
    std::ifstream f(getPath());
    return !f.is_open();  // no default config file yet
}

// ── Per-module preset name management ───────────────────────────────

std::vector<std::string> Config::getPresetNames(const std::string& mod) {
    auto it = s_data.find("preset." + mod + "._names");
    if (it == s_data.end() || it->second.empty()) return {};
    std::vector<std::string> out;
    std::istringstream ss(it->second);
    std::string item;
    while (std::getline(ss, item, ',')) {
        while (!item.empty() && item.front() == ' ') item.erase(item.begin());
        while (!item.empty() && item.back() == ' ') item.pop_back();
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

void Config::savePresetName(const std::string& mod, const std::string& name) {
    // Commas would break the comma-separated list format. Reject silently.
    if (name.find(',') != std::string::npos) return;
    auto names = getPresetNames(mod);
    for (auto& n : names) if (n == name) return;  // already present
    names.push_back(name);
    std::string joined;
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) joined += ",";
        joined += names[i];
    }
    s_data["preset." + mod + "._names"] = joined;
}

void Config::deletePresetName(const std::string& mod, const std::string& name) {
    auto names = getPresetNames(mod);
    names.erase(std::remove(names.begin(), names.end(), name), names.end());
    std::string joined;
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) joined += ",";
        joined += names[i];
    }
    if (names.empty())
        s_data.erase("preset." + mod + "._names");
    else
        s_data["preset." + mod + "._names"] = joined;
}

void Config::deletePresetData(const std::string& mod, const std::string& name) {
    std::string prefix = "preset." + mod + "." + name + ".";
    // Remove every key that starts with this prefix
    for (auto it = s_data.begin(); it != s_data.end(); ) {
        if (it->first.rfind(prefix, 0) == 0)
            it = s_data.erase(it);
        else
            ++it;
    }
}

// ── Accessors ───────────────────────────────────────────────────────

float Config::getFloat(const std::string& mod, const std::string& key, float def) {
    auto it = s_data.find(mod + "." + key);
    return it != s_data.end() ? (float)atof(it->second.c_str()) : def;
}
bool Config::getBool(const std::string& mod, const std::string& key, bool def) {
    auto it = s_data.find(mod + "." + key);
    return it != s_data.end() ? (it->second == "1" || it->second == "true") : def;
}
int Config::getInt(const std::string& mod, const std::string& key, int def) {
    auto it = s_data.find(mod + "." + key);
    return it != s_data.end() ? atoi(it->second.c_str()) : def;
}
ImU32 Config::getColor(const std::string& mod, const std::string& key, ImU32 def) {
    auto it = s_data.find(mod + "." + key);
    return it != s_data.end() ? (ImU32)strtoul(it->second.c_str(), nullptr, 16) : def;
}
std::string Config::getString(const std::string& mod, const std::string& key, const std::string& def) {
    auto it = s_data.find(mod + "." + key);
    return it != s_data.end() ? it->second : def;
}
void Config::setFloat(const std::string& mod, const std::string& key, float val) {
    s_data[mod + "." + key] = std::to_string(val);
}
void Config::setBool(const std::string& mod, const std::string& key, bool val) {
    s_data[mod + "." + key] = val ? "1" : "0";
}
void Config::setInt(const std::string& mod, const std::string& key, int val) {
    s_data[mod + "." + key] = std::to_string(val);
}
void Config::setColor(const std::string& mod, const std::string& key, ImU32 val) {
    char buf[16]; snprintf(buf, sizeof(buf), "%08X", val);
    s_data[mod + "." + key] = buf;
}
void Config::setString(const std::string& mod, const std::string& key, const std::string& val) {
    s_data[mod + "." + key] = val;
}
