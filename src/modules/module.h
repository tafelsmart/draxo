#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "core/config.h"

enum class ModuleCategory : int {
    COMBAT, MOVEMENT, RENDER, PLAYER, WORLD, EXPLOIT, MISC, HUD
};

inline const char* categoryName(ModuleCategory cat) {
    switch (cat) {
        case ModuleCategory::COMBAT:   return "COMBAT";
        case ModuleCategory::MOVEMENT: return "MOVEMENT";
        case ModuleCategory::RENDER:   return "RENDER";
        case ModuleCategory::PLAYER:   return "PLAYER";
        case ModuleCategory::WORLD:    return "WORLD";
        case ModuleCategory::EXPLOIT:  return "EXPLOIT";
        case ModuleCategory::MISC:     return "MISC";
        case ModuleCategory::HUD:      return "HUD";
        default: return "UNKNOWN";
    }
}

enum class SettingType : int {
    Bool, Float, Int, Color, String, Keybind, Mode
};

struct SettingInfo {
    SettingType type = SettingType::Bool;
    std::string displayName;
    float min = 0, max = 1, step = 0.1f;
    std::string format = "%.1f";
    std::vector<const char*> modeLabels;
};

class Module {
public:
    Module(const std::string& name, ModuleCategory cat, int keyBind,
           const std::string& desc = "")
        : m_name(name), m_category(cat), m_keyBind(keyBind),
          m_defaultKeyBind(keyBind), m_description(desc) {}

    virtual ~Module() = default;

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onUpdate(JNIEnv* env) {}
    virtual void onRender() {}

    int tickInterval() const { return m_tickInterval; }
    void setTickInterval(int i) { m_tickInterval = i; }

    void toggle() {
        m_enabled = !m_enabled;
        if (m_enabled) onEnable(); else onDisable();
    }
    void setEnabled(bool v) {
        if (v != m_enabled) { m_enabled = v; if (m_enabled) onEnable(); else onDisable(); }
    }

    bool isEnabled()            const { return m_enabled; }
    const std::string& getName() const { return m_name; }
    ModuleCategory getCategory()  const { return m_category; }
    int  getKeyBind()             const { return m_keyBind; }
    void setKeyBind(int kb)       { m_keyBind = kb; }
    int  getDefaultKeyBind()      const { return m_defaultKeyBind; }
    const std::string& getDesc()  const { return m_description; }

    bool isHidden()  const { return m_hidden; }
    void setHidden(bool h) { m_hidden = h; }

    bool isFavorite() const { return m_favorite; }
    void setFavorite(bool f) { m_favorite = f; }

    const std::vector<std::string>& searchTags() const { return m_searchTags; }
    void addSearchTag(const std::string& tag) { m_searchTags.push_back(tag); }

    // ── Interface Mode (Simple / Advanced) ────────────────────────────
    static int& interfaceMode() { return s_interfaceMode; }
    static bool isSimpleMode()  { return s_interfaceMode == 0; }

    int  simplePreset() const   { return m_simplePreset; }
    void setSimplePreset(int p) { m_simplePreset = p; }
    const char* simplePresetName() const { return m_simplePreset == 0 ? "Legit" : "Rage"; }

    bool hasPresets() const;
    void applyPreset(const char* name);
    void applyStoredPreset();

    // ── Detection Warnings ──────────────────────────────────────────
    struct DetectWarning {
        std::string label; bool active = false; std::string detail;
    };
    void addDetectWarning(const std::string& label, bool cond, const std::string& detail) {
        if (cond) m_detectWarnings.push_back({label, true, detail});
    }
    const auto& detectWarnings() const { return m_detectWarnings; }
    void clearDetectWarnings() { m_detectWarnings.clear(); }
    bool hasDetectWarnings() const { return !m_detectWarnings.empty(); }

    // ── Settings ────────────────────────────────────────────────────
    float& floatSetting(const std::string& key) { return m_floatSettings[key]; }
    bool&  boolSetting(const std::string& key)  { return m_boolSettings[key]; }
    ImU32& colorSetting(const std::string& key) { return m_colorSettings[key]; }
    int&   intSetting(const std::string& key)   { return m_intSettings[key]; }
    std::string& stringSetting(const std::string& key) { return m_stringSettings[key]; }
    float& getFloatSetting(const std::string& k) { return floatSetting(k); }
    bool&  getBoolSetting(const std::string& k)  { return boolSetting(k); }
    ImU32& getColorSetting(const std::string& k) { return colorSetting(k); }

    SettingInfo& settingInfo(const std::string& key) { return m_settingInfos[key]; }

    void defineFloat(const std::string& key, const std::string& label,
                     float def, float min, float max, const char* fmt = "%.1f") {
        m_floatSettings[key] = def; m_defaultFloats[key] = def;
        m_settingInfos[key] = {SettingType::Float, label, min, max, 0.1f, fmt};
        m_displayNames[key] = label; _addToGroup(key);
    }
    void defineBool(const std::string& key, const std::string& label, bool def = false) {
        m_boolSettings[key] = def; m_defaultBools[key] = def;
        m_settingInfos[key] = {SettingType::Bool, label};
        m_displayNames[key] = label; _addToGroup(key);
    }
    void defineInt(const std::string& key, const std::string& label,
                   int def, int min, int max) {
        m_intSettings[key] = def; m_defaultInts[key] = def;
        m_settingInfos[key] = {SettingType::Int, label, (float)min, (float)max, 1.0f, "%.0f"};
        m_displayNames[key] = label; _addToGroup(key);
    }
    void defineMode(const std::string& key, const std::string& label,
                    int def, const std::vector<const char*>& labels) {
        m_intSettings[key] = def; m_defaultInts[key] = def;
        m_settingInfos[key] = {SettingType::Mode, label, 0, (float)(labels.size()-1), 1.0f, "%s"};
        m_settingInfos[key].modeLabels = labels;
        m_displayNames[key] = label; _addToGroup(key);
    }
    void defineColor(const std::string& key, const std::string& label, ImU32 def) {
        m_colorSettings[key] = def; m_defaultColors[key] = def;
        m_settingInfos[key] = {SettingType::Color, label};
        m_displayNames[key] = label; _addToGroup(key);
    }
    void defineString(const std::string& key, const std::string& label, const std::string& def) {
        m_stringSettings[key] = def; m_defaultStrings[key] = def;
        m_settingInfos[key] = {SettingType::String, label};
        m_displayNames[key] = label; _addToGroup(key);
    }

    void setBoolSetting(const std::string& k, bool v) { m_boolSettings[k] = v; }
    void setFloatSetting(const std::string& k, float v) { m_floatSettings[k] = v; }
    void setIntSetting(const std::string& k, int v) { m_intSettings[k] = v; }

    void setDisplayName(const std::string& key, const std::string& name) { m_displayNames[key] = name; }
    std::string displayName(const std::string& key) const {
        auto it = m_displayNames.find(key);
        return it != m_displayNames.end() ? it->second : key;
    }

    void defineGroup(const std::string& name) { m_currentGroup = name; }
    void defineGroupEnd()                     { m_currentGroup.clear(); }
    const auto& groups()       const { return m_settingGroups; }
    const auto& groupFor(const std::string& key) const {
        static std::string empty;
        auto it = m_settingToGroup.find(key);
        return it != m_settingToGroup.end() ? it->second : empty;
    }
    bool hasGroups() const { return !m_settingGroups.empty(); }

    const auto& floatMap() const { return m_floatSettings; }
    const auto& boolMap()  const { return m_boolSettings; }
    const auto& colorMap() const { return m_colorSettings; }
    const auto& intMap()   const { return m_intSettings; }
    const auto& stringMap()const { return m_stringSettings; }
    const auto& infoMap()  const { return m_settingInfos; }
    auto& floatSettings() { return m_floatSettings; }
    auto& boolSettings()  { return m_boolSettings; }
    auto& intSettings()   { return m_intSettings; }
    const auto& floatSettings() const { return m_floatSettings; }
    const auto& boolSettings()  const { return m_boolSettings; }
    const auto& intSettings()   const { return m_intSettings; }

    void resetToDefaults() {
        for (auto& [k, v] : m_defaultFloats) m_floatSettings[k] = v;
        for (auto& [k, v] : m_defaultBools)  m_boolSettings[k]  = v;
        for (auto& [k, v] : m_defaultInts)   m_intSettings[k]   = v;
        for (auto& [k, v] : m_defaultColors)  m_colorSettings[k]  = v;
        for (auto& [k, v] : m_defaultStrings) m_stringSettings[k] = v;
        m_keyBind = m_defaultKeyBind;
        m_hidden = false;
    }

    void writeToConfig() {
        Config::setInt(m_name, "_keybind", m_keyBind);
        Config::setBool(m_name, "_hidden", m_hidden);
        Config::setBool(m_name, "_favorite", m_favorite);
        Config::setBool(m_name, "_enabled", m_enabled);
        Config::setInt(m_name, "_simplePreset", m_simplePreset);
        for (auto& [k, v] : m_floatSettings)  Config::setFloat(m_name, "f." + k, v);
        for (auto& [k, v] : m_boolSettings)   Config::setBool(m_name, "b." + k, v);
        for (auto& [k, v] : m_intSettings)    Config::setInt(m_name, "i." + k, v);
        for (auto& [k, v] : m_colorSettings)  Config::setColor(m_name, "c." + k, v);
        for (auto& [k, v] : m_stringSettings) Config::setString(m_name, "s." + k, v);
    }

    void readFromConfig() {
        int kb = Config::getInt(m_name, "_keybind", m_keyBind);
        if (kb) setKeyBind(kb);
        setHidden(Config::getBool(m_name, "_hidden", m_hidden));
        setFavorite(Config::getBool(m_name, "_favorite", m_favorite));
        m_simplePreset = Config::getInt(m_name, "_simplePreset", m_simplePreset);
        for (auto& [k, v] : m_floatSettings)  m_floatSettings[k]  = Config::getFloat(m_name, "f." + k, v);
        for (auto& [k, v] : m_boolSettings)   m_boolSettings[k]   = Config::getBool(m_name, "b." + k, v);
        for (auto& [k, v] : m_intSettings)    m_intSettings[k]    = Config::getInt(m_name, "i." + k, v);
        for (auto& [k, v] : m_colorSettings)  m_colorSettings[k]  = Config::getColor(m_name, "c." + k, v);
        for (auto& [k, v] : m_stringSettings) m_stringSettings[k] = Config::getString(m_name, "s." + k, v);
    }

    bool savedEnabled() const { return m_savedEnabled; }
    void setSavedEnabled(bool e) { m_savedEnabled = e; }

    void savePreset(const std::string& presetName) {
        std::string prefix = std::string("preset.") + m_name + "." + presetName + ".";
        for (auto& [k, v] : m_floatSettings)  Config::setFloat(prefix + "f." + k, "_v", v);
        for (auto& [k, v] : m_boolSettings)   Config::setBool(prefix + "b." + k, "_v", v);
        for (auto& [k, v] : m_intSettings)    Config::setInt(prefix + "i." + k, "_v", v);
        for (auto& [k, v] : m_colorSettings)  Config::setColor(prefix + "c." + k, "_v", v);
        Config::savePresetName(m_name, presetName);
    }

    void loadPreset(const std::string& presetName) {
        std::string prefix = std::string("preset.") + m_name + "." + presetName + ".";
        for (auto& [k, v] : m_floatSettings)
            m_floatSettings[k] = Config::getFloat(prefix + "f." + k, "_v", v);
        for (auto& [k, v] : m_boolSettings)
            m_boolSettings[k] = Config::getBool(prefix + "b." + k, "_v", v);
        for (auto& [k, v] : m_intSettings)
            m_intSettings[k] = Config::getInt(prefix + "i." + k, "_v", v);
        for (auto& [k, v] : m_colorSettings)
            m_colorSettings[k] = Config::getColor(prefix + "c." + k, "_v", v);
    }

    void deletePreset(const std::string& presetName) {
        Config::deletePresetData(m_name, presetName);
        Config::deletePresetName(m_name, presetName);
    }

    std::vector<std::string> listPresets() const {
        std::vector<std::string> out;
        out.push_back("Legit");
        out.push_back("Rage");
        auto names = Config::getPresetNames(m_name);
        for (auto& n : names) {
            if (n != "Legit" && n != "Rage") out.push_back(n);
        }
        return out;
    }

protected:
    std::string      m_name;
    ModuleCategory   m_category = ModuleCategory::MISC;
    int              m_keyBind  = 0;
    int              m_defaultKeyBind = 0;
    std::string      m_description;
    bool             m_enabled  = false;
    bool             m_hidden   = false;
    bool             m_favorite = false;
    bool             m_savedEnabled = false;
    int              m_simplePreset = 0;
    static inline int s_interfaceMode = 1;
    int              m_tickInterval = 1;
    std::vector<std::string> m_searchTags;
    std::vector<DetectWarning> m_detectWarnings;

    std::unordered_map<std::string, float>  m_floatSettings;
    std::unordered_map<std::string, bool>   m_boolSettings;
    std::unordered_map<std::string, ImU32>  m_colorSettings;
    std::unordered_map<std::string, int>    m_intSettings;
    std::unordered_map<std::string, std::string> m_stringSettings;

    std::unordered_map<std::string, float>  m_defaultFloats;
    std::unordered_map<std::string, bool>   m_defaultBools;
    std::unordered_map<std::string, int>    m_defaultInts;
    std::unordered_map<std::string, ImU32>  m_defaultColors;
    std::unordered_map<std::string, std::string> m_defaultStrings;

    std::unordered_map<std::string, SettingInfo> m_settingInfos;
    std::unordered_map<std::string, std::string> m_displayNames;

    std::string m_currentGroup;
    std::vector<std::string> m_settingGroups;
    std::unordered_map<std::string, std::string> m_settingToGroup;
    void _addToGroup(const std::string& key) {
        if (m_currentGroup.empty()) return;
        m_settingToGroup[key] = m_currentGroup;
        if (std::find(m_settingGroups.begin(), m_settingGroups.end(), m_currentGroup) == m_settingGroups.end())
            m_settingGroups.push_back(m_currentGroup);
    }
};
