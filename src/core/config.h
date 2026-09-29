#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>

/*
 * Config — Persistent key/value store with multiple named configs.
 *
 *   - Default config:  <dll dir>/draxo_config.ini
 *   - Named configs:   <dll dir>/cfg_<name>.ini   (sanitized name)
 *   - Auto-load:       <dll dir>/draxo_autoload.txt  (contains config name)
 *
 * Keys are stored as "module.key". Everything the client knows how to
 * persist (module settings, keybinds, favorites, HUD layout, themes,
 * notifications, client options) is written through this class.
 */
class Config {
public:
    // ── Named config management ────────────────────────────────────
    static bool load();                              // auto-load config or default
    static bool save();                              // save to current config
    static bool saveAs(const std::string& name);     // save to <name> + make current
    static bool loadNamed(const std::string& name);  // load <name> + make current
    static bool saveDefault();                       // write to default file (no switch)
    static bool loadDefault();                       // force-load default file
    static bool deleteConfig(const std::string& name);
    static std::vector<std::string> listConfigs();   // all saved config names
    static std::string getAutoLoad();                // "" = none
    static void setAutoLoad(const std::string& name);// "" = clear
    static std::string currentName();                // "default" or named config

    // ── Per-server auto-load ───────────────────────────────────────
    // Datei: <dll dir>/draxo_server_binds.txt  (Zeilen: serverIp|configName)
    // Beim Joinen eines Servers wird die gebundene Config automatisch geladen.
    static void bindServer(const std::string& ip, const std::string& configName);
    static std::string getServerBind(const std::string& ip);   // "" = none
    static void unbindServer(const std::string& ip);
    static std::vector<std::pair<std::string,std::string>> listServerBinds();

    // ── Export / Import (shareable configs) ───────────────────────
    // exportAll() returns the full config as a .ini-format string.
    // importString() parses and applies a previously-exported config.
    static std::string exportAll();
    static bool importString(const std::string& text, bool apply = true);

    // ── First-run detection ────────────────────────────────────────
    // True when no default config file exists yet (fresh inject).
    static bool isFresh();

    // ── Per-module preset name management ──────────────────────────
    // Preset names are stored comma-separated under
    //   preset.<module>._names=name1,name2,...
    // while preset *values* are under
    //   preset.<module>.<name>.f.<key>._v=...
    // (see Module::savePreset / loadPreset / listPresets).
    static std::vector<std::string> getPresetNames(const std::string& mod);
    static void savePresetName(const std::string& mod, const std::string& name);
    static void deletePresetName(const std::string& mod, const std::string& name);
    static void deletePresetData(const std::string& mod, const std::string& name);

    // ── Accessors (module.key) ─────────────────────────────────────
    static float getFloat(const std::string& mod, const std::string& key, float def);
    static bool  getBool(const std::string& mod, const std::string& key, bool def);
    static int   getInt(const std::string& mod, const std::string& key, int def);
    static ImU32 getColor(const std::string& mod, const std::string& key, ImU32 def);
    static std::string getString(const std::string& mod, const std::string& key, const std::string& def);
    static void setFloat(const std::string& mod, const std::string& key, float val);
    static void setBool(const std::string& mod, const std::string& key, bool val);
    static void setInt(const std::string& mod, const std::string& key, int val);
    static void setColor(const std::string& mod, const std::string& key, ImU32 val);
    static void setString(const std::string& mod, const std::string& key, const std::string& val);

private:
    static std::string dir();
    static std::string bindsPath();                  // draxo_server_binds.txt
    static std::string getPath();                    // default config file
    static std::string pathFor(const std::string& name);
    static std::string sanitize(const std::string& name);
    static bool readFile(const std::string& path);   // fill s_data from file
    static bool writeFile(const std::string& path);  // flush s_data to file

    static inline std::string s_currentPath;
    static inline std::unordered_map<std::string, std::string> s_data;
};
