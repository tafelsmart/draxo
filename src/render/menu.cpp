#include "pch.h"
#include "render/menu.h"
#include "render/renderer.h"
#include "render/notifications.h"
#include "modules/module_manager.h"
#include "core/cursor.h"
#include "core/config.h"
#include "core/animation.h"
#include "sdk/minecraft.h"

// ══════════════════════════════════════════════════════════════════
//  DRAXO CLIENT — Hauptmenü
// ══════════════════════════════════════════════════════════════════

// ── Farben ────────────────────────────────────────────────────────
#define T_ACCENT ImVec4(0.65f, 0.45f, 0.95f, 1.00f)
#define T_GREEN  ImVec4(0.35f, 0.88f, 0.35f, 1.00f)
#define T_RED    ImVec4(1.00f, 0.35f, 0.35f, 1.00f)
#define T_ORANGE ImVec4(1.00f, 0.55f, 0.10f, 1.00f)

// ── Tabs ──────────────────────────────────────────────────────────
struct TabInfo { const char* name; ModuleCategory cat; };
static const TabInfo kTabs[] = {
    {"COMBAT",   ModuleCategory::COMBAT},
    {"RENDER",   ModuleCategory::RENDER},
    {"MOVEMENT", ModuleCategory::MOVEMENT},
    {"PLAYER",   ModuleCategory::PLAYER},
    {"WORLD",    ModuleCategory::WORLD},
    {"CONFIG",   ModuleCategory::HUD},
};
static const int kTabCount = 6;

// ── Fenster-Geometrie ─────────────────────────────────────────────
static const float kWinW = 900.0f;   // Standard-Breite
static const float kWinH = 540.0f;   // Standard-Höhe
static const float kMinScale = 0.6f; // Untergrenze der Skalierung
static const float kMaxScale = 1.8f; // Obergrenze der Skalierung

// ── Tastenname ────────────────────────────────────────────────────
const char* vkName(int vk) {
    if (vk == 0) return "---";
    if (vk >= 'A' && vk <= 'Z') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
    if (vk >= '0' && vk <= '9') { static char b[2]; b[0] = (char)vk; b[1] = 0; return b; }
    switch (vk) {
        case VK_INSERT: return "INS";  case VK_DELETE: return "DEL";
        case VK_HOME:   return "HOME"; case VK_END:    return "END";
        case VK_SPACE:  return "SPC";  case VK_RETURN: return "ENT";
        case VK_TAB:    return "TAB";  case VK_ESCAPE: return "ESC";
        case VK_SHIFT:  return "SHFT"; case VK_CONTROL: return "CTRL"; case VK_MENU: return "ALT";
        case VK_F1:  return "F1";  case VK_F2:  return "F2";  case VK_F3:  return "F3";  case VK_F4:  return "F4";
        case VK_F5:  return "F5";  case VK_F6:  return "F6";  case VK_F7:  return "F7";  case VK_F8:  return "F8";
        case VK_F9:  return "F9";  case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
        default: return "...";
    }
}

// Text kürzen mit "..." wenn zu lang
static std::string clipText(const char* txt, float maxW) {
    std::string s(txt);
    if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
    while (s.size() > 3) {
        s.pop_back();
        if (ImGui::CalcTextSize((s + "...").c_str()).x <= maxW) return s + "...";
    }
    return s.substr(0, 3) + "...";
}

// Kleinbuchstaben-Helfer für Suche
static std::string toLower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// ── Zustand ───────────────────────────────────────────────────────
static AnimatedFloat s_animAlpha;
static bool s_closing = false;
static bool s_keybindCapturing = false;
static bool s_autoSave = false;
static bool s_maximized = false;
static int  s_activeTab = 0;

// ── Fenster-Zustand (Position / Größe, in Config speicherbar) ─────
static ImVec2 s_winPos(0, 0);
static ImVec2 s_winSize(kWinW, kWinH);
static bool   s_hasPos = false;   // nächsten Frame Pos/Size erzwingen
static bool   s_winCfgLoaded = false;

static void loadWinCfg() {
    if (s_winCfgLoaded) return;
    s_winCfgLoaded = true;
    s_autoSave = Config::getBool("Client", "auto_save", false);
    float x = Config::getFloat("Client", "win_x", -1.0f);
    float y = Config::getFloat("Client", "win_y", -1.0f);
    float w = Config::getFloat("Client", "win_w", kWinW);
    float h = Config::getFloat("Client", "win_h", kWinH);
    if (x >= 0 && y >= 0 && w >= 200 && h >= 200) {
        s_winPos = ImVec2(x, y);
        s_winSize = ImVec2(w, h);
        s_hasPos = true;
    }
}

static void saveWinCfg() {
    Config::setFloat("Client", "win_x", s_winPos.x);
    Config::setFloat("Client", "win_y", s_winPos.y);
    Config::setFloat("Client", "win_w", s_winSize.x);
    Config::setFloat("Client", "win_h", s_winSize.y);
    Config::save();
    Notifications::push("Window", "Position saved");
}

void Menu::renderWelcomeOverlay() {}

void Menu::toggle() {
    s_visible = !s_visible;
    if (s_visible) {
        Cursor::forceVisible();
        s_animAlpha.snap(0);
        s_animAlpha.setTarget(1, 0.15f, 0, Ease::outCubic);
    } else {
        s_visible = true;
        s_animAlpha.setTarget(0, 0.12f, 0, Ease::inCubic);
        s_closing = true;
    }
}

// ══════════════════════════════════════════════════════════════════
//  CONFIG-TAB
// ══════════════════════════════════════════════════════════════════

// Primäraktions-Button mit gefülltem Lila-Akzent (Save/Load/Bind...)
static bool accentButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.52f, 0.36f, 0.84f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.46f, 0.94f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.72f, 0.56f, 1.00f, 1.00f));
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return r;
}

static void configTab(float sf) {
    // ── Modus / Anzeige ──────────────────────────────────────────
    ImGui::Spacing();
    int ifm = Module::isSimpleMode() ? 0 : 1;
    ImGui::SetNextItemWidth(170 * sf);
    if (ImGui::Combo("##ifm", &ifm, "SIMPLE\0ADVANCED\0"))
        ModuleManager::setInterfaceMode(ifm);

    ImGui::SameLine();
    int fs = Renderer::getFontSize();
    ImGui::SetNextItemWidth(150 * sf);
    if (ImGui::SliderInt("##fs", &fs, 12, 28, "Font: %dpx"))
        Renderer::setFontSize(fs);

    ImGui::SameLine();
    bool snd = ModuleManager::soundEnabled();
    ImGui::Checkbox("Sounds", &snd); ModuleManager::soundEnabled() = snd;

    ImGui::SameLine();
    bool cur = Cursor::enabled();
    ImGui::Checkbox("Cursor", &cur); Cursor::enabled() = cur;

    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-Save", &s_autoSave)) {
        Config::setBool("Client", "auto_save", s_autoSave);
        Config::save();   // direkt persistieren
    }

    // ── Chat Bypass (X-Box restriction) ───────────────────────────
    ImGui::SameLine();
    if (auto* cb = ModuleManager::getModule("ChatBypass")) {
        bool chatOk = cb->isEnabled();
        if (ImGui::Checkbox("Chat Bypass", &chatOk)) {
            cb->setEnabled(chatOk);
            if (s_autoSave) { ModuleManager::saveAllSettings(); Config::save(); }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Erlaubt Chat auf X-Box-eingeschraenkten Accounts");
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Config speichern/laden ───────────────────────────────────
    static char cfg[64] = "";
    ImGui::SetNextItemWidth(220 * sf);
    ImGui::InputTextWithHint("##cfg", "Config name", cfg, sizeof(cfg));
    ImGui::SameLine();
    if (accentButton("Save As", ImVec2(90 * sf, 22 * sf)) && cfg[0]) {
        Config::saveAs(cfg);
        ModuleManager::saveAllSettings();
        Config::save();
        Notifications::push("Config", "Saved: " + std::string(cfg));
        memset(cfg, 0, 64);
    }
    ImGui::SameLine();
    if (accentButton("Load", ImVec2(70 * sf, 22 * sf)) && cfg[0]) {
        Config::loadNamed(cfg);
        ModuleManager::loadAllSettings();
        Notifications::push("Config", "Loaded: " + std::string(cfg));
        memset(cfg, 0, 64);
    }
    ImGui::SameLine();
    if (accentButton("Save All", ImVec2(90 * sf, 22 * sf))) {
        ModuleManager::saveAllSettings();
        Config::save();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Fenster-Position / -Größe ────────────────────────────────
    if (!s_maximized) {
        ImGui::Text("Window: %dx%d @ (%.0f, %.0f)",
                    (int)s_winSize.x, (int)s_winSize.y, s_winPos.x, s_winPos.y);
        if (accentButton("Save Window", ImVec2(130 * sf, 22 * sf))) saveWinCfg();
        ImGui::SameLine();
        if (ImGui::Button("Reset Window", ImVec2(130 * sf, 22 * sf))) {
            s_winSize = ImVec2(kWinW, kWinH);
            s_winPos = ImVec2(0, 0);   // wird unten zentriert
            s_hasPos = true;
            Notifications::push("Window", "Reset");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // ── Server-Bindung ───────────────────────────────────────────
    std::string ip = CMinecraft::getServerIp();
    ImGui::Text("Server: %s", ip.empty() ? "Singleplayer" : ip.c_str());
    if (!ip.empty() && ip != "singleplayer") {
        std::string b = Config::getServerBind(ip);
        if (!b.empty()) { ImGui::SameLine(); ImGui::TextColored(T_GREEN, "Auto: %s", b.c_str()); }
        ImGui::SameLine();
        if (accentButton("Bind", ImVec2(60 * sf, 22 * sf)) && cfg[0]) {
            Config::bindServer(ip, cfg);
            Config::save();
            Notifications::push("Config", "Bound");
            memset(cfg, 0, 64);
        }
        if (!b.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Unbind", ImVec2(70 * sf, 22 * sf))) { Config::unbindServer(ip); Config::save(); }
        }
    }
}

// ══════════════════════════════════════════════════════════════════
//  MODUL-ZEILE
// ══════════════════════════════════════════════════════════════════
static void renderModuleRow(Module* mod, float rowW, float sf) {
    if (!mod) return;
    ImGui::PushID(mod->getName().c_str());

    bool en = mod->isEnabled();
    ImU32 dotCol = en ? IM_COL32(160, 100, 240, 255) : IM_COL32(100, 100, 110, 180);
    ImU32 nameCol = en ? dotCol : IM_COL32(210, 210, 220, 210);

    ImVec2 rp = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rh = ImGui::GetTextLineHeight() + 6;

    // ── Rechte Gruppe KOORDINATEN zuerst berechnen ───────────────
    // Layout:  ... [Keybind] [(i)] [>]  (rechtsbündig, NICHT überlappend)
    const char* kn = vkName(mod->getKeyBind());
    float kw = ImGui::CalcTextSize(kn).x;          // Keybind-Textbreite (skaliert via Font)
    float rightEnd = rp.x + rowW - 6 * sf;
    float arrowX = rightEnd - 18 * sf;             // > Pfeil
    float infoX  = arrowX - 8 * sf - 20 * sf;      // (i) Button
    float keyX   = infoX - 8 * sf - kw;            // Keybind
    float rowEnd = keyX - 6 * sf;                  // Zeile endet VOR der Gruppe

    // ── Klickbare Zeile (Toggle) — überlappt die Gruppe NICHT ───
    ImGui::InvisibleButton("##row", ImVec2(rowEnd - rp.x, rh));
    if (ImGui::IsItemHovered())
        dl->AddRectFilled(rp, ImVec2(rowEnd, rp.y + rh), IM_COL32(255, 255, 255, 8), 4);
    if (ImGui::IsItemClicked()) {
        mod->toggle();
        Notifications::push(mod->getName(), mod->isEnabled() ? "ON" : "OFF");
        if (s_autoSave) { ModuleManager::saveAllSettings(); Config::save(); }
    }

    // > Pfeil (Einstellungen aufklappen)
    dl->AddText(ImVec2(arrowX, rp.y - 1), IM_COL32(190, 160, 230, 220), ">");
    ImGui::SetCursorScreenPos(ImVec2(arrowX, rp.y));
    static std::unordered_map<std::string, bool> s_exp;
    bool& ex = s_exp[mod->getName()];
    if (ImGui::InvisibleButton("##exp", ImVec2(18 * sf, rh))) ex = !ex;

    // (i) Info-Button — Info-Panel erscheint NEBEN der Zeile (rechts)
    // statt unterhalb, damit es die darunterliegenden Zeilen nicht verdeckt.
    dl->AddText(ImVec2(infoX, rp.y), IM_COL32(170, 130, 220, 170), "(i)");
    ImGui::SetCursorScreenPos(ImVec2(infoX, rp.y));
    ImGui::InvisibleButton("##info", ImVec2(20 * sf, rh));
    if (ImGui::IsItemHovered() && !mod->getDesc().empty()) {
        // Panel rechts neben (i) öffnen; falls kein Platz, nach links klappen
        float dispW = ImGui::GetIO().DisplaySize.x;
        float pw = 360.0f * sf;
        if (pw > dispW - 16) pw = dispW - 16;   // nie breiter als der Screen
        float px = infoX + 24 * sf;
        if (px + pw > dispW) px = infoX - pw - 24 * sf;
        if (px < 8) px = 8;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.03f, 0.10f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.40f, 0.90f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::SetNextWindowPos(ImVec2(px, rp.y), ImGuiCond_Always);
        ImGui::Begin("##infopanel", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PushTextWrapPos(pw);
        ImGui::TextColored(T_ACCENT, "%s", mod->getName().c_str());
        ImGui::TextWrapped("%s", mod->getDesc().c_str());
        ImGui::PopTextWrapPos();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    // Keybind-Label
    if (mod->getKeyBind())
        dl->AddText(ImVec2(keyX, rp.y + 1), IM_COL32(170, 150, 210, 180), kn);

    // ── Linke Seite: Dot + Name ─────────────────────────────────
    dl->AddCircleFilled(ImVec2(rp.x + 7, rp.y + rh * 0.5f), 3.0f, dotCol);
    float nameMax = rowEnd - 6 * sf - (rp.x + 16);
    std::string dname = clipText(mod->getName().c_str(), nameMax);
    dl->AddText(ImVec2(rp.x + 16, rp.y + 1), nameCol, dname.c_str());

    if (!ex) { ImGui::PopID(); return; }

    // ── Aufgeklappte Einstellungen ──────────────────────────────
    ImGui::Spacing();
    ImGui::SetNextItemWidth(rowW - 20);

    // Detect-Warnungen (nur Advanced)
    if (!Module::isSimpleMode() && mod->hasDetectWarnings()) {
        for (auto& w : mod->detectWarnings()) {
            if (!w.active) continue;
            ImGui::TextColored(T_RED, "⚠ %s", w.label.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextWrapped("%s", w.detail.c_str());
                ImGui::EndTooltip();
            }
        }
    }

    // Keybind / Reset / Hide HUD
    char kb[32]; snprintf(kb, sizeof(kb), "[%s]", vkName(mod->getKeyBind()));
    if (ImGui::Button(kb, ImVec2(60 * sf, 20 * sf))) s_keybindCapturing = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(60 * sf, 20 * sf))) {
        mod->resetToDefaults();
        if (Module::isSimpleMode() && mod->hasPresets()) mod->applyStoredPreset();
    }
    if (s_keybindCapturing) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Press a key...");
        // Ab VK_BACK starten: Maus-Buttons (1..5) & Sonder-Tasten ignorieren,
        // damit der Klick auf den Keybind-Button nicht sofort als Key erkannt wird.
        for (int k = VK_BACK; k < 256; k++) {
            if (GetAsyncKeyState(k) & 0x8000) {
                mod->setKeyBind(k == VK_ESCAPE ? 0 : k);
                s_keybindCapturing = false;
                break;
            }
        }
    }
    ImGui::SameLine();
    bool hid = mod->isHidden();
    ImGui::Checkbox("Hide HUD", &hid); mod->setHidden(hid);
    ImGui::Spacing();

    // ── Simple-Modus: Legit/Rage ────────────────────────────────
    if (Module::isSimpleMode() && mod->hasPresets()) {
        int p = mod->simplePreset();
        if (ImGui::Button("Legit (safe)", ImVec2(150 * sf, 22 * sf))) { mod->setSimplePreset(0); mod->applyStoredPreset(); }
        ImGui::SameLine(); if (p == 0) ImGui::TextColored(T_GREEN, "SAFE");
        ImGui::SameLine(340 * sf);
        if (ImGui::Button("Rage", ImVec2(150 * sf, 22 * sf))) { mod->setSimplePreset(1); mod->applyStoredPreset(); }
        ImGui::SameLine(); if (p == 1) ImGui::TextColored(T_ORANGE, "AGGRESSIVE");
        ImGui::Spacing();
    }

    // ── Advanced-Modus: alle Settings ───────────────────────────
    if (!Module::isSimpleMode()) {
        if (mod->hasPresets()) {
            ImGui::Text("Quick:"); ImGui::SameLine();
            if (ImGui::Button("Legit", ImVec2(70 * sf, 20 * sf))) { mod->setSimplePreset(0); mod->applyStoredPreset(); }
            ImGui::SameLine();
            if (ImGui::Button("Rage", ImVec2(70 * sf, 20 * sf))) { mod->setSimplePreset(1); mod->applyStoredPreset(); }
            ImGui::Spacing();
        }
        float half = (rowW - 40) * 0.5f;
        for (auto& [key, info] : mod->infoMap()) {
            ImGui::PushID(key.c_str());
            switch (info.type) {
                case SettingType::Bool: {
                    bool& v = mod->getBoolSetting(key);
                    ImGui::Checkbox(info.displayName.c_str(), &v);
                    break;
                }
                case SettingType::Float: {
                    float& v = mod->getFloatSetting(key);
                    ImGui::Text("%s", info.displayName.c_str());
                    ImGui::SetNextItemWidth(half);
                    ImGui::SliderFloat("##v", &v, info.min, info.max, info.format.c_str());
                    break;
                }
                case SettingType::Int: {
                    int& v = mod->intSetting(key);
                    ImGui::Text("%s", info.displayName.c_str());
                    ImGui::SetNextItemWidth(half);
                    ImGui::SliderInt("##v", &v, (int)info.min, (int)info.max, "%d");
                    break;
                }
                case SettingType::Mode: {
                    int& v = mod->intSetting(key);
                    if (!info.modeLabels.empty()) {
                        ImGui::Text("%s", info.displayName.c_str());
                        ImGui::SetNextItemWidth(half);
                        const char* pv = (v >= 0 && (size_t)v < info.modeLabels.size()) ? info.modeLabels[v] : "...";
                        if (ImGui::BeginCombo("##v", pv)) {
                            for (size_t j = 0; j < info.modeLabels.size(); j++)
                                if (ImGui::Selectable(info.modeLabels[j], (int)j == v)) v = (int)j;
                            ImGui::EndCombo();
                        }
                    }
                    break;
                }
                case SettingType::Color: {
                    ImU32& c = mod->getColorSetting(key);
                    float cl[4] = {
                        (float)((c >> 0) & 0xFF) / 255, (float)((c >> 8) & 0xFF) / 255,
                        (float)((c >> 16) & 0xFF) / 255, (float)((c >> 24) & 0xFF) / 255
                    };
                    ImGui::Text("%s", info.displayName.c_str());
                    ImGui::SameLine();
                    if (ImGui::ColorEdit4("##v", cl, ImGuiColorEditFlags_NoInputs))
                        c = IM_COL32((int)(cl[0] * 255), (int)(cl[1] * 255), (int)(cl[2] * 255), (int)(cl[3] * 255));
                    break;
                }
                default: break;
            }
            ImGui::PopID();
        }
    }
    ImGui::PopID();
}

// ══════════════════════════════════════════════════════════════════
//  HAUPT-RENDER
// ══════════════════════════════════════════════════════════════════
void Menu::render() {
    if (s_closing && s_animAlpha.value < 0.005f) { s_visible = false; s_closing = false; Cursor::forceHidden(); }
    if (!s_visible) { s_animAlpha.tick(); return; }
    float a = s_animAlpha.tick();
    if (a < 0.005f && s_closing) return;

    loadWinCfg();
    ImVec2 ds = ImGui::GetIO().DisplaySize;

    // ── Position / Größe setzen ─────────────────────────────────
    if (s_maximized) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(ds.x, ds.y));
    } else if (s_hasPos) {
        s_hasPos = false;
        // Zentrieren falls "Reset" oder erster Start ohne gespeicherte Pos
        if (s_winPos.x <= 0 && s_winPos.y <= 0)
            s_winPos = ImVec2((ds.x - s_winSize.x) * 0.5f, (ds.y - s_winSize.y) * 0.5f);
        ImGui::SetNextWindowPos(s_winPos);
        ImGui::SetNextWindowSize(s_winSize);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2(560, 380), ImVec2(ds.x, ds.y));

    // ── Basis-Styles (unabhängig von sf) ────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, s_maximized ? 0.0f : 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.02f, 0.10f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.02f, 0.10f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.40f, 0.90f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.38f, 0.22f, 0.58f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.52f, 0.34f, 0.75f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.60f, 0.42f, 0.92f, 0.45f));
    // ── Lila-Glass: Buttons, Inputs, Checkboxen ───────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.28f, 0.16f, 0.48f, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.40f, 0.26f, 0.64f, 0.38f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.48f, 0.32f, 0.76f, 0.48f));
    ImGui::PushStyleColor(ImGuiCol_Button,         ImVec4(0.30f, 0.18f, 0.50f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.44f, 0.28f, 0.70f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.54f, 0.38f, 0.86f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      ImVec4(0.72f, 0.50f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,        ImVec4(0.06f, 0.03f, 0.13f, 0.96f));

    // Im Fenster-Modus: verschiebbar + skalierbar; im Vollbild fix
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (s_maximized) winFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

    ImGui::Begin("##draxoMenu", &s_visible, winFlags);

    // ── ECHTE Fenstergröße lesen → Skalierung (kein Frame-Lag) ──
    // ww/wh/sf kommen NACH Begin aus der tatsächlichen Fenstergröße,
    // damit Layout + Schrift beim Resizen sofort (ohne 1-Frame-Versatz)
    // der echten Größe folgen und nie überlappen.
    ImVec2 curPos = ImGui::GetWindowPos();
    ImVec2 curSz  = ImGui::GetWindowSize();
    if (!s_maximized) { s_winPos = curPos; s_winSize = curSz; }

    float ww = curSz.x;
    float wh = curSz.y;
    float sf = ww / kWinW;
    if (sf > kMaxScale) sf = kMaxScale;
    if (sf < kMinScale) sf = kMinScale;
    ImGui::GetIO().FontGlobalScale = sf;

    // ── sf-abhängige Styles (nach Begin, mit aktueller Größe) ───
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6 * sf);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * sf, 4 * sf));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4 * sf, 3 * sf));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6 * sf);   // Buttons/Inputs/Combos
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 6 * sf);    // Slider-Griff
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 6 * sf);     // Tabs
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8 * sf);   // Dropdowns/Popups

    ImDrawList* dls = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float aw = ww - 20;                 // verfügbare Breite
    float hdrH = 30.0f * sf;            // Header-Höhe (skaliert)

    // ── Header ──────────────────────────────────────────────────
    float hdrRound = s_maximized ? 0.0f : 14.0f;
    int hdrFlags = s_maximized ? 0 : ImDrawFlags_RoundCornersTop;
    dls->AddRectFilled(wp, ImVec2(wp.x + ww, wp.y + hdrH), IM_COL32(12, 5, 28, 100), hdrRound, hdrFlags);

    // Maximize + Close Buttons rechtsbündig
    float btnY = (hdrH - 20 * sf) * 0.5f;
    float bx = ww - 20 - 6 * sf - 28 * sf;
    float bm = bx - 8 * sf - 28 * sf;

    // Verschieben per Drag
    if (!s_maximized) {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2(bm - 6 * sf, hdrH));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            ImVec2 cw = ImGui::GetWindowPos();
            float nx = cw.x + delta.x; if (nx < 0) nx = 0;
            float ny = cw.y + delta.y; if (ny < 0) ny = 0;
            ImGui::SetWindowPos(ImVec2(nx, ny));
        }
    }

    ImGui::SetCursorPos(ImVec2(14, hdrH * 0.5f - ImGui::GetTextLineHeight() * 0.5f));
    ImGui::TextColored(T_ACCENT, "DRAXO CLIENT");

    ImGui::SetCursorPos(ImVec2(bm, btnY));
    if (ImGui::Button("[]", ImVec2(28 * sf, 20 * sf))) {
        if (s_maximized) {
            s_maximized = false;
            s_hasPos = true;
        } else {
            s_maximized = true;
        }
    }
    ImGui::SetCursorPos(ImVec2(bx, btnY));
    if (ImGui::Button("X", ImVec2(28 * sf, 20 * sf))) Menu::toggle();

    // ── Tab-Bar ─────────────────────────────────────────────────
    ImGui::SetCursorPos(ImVec2(10, hdrH + 4 * sf));
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        for (int i = 0; i < kTabCount; i++) {
            if (ImGui::BeginTabItem(kTabs[i].name)) { s_activeTab = i; ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar();
    }

    // ── Modul-Liste ─────────────────────────────────────────────
    // Suche zuerst auswerten — steuert, was in der Liste erscheint
    static char sb[64] = "";
    bool searching = sb[0] != '\0';
    std::string searchQ = searching ? toLower(sb) : "";

    float listY = hdrH + 4 * sf + ImGui::GetFrameHeight() + 6;
    float ch = wh - listY - 44 * sf;
    // Child exakt im Content-Bereich (0..aw), damit die Scrollbar
    // nicht am rechten Rand abgeschnitten wird
    ImGui::SetCursorPos(ImVec2(0, listY));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.40f, 0.90f, 0.20f));
    // Scrollbar IMMER sichtbar — so wird klar, dass man scrollen kann
    ImGui::BeginChild("##list", ImVec2(aw, ch), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        auto& mods = ModuleManager::getModules();

        // Scrollbar-Breite freihalten, damit die rechte Gruppe
        // (Keybind/(i)/>) nie unter der Scrollbar liegt
        float sbw = ImGui::GetStyle().ScrollbarSize;
        float rowW = aw - sbw - 12;

        if (searching) {
            // Suche: Filtert ALLE Module über alle Kategorien
            for (auto& m : mods) {
                if (!m) continue;
                if (toLower(m->getName()).find(searchQ) == std::string::npos) continue;
                renderModuleRow(m.get(), rowW, sf);
            }
        } else if (s_activeTab == kTabCount - 1) {
            configTab(sf);
        } else {
            ModuleCategory cat = kTabs[s_activeTab].cat;
            for (auto& m : mods) {
                if (!m) continue;
                if (m->getCategory() != cat) continue;
                renderModuleRow(m.get(), rowW, sf);
            }
        }
        // Etwas Platz am Ende, damit die letzte Zeile nicht abgeschnitten wirkt
        ImGui::Dummy(ImVec2(0, 12 * sf));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Footer + Suche ──────────────────────────────────────────
    float fy = wh - 42 * sf;
    float ftrRound = s_maximized ? 0.0f : 14.0f;
    int ftrFlags = s_maximized ? 0 : ImDrawFlags_RoundCornersBottom;
    dls->AddRectFilled(ImVec2(wp.x, wp.y + fy), ImVec2(wp.x + ww, wp.y + wh),
                       IM_COL32(12, 5, 28, 120), ftrRound, ftrFlags);
    ImGui::SetCursorPos(ImVec2(14, fy + 10 * sf));
    ImGui::PushItemWidth(220 * sf);
    ImGui::InputTextWithHint("##search", "Search modules...", sb, sizeof(sb));
    ImGui::PopItemWidth();
    if (searching) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Searching all tabs...");
    }

    // Post-Begin Styles VOR End() zurücksetzen — ImGui prüft in End(),
    // ob der Style-Stack wieder auf dem Begin()-Stand ist. Sonst: Missing
    // PopStyleVar() in '##draxoMenu' (rotes ImGui-Debug-Fenster am Mauszeiger).
    ImGui::PopStyleVar(7);

    ImGui::End();

    // Pre-Begin Styles (Fenster-Design: Alpha, Padding, Farben) erst nach
    // End() aufräumen — sie gehören nicht zum Begin()-Snapshot.
    ImGui::PopStyleColor(14);
    ImGui::PopStyleVar(3);

    // IMMER zurücksetzen — sonst bleibt die Schrift beim Wechsel
    // von groß → klein riesig stehen.
    ImGui::GetIO().FontGlobalScale = 1.0f;
}

// ══════════════════════════════════════════════════════════════════
//  HUD-Grid (deaktiviert — von HUD::render referenziert)
// ══════════════════════════════════════════════════════════════════
bool Menu::isHudEditMode() { return false; }
bool Menu::hudShowGrid()   { return false; }
float Menu::hudGridSize()  { return 20.0f; }
