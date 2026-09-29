#include "pch.h"
#include "render/hud.h"
#include "render/renderer.h"
#include "render/menu.h"
#include "core/animation.h"
#include "core/config.h"
#include "core/version.h"
#include "modules/module_manager.h"
#include "sdk/minecraft.h"
#include "config/mappings.h"
#include <chrono>
#include <cmath>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════
// HUD Font Helper
// ═══════════════════════════════════════════════════════════════════════
static void hudText(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* text,
                    float fontSizeOverride = 0.0f) {
    ImFont* f = Renderer::getHudFont();
    if (!f) f = ImGui::GetFont();
    float fs = fontSizeOverride > 0 ? fontSizeOverride : Renderer::getHudFontSize();
    dl->AddText(f, fs, pos, col, text);
}

// ═══════════════════════════════════════════════════════════════════════
// Element Registry — Premium Layout (v6)
//
// Layout:
//   TOP LEFT (8px)           → FPS · Ping · TPS · Clock
//   TOP CENTER               → Server IP
//   TOP RIGHT (8px)          → DRAXO (big)
//   LEFT MID                 → ArrayList (active modules)
//   BOTTOM LEFT              → Coords · Keystrokes · CPS · Combo · Keybinds
//   BOTTOM RIGHT             → Session · Direction
// ═══════════════════════════════════════════════════════════════════════
static bool s_elementsInit = false;
static void initElements() {
    if (s_elementsInit) return; s_elementsInit = true;
    HUD::getElements().clear();

    // Default screen size used for initial positioning
    float sw = 1920.0f, sh = 1080.0f;

    // ── TOP LEFT — Stats (compact) ──────────────────────────────────
    #define EL(id, lbl, sec, x, y) HUD::getElements().push_back({id, lbl, sec, true, x, y})
    EL("##HudFPS",   "FPS",          "Stats",         8,   8);
    EL("##HudPing",  "Ping",         "Stats",         8,   38);
    EL("##HudTPS",   "TPS",          "Stats",         8,   68);
    EL("##HudClock", "Clock",        "Stats",         8,   98);

    // ── TOP CENTER — Server IP ─────────────────────────────────────
    EL("##HudIP",    "Server IP",    "Center",        sw/2-120, 8);
    EL("##HudPlrs",  "Players",      "Center",        sw/2-120, 38);

    // ── TOP RIGHT — DRAXO (big watermark) ──────────────────────────
    EL("##HudWM",    "DRAXO",        "Brand",         sw-160, 8);

    // ── LEFT MID — Active Modules List ─────────────────────────────
    EL("##HudAL",    "ArrayList",    "Left Side",     8,   160);

    // ── LEFT BOTTOM — Combat + World info ──────────────────────────
    EL("##HudCoord", "Coordinates",  "World",         8,   sh-70);
    EL("##HudKeys",  "Keystrokes",   "Combat",        8,   sh-260);
    EL("##HudCPS",   "CPS",          "Combat",        8,   sh-160);
    EL("##HudCombo", "Combo",        "Combat",        8,   sh-190);
    EL("##HudKB",    "Keybinds",     "Combat",        8,   sh-225);

    // ── BOTTOM RIGHT — Session + Direction ─────────────────────────
    EL("##HudSess",  "Session",      "Session",       sw-180, sh-60);
    EL("##HudDir",   "Direction",    "World",         sw-80,  sh-130);
    #undef EL

    // Load saved positions/config
    HUD::loadConfig();
}

// ═══════════════════════════════════════════════════════════════════════
// Config load/save — v6 = complete redesign with premium layout
// ═══════════════════════════════════════════════════════════════════════
static const int kHudVersion = 7;

void HUD::loadConfig() {
    int savedVer = Config::getInt("HUD", "_version", 0);
    bool resetAll = savedVer < kHudVersion;
    s_panelRounding = Config::getFloat("HUD", "_panelRounding", 8.0f);
    s_panelAlpha    = Config::getFloat("HUD", "_panelAlpha", 0.55f);
    s_panelShadow   = Config::getBool("HUD", "_panelShadow", true);
    s_panelBorder   = Config::getBool("HUD", "_panelBorder", true);
    for (auto& el : s_elements) {
        std::string n(el.label);
        if (resetAll) continue;  // Use fresh defaults from the premium layout
        el.visible     = Config::getBool(n, "_vis", el.visible);
        el.posX        = Config::getFloat(n, "_x", el.posX);
        el.posY        = Config::getFloat(n, "_y", el.posY);
        el.scale       = Config::getFloat(n, "_sc", el.scale);
        el.textColor   = Config::getColor(n, "_tc", el.textColor);
        el.bgColor     = Config::getColor(n, "_bg", el.bgColor);
        el.accentColor = Config::getColor(n, "_ac", el.accentColor);
        el.panelMode   = Config::getBool(n, "_pm", el.panelMode);
    }
    if (resetAll) {
        Config::setInt("HUD", "_version", kHudVersion);
        saveConfig();
    }
}

void HUD::saveConfig() {
    Config::setFloat("HUD", "_panelRounding", s_panelRounding);
    Config::setFloat("HUD", "_panelAlpha", s_panelAlpha);
    Config::setBool("HUD", "_panelShadow", s_panelShadow);
    Config::setBool("HUD", "_panelBorder", s_panelBorder);
    Config::setInt("HUD", "_version", kHudVersion);
    for (auto& el : s_elements) {
        std::string n(el.label);
        Config::setBool(n, "_vis", el.visible);
        Config::setFloat(n, "_x", el.posX);
        Config::setFloat(n, "_y", el.posY);
        Config::setFloat(n, "_sc", el.scale);
        Config::setColor(n, "_tc", el.textColor);
        Config::setColor(n, "_bg", el.bgColor);
        Config::setColor(n, "_ac", el.accentColor);
        Config::setBool(n, "_pm", el.panelMode);
    }
}

HudElement* HUD::getElement(const char* id) {
    for (auto& el : s_elements)
        if (strcmp(el.id, id) == 0) return &el;
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Drag System + Snaplines
// ═══════════════════════════════════════════════════════════════════════
static bool s_dragActive = false;
static float s_dragOffX = 0, s_dragOffY = 0;
static HudElement* s_dragEl = nullptr;

bool HUD::beginElement(HudElement& el, float w, float h) {
    // Smooth fade in/out
    el.animAlpha = Anim::smooth(el.animAlpha, el.visible ? 1.0f : 0.0f, 0.10f);
    el.animScale = Anim::smooth(el.animScale, 1.0f, 0.08f);
    if (el.animAlpha < 0.01f) return false;

    float sx = el.scale * el.animScale;
    ImVec2 pos(el.posX, el.posY);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w * sx, h * sx), ImGuiCond_Always);

    // Transparent window — we draw everything manually with DrawList
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    bool open = true;
    ImGui::Begin(el.id, &open,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ── Drag detection with snaplines ───────────────────────────────
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ms = ImGui::GetMousePos();
    bool hover = (ms.x >= wp.x && ms.x <= wp.x + w*sx && ms.y >= wp.y && ms.y <= wp.y + h*sx);
    bool canDrag = Menu::isHudEditMode() || !Menu::isVisible();

    if (hover && ImGui::IsMouseClicked(0) && !s_dragActive && canDrag) {
        s_dragActive = true; s_dragEl = &el;
        s_dragOffX = ms.x - wp.x; s_dragOffY = ms.y - wp.y;
    }
    if (s_dragActive && s_dragEl == &el) {
        if (ImGui::IsMouseDown(0)) {
            float nx = ms.x - s_dragOffX;
            float ny = ms.y - s_dragOffY;
            float ew = w * sx, eh = h * sx;
            ImVec2 ds = ImGui::GetIO().DisplaySize;
            const float SNAP = 10.0f;

            // Grid snap (edit mode)
            if (Menu::isHudEditMode() && Menu::hudShowGrid()) {
                float gs = Menu::hudGridSize();
                nx = roundf(nx / gs) * gs;
                ny = roundf(ny / gs) * gs;
            }

            // Screen edge snap
            if (fabs(nx) < SNAP) nx = 0;
            if (fabs((nx + ew) - ds.x) < SNAP) nx = ds.x - ew;
            if (fabs(ny) < SNAP) ny = 0;
            if (fabs((ny + eh) - ds.y) < SNAP) ny = ds.y - eh;

            // Center-X snap
            if (fabs(nx + ew/2 - ds.x/2) < SNAP) nx = ds.x/2 - ew/2;

            // Center-Y snap
            if (fabs(ny + eh/2 - ds.y/2) < SNAP) ny = ds.y/2 - eh/2;

            // Thirds snap (vertical thirds guide lines)
            if (fabs(nx - ds.x/3) < SNAP) nx = ds.x/3;
            if (fabs(nx - ds.x*2/3) < SNAP) nx = ds.x*2/3;
            if (fabs(nx + ew - ds.x/3) < SNAP) nx = ds.x/3 - ew;
            if (fabs(nx + ew - ds.x*2/3) < SNAP) nx = ds.x*2/3 - ew;

            el.posX = nx; el.posY = ny;
        } else { s_dragActive = false; s_dragEl = nullptr; }
    }
    return true;
}

void HUD::endElement() {
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// ═══════════════════════════════════════════════════════════════════════
// Panel Renderer — blurred background with shadow + accent border
// ═══════════════════════════════════════════════════════════════════════
void HUD::drawPanel(const HudElement& el, float w, float h) {
    if (!el.panelMode) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el.scale * el.animScale;
    float r = s_panelRounding * sx;

    // Shadow
    if (s_panelShadow) {
        ImU32 shadow = IM_COL32(0, 0, 0, (int)(80 * el.animAlpha));
        dl->AddRectFilled(ImVec2(p.x + 3*sx, p.y + 3*sx),
                          ImVec2(p.x + w*sx + 3*sx, p.y + h*sx + 3*sx),
                          shadow, r);
    }

    // Background fill
    ImU32 bg = (ImU32)((el.bgColor & 0x00FFFFFF) |
                ((ImU32)((el.bgColor >> 24) * s_panelAlpha * el.animAlpha) << 24));
    dl->AddRectFilled(p, ImVec2(p.x + w*sx, p.y + h*sx), bg, r);

    // Accent border (left side, 2px wide)
    if (s_panelBorder) {
        ImU32 acc = (ImU32)((el.accentColor & 0x00FFFFFF) |
                    ((ImU32)((el.accentColor >> 24) * el.animAlpha) << 24));
        float bw = 2.5f * sx;
        dl->AddRectFilled(ImVec2(p.x, p.y + r*0.3f),
                          ImVec2(p.x + bw, p.y + h*sx - r*0.3f),
                          acc, bw * 0.5f);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// ELEMENT RENDERERS — Premium Layout
// ═══════════════════════════════════════════════════════════════════════

// ── TOP RIGHT: DRAXO Watermark (big clean text) ──────────────────
void HUD::renderWatermark() {
    auto* el = getElement("##HudWM"); if (!el || !el->visible) return;
    float w = 150, h = 40;
    if (!beginElement(*el, w, h)) return;
    drawPanel(*el, w, h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 acc = (ImU32)((el->accentColor & 0x00FFFFFF) |
                ((ImU32)((el->accentColor >> 24) * el->animAlpha) << 24));

    // Big clean DRAXO — centered in panel
    float bigSize = 30.0f * sx;
    ImFont* f = Renderer::getHudFont();
    if (!f) f = ImGui::GetFont();
    ImVec2 ts = f->CalcTextSizeA(bigSize, FLT_MAX, 0.0f, "DRAXO");
    dl->AddText(f, bigSize,
                ImVec2(p.x + w*sx/2 - ts.x/2, p.y + h*sx/2 - ts.y/2),
                acc, "DRAXO");

    ImGui::Dummy(ImVec2(w, h));
    endElement();
}

// ── TOP LEFT: FPS ────────────────────────────────────────────────────
void HUD::renderFPS() {
    auto* el = getElement("##HudFPS"); if (!el || !el->visible) return;
    if (!beginElement(*el, 90, 26)) return;
    drawPanel(*el, 90, 26);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int)s_fpsSmooth);
    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "FPS", fs);
    hudText(dl, ImVec2(p.x + 42*sx, p.y + 3*sx), txt, buf, fs);

    ImGui::Dummy(ImVec2(90, 26));
    endElement();
}

// ── TOP LEFT: Ping ───────────────────────────────────────────────────
void HUD::renderPing() {
    auto* el = getElement("##HudPing"); if (!el || !el->visible) return;
    if (!beginElement(*el, 90, 26)) return;
    drawPanel(*el, 90, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    // Get ping
    int ping = 0;
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        jobject pl = CMinecraft::getPlayer();
        if (pl) {
            static jfieldID s_ping = nullptr;
            static bool s_pingAttempted = false;
            // Cache the lookup (incl. failures): without this, a missing field
            // throws NoSuchFieldError + leaks a class local EVERY FRAME.
            if (!s_ping && !s_pingAttempted) {
                s_pingAttempted = true;
                jclass pp = env->GetObjectClass(pl);
                s_ping = env->GetFieldID(pp, "latency", "I");
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_ping = nullptr; }
                env->DeleteLocalRef(pp);
            }
            if (s_ping) ping = env->GetIntField(pl, s_ping);
            env->DeleteLocalRef(pl);
        }
    }

    char buf[16]; snprintf(buf, sizeof(buf), "%d", ping);
    ImU32 col = ping < 50  ? IM_COL32(80, 220, 80, (int)(255*el->animAlpha))
              : ping < 150 ? IM_COL32(220, 200, 80, (int)(255*el->animAlpha))
              :              IM_COL32(220, 80, 80, (int)(255*el->animAlpha));

    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "PING", fs);
    hudText(dl, ImVec2(p.x + 44*sx, p.y + 3*sx), col, buf, fs);

    ImGui::Dummy(ImVec2(90, 26));
    endElement();
}

// ── TOP LEFT: TPS ────────────────────────────────────────────────────
void HUD::renderTPS() {
    auto* el = getElement("##HudTPS"); if (!el || !el->visible) return;
    if (!beginElement(*el, 90, 26)) return;
    drawPanel(*el, 90, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    float tps = 20.0f;
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            static jfieldID s_timer = nullptr, s_tps = nullptr;
            static bool s_timerAttempted = false;
            // Cache the lookup (incl. failures): MC_timer is missing on some
            // versions and retrying it every frame throws+clears a JNI
            // exception 60x/sec and leaks the class local ref each time.
            if (!s_timer && !s_timerAttempted) {
                s_timerAttempted = true;
                jclass mcC = env->GetObjectClass(mc);
                s_timer = env->GetFieldID(mcC,
                    Mappings::MC_timer, Mappings::MC_timer_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_timer = nullptr; }
                env->DeleteLocalRef(mcC);
            }
            if (s_timer) {
                jobject t = env->GetObjectField(mc, s_timer);
                if (t) {
                    if (!s_tps) {
                        jclass tC = env->GetObjectClass(t);
                        s_tps = env->GetFieldID(tC,
                            Mappings::Timer_msPerTick, Mappings::Timer_msPerTick_Sig);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); s_tps = nullptr; }
                    }
                    if (s_tps) {
                        float mspt = env->GetFloatField(t, s_tps);
                        if (mspt > 0) tps = 1000.0f / mspt;
                    }
                    env->DeleteLocalRef(t);
                }
            }
            env->DeleteLocalRef(mc);
        }
    }

    char buf[16]; snprintf(buf, sizeof(buf), "%.1f", tps);
    ImU32 col = tps > 18 ? IM_COL32(80, 220, 80, (int)(255*el->animAlpha))
                         : IM_COL32(220, 80, 80, (int)(255*el->animAlpha));

    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "TPS", fs);
    hudText(dl, ImVec2(p.x + 38*sx, p.y + 3*sx), col, buf, fs);

    ImGui::Dummy(ImVec2(90, 26));
    endElement();
}

// ── TOP CENTER: Clock ────────────────────────────────────────────────
void HUD::renderClock() {
    auto* el = getElement("##HudClock"); if (!el || !el->visible) return;
    if (!beginElement(*el, 90, 26)) return;
    drawPanel(*el, 90, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf; localtime_s(&tm_buf, &t);
    char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                           tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    float fs = 20.0f * sx;
    ImFont* cf = Renderer::getHudFont();
    if (!cf) cf = ImGui::GetFont();
    float tw = cf->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf).x;
    hudText(dl, ImVec2(p.x + 45*sx - tw/2, p.y + 3*sx), txt, buf, fs);

    ImGui::Dummy(ImVec2(90, 26));
    endElement();
}

// ── TOP CENTER: Server IP ────────────────────────────────────────
void HUD::renderServerIP() {
    auto* el = getElement("##HudIP"); if (!el || !el->visible) return;
    if (!beginElement(*el, 260, 26)) return;
    drawPanel(*el, 260, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));

    std::string ipStr = CMinecraft::getServerIp();
    if (ipStr == "singleplayer") ipStr = "Singleplayer";

    // Center text in panel
    float fs = 18.0f * sx;
    ImFont* rf = Renderer::getHudFont();
    if (!rf) rf = ImGui::GetFont();
    ImVec2 ts = rf->CalcTextSizeA(fs, FLT_MAX, 0.0f, ipStr.c_str());
    hudText(dl, ImVec2(p.x + 130*sx - ts.x/2, p.y + 3*sx), txt, ipStr.c_str(), fs);

    ImGui::Dummy(ImVec2(260, 26));
    endElement();
}

// ── TOP RIGHT: Player Count ──────────────────────────────────────────
void HUD::renderPlayerCount() {
    auto* el = getElement("##HudPlrs"); if (!el || !el->visible) return;
    if (!beginElement(*el, 70, 26)) return;
    drawPanel(*el, 70, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    // Get player list size
    int count = 0;
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        jobject mc = CMinecraft::getInstance();
        if (mc) {
            static jmethodID s_getServer = nullptr;
            static jfieldID s_sdList = nullptr;
            static jfieldID s_mcList = nullptr;
            static bool init = false;
            if (!init) {
                jclass mcC = env->GetObjectClass(mc);
                s_getServer = env->GetMethodID(mcC,
                    Mappings::MC_getCurrentServer, Mappings::MC_getCurrentServer_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_getServer = nullptr; }
                s_mcList = env->GetFieldID(mcC,
                    Mappings::MC_playerList, Mappings::MC_playerList_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_mcList = nullptr; }
                env->DeleteLocalRef(mcC);
                jclass sdC = JvmWrapper::findClass(Mappings::ServerData_Class);
                if (sdC) {
                    s_sdList = env->GetFieldID(sdC,
                        Mappings::ServerData_playerList, Mappings::ServerData_playerList_Sig);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); s_sdList = nullptr; }
                    env->DeleteLocalRef(sdC);
                }
                init = true;
            }
            jobject plist = nullptr;
            // Modern (1.21.2+): Minecraft.getCurrentServer().playerList
            if (s_getServer && s_sdList) {
                jobject sd = env->CallObjectMethod(mc, s_getServer);
                if (sd) {
                    plist = env->GetObjectField(sd, s_sdList);
                    env->DeleteLocalRef(sd);
                }
            }
            // Legacy (< 1.21.2): Minecraft.playerList
            if (!plist && s_mcList) plist = env->GetObjectField(mc, s_mcList);
            if (plist) {
                jclass colC = env->GetObjectClass(plist);
                jmethodID sizeM = env->GetMethodID(colC, "size", "()I");
                if (sizeM) count = env->CallIntMethod(plist, sizeM);
                env->DeleteLocalRef(plist);
            }
            env->DeleteLocalRef(mc);
        }
    }

    char buf[16];
    if (count > 0) snprintf(buf, sizeof(buf), "%d", count);
    else snprintf(buf, sizeof(buf), "—");

    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "PLR", fs);
    // Right-align the value
    ImFont* rf = Renderer::getHudFont();
    if (!rf) rf = ImGui::GetFont();
    float vw = rf->CalcTextSizeA(fs, FLT_MAX, 0.0f, buf).x;
    hudText(dl, ImVec2(p.x + 70*sx - 6*sx - vw, p.y + 3*sx), txt, buf, fs);

    ImGui::Dummy(ImVec2(70, 26));
    endElement();
}

// ── LEFT MID: ArrayList ──────────────────────────────────────────────
void HUD::renderArrayList() {
    auto* el = getElement("##HudAL"); if (!el) return;
    auto& mods = ModuleManager::getModules();

    // Show: enabled modules OR modules with a keybind set
    auto showRow = [](Module* m) {
        if (!m || m->isHidden()) return false;
        return m->isEnabled() || m->getKeyBind() != 0;
    };

    // Collect visible entries + sort by text length (shortest first)
    struct Entry { Module* mod; std::string label; };
    std::vector<Entry> entries;
    for (auto& m : mods) {
        if (!showRow(m.get())) continue;
        std::string lbl = m->getName();
        if (m->getKeyBind()) { lbl += " ["; lbl += vkName(m->getKeyBind()); lbl += "]"; }
        entries.push_back({m.get(), lbl});
    }
    if (entries.empty()) return;
    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.label.size() < b.label.size(); });

    int n = (int)entries.size();

    // Calculate width dynamically from longest entry
    float maxNameW = 120.0f;
    ImFont* f = Renderer::getHudFont();
    float fSize = Renderer::getHudFontSize();
    for (auto& e : entries) {
        float tw = f ? f->CalcTextSizeA(fSize, FLT_MAX, 0.0f, e.label.c_str()).x
                     : ImGui::CalcTextSize(e.label.c_str()).x;
        if (tw + 30.0f > maxNameW) maxNameW = tw + 30.0f;
    }
    float rowH = fSize + 6.0f;
    float h = n * rowH + 12.0f;

    if (!beginElement(*el, maxNameW, h)) return;
    drawPanel(*el, maxNameW, h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 140, 158, (int)(170 * el->animAlpha));

    float y = p.y + 6*sx;
    for (auto& e : entries) {
        bool active = e.mod->isEnabled() && !e.mod->isHidden();

        // Active = lila/purple dot + text, Inactive with hotkey = gray
        ImU32 col = active
            ? IM_COL32(160, 100, 240, 255)   // lila
            : dim;                             // gray

        dl->AddCircleFilled(ImVec2(p.x + 10*sx, y + rowH/2*sx),
            active ? 3.5f*sx : 2.5f*sx, col);

        hudText(dl, ImVec2(p.x + 22*sx, y), col, e.label.c_str());
        y += rowH * sx;
    }

    ImGui::Dummy(ImVec2(maxNameW, h));
    endElement();
}

// ── LEFT BOTTOM: Coordinates + Direction + Biome ─────────────────────
void HUD::renderCoords() {
    auto* el = getElement("##HudCoord"); if (!el || !el->visible) return;
    auto cam = CMinecraft::getCameraData(); if (!cam.valid) return;

    if (!beginElement(*el, 240, 52)) return;
    drawPanel(*el, 240, 52);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(140, 150, 170, (int)(200 * el->animAlpha));

    // XYZ line
    char buf[64];
    snprintf(buf, sizeof(buf), "XYZ  %.1f / %.1f / %.1f", cam.x, cam.y, cam.z);
    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 2*sx), txt, buf, fs);

    // Direction
    const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    float yaw = fmod(fmod(cam.yaw, 360.0f) + 360.0f + 22.5f, 360.0f);
    if (!(yaw >= 0.0f)) yaw = 0.0f;
    int idx = ((int)(yaw / 45.0f)) % 8;
    if (idx < 0 || idx > 7) idx = 0;

    // Get biome
    std::string biome = "Overworld";
    // Biome lookup (placeholder — full JNI biome reading would slow down rendering)
    int bx = (int)floor(cam.x), bz = (int)floor(cam.z);

    snprintf(buf, sizeof(buf), "%s  %s  %d, %d", dirs[idx], biome.c_str(), bx, bz);
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 26*sx), dim, buf, 14.0f * sx);

    ImGui::Dummy(ImVec2(240, 52));
    endElement();
}

// ── LEFT BOTTOM: Direction (standalone) ──────────────────────────────
void HUD::renderDirection() {
    auto* el = getElement("##HudDir"); if (!el || !el->visible) return;
    auto cam = CMinecraft::getCameraData(); if (!cam.valid) return;

    if (!beginElement(*el, 60, 28)) return;
    drawPanel(*el, 60, 28);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 acc = (ImU32)((el->accentColor & 0x00FFFFFF) |
                ((ImU32)((el->accentColor >> 24) * el->animAlpha) << 24));

    const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
    float yaw = fmod(fmod(cam.yaw, 360.0f) + 360.0f + 22.5f, 360.0f);
    if (!(yaw >= 0.0f)) yaw = 0.0f;
    int idx = ((int)(yaw / 45.0f)) % 8;
    if (idx < 0 || idx > 7) idx = 0;

    float fs = 22.0f * sx;
    ImFont* f = Renderer::getHudFont();
    ImVec2 ts = f ? f->CalcTextSizeA(fs, FLT_MAX, 0.0f, dirs[idx])
                  : ImGui::CalcTextSize(dirs[idx]);
    hudText(dl, ImVec2(p.x + 30*sx - ts.x/2, p.y + 14*sx - ts.y/2), acc, dirs[idx], fs);

    ImGui::Dummy(ImVec2(60, 28));
    endElement();
}

// ── Keystrokes ───────────────────────────────────────────────────────
void HUD::renderKeystrokes() {
    auto* el = getElement("##HudKeys"); if (!el || !el->visible) return;
    if (!beginElement(*el, 70, 92)) return;
    drawPanel(*el, 70, 92);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 on  = (ImU32)((el->accentColor & 0x00FFFFFF) |
                ((ImU32)((el->accentColor >> 24) * el->animAlpha) << 24));
    ImU32 off = IM_COL32(35, 38, 48, (int)(200 * el->animAlpha));
    ImU32 txt = IM_COL32(180, 190, 210, (int)(200 * el->animAlpha));
    ImFont* kf = Renderer::getHudFont() ? Renderer::getHudFont() : ImGui::GetFont();
    float kfSize = Renderer::getHudFontSize();

    auto key = [&](float x, float y, float kw, float kh, bool pressed, const char* label) {
        dl->AddRectFilled(ImVec2(p.x + x*sx, p.y + y*sx),
                          ImVec2(p.x + (x+kw)*sx, p.y + (y+kh)*sx),
                          pressed ? on : off, 3*sx);
        ImVec2 ts = kf->CalcTextSizeA(kfSize, FLT_MAX, 0.0f, label);
        dl->AddText(kf, kfSize,
                    ImVec2(p.x + (x+kw/2)*sx - ts.x/2, p.y + (y+kh/2)*sx - ts.y/2),
                    txt, label);
    };

    bool w = (GetAsyncKeyState('W')&0x8000), a = (GetAsyncKeyState('A')&0x8000);
    bool s = (GetAsyncKeyState('S')&0x8000), d = (GetAsyncKeyState('D')&0x8000);
    key(22, 2, 24, 22, w, "W");
    key(2, 28, 24, 22, a, "A");
    key(22, 28, 24, 22, s, "S");
    key(42, 28, 24, 22, d, "D");
    bool sp = (GetAsyncKeyState(VK_SPACE)&0x8000);
    key(4, 54, 60, 14, sp, "SPACE");
    bool lmb = (GetAsyncKeyState(VK_LBUTTON)&0x8000), rmb = (GetAsyncKeyState(VK_RBUTTON)&0x8000);
    key(4, 72, 30, 16, lmb, "LMB");
    key(36, 72, 30, 16, rmb, "RMB");

    ImGui::Dummy(ImVec2(70, 92));
    endElement();
}

// ── CPS Counter (moved to combat section) ────────────────────────────
void HUD::renderCPS() {
    auto* el = getElement("##HudCPS"); if (!el || !el->visible) return;
    if (!beginElement(*el, 80, 26)) return;
    drawPanel(*el, 80, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    // Track clicks
    static bool wasLmb = false; bool lmb = (GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if (lmb && !wasLmb) s_leftClicks++;
    wasLmb = lmb;

    char buf[16]; snprintf(buf, sizeof(buf), "%d", s_realCPS);
    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "CPS", fs);
    hudText(dl, ImVec2(p.x + 38*sx, p.y + 3*sx), txt, buf, fs);

    ImGui::Dummy(ImVec2(80, 26));
    endElement();
}

// ── Combo Counter ────────────────────────────────────────────────────
void HUD::renderCombo() {
    auto* el = getElement("##HudCombo"); if (!el || !el->visible) return;
    if (!beginElement(*el, 90, 26)) return;
    drawPanel(*el, 90, 26);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));
    ImU32 acc = (ImU32)((el->accentColor & 0x00FFFFFF) |
                ((ImU32)((el->accentColor >> 24) * el->animAlpha) << 24));

    char buf[16]; snprintf(buf, sizeof(buf), "%d", s_comboCount);
    ImU32 col = s_comboCount >= 10 ? acc : txt;

    float fs = 18.0f * sx;
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 3*sx), dim, "COMBO", fs);
    hudText(dl, ImVec2(p.x + 54*sx, p.y + 3*sx), col, buf, fs);

    ImGui::Dummy(ImVec2(90, 26));
    endElement();
}

// ── LEFT BOTTOM: Keybind List ─────────────────────────────────────
void HUD::renderKeybindList() {
    auto* el = getElement("##HudKB"); if (!el || !el->visible) return;
    auto& mods = ModuleManager::getModules();

    // Collect all modules that have a keybind
    struct KB { std::string label; bool active; int vk; ModuleCategory cat; };
    std::vector<KB> binds;
    for (auto& m : mods) {
        if (!m) continue;
        int vk = m->getKeyBind();
        if (!vk) continue;
        std::string lbl = m->getName();
        lbl += " ["; lbl += vkName(vk); lbl += "]";
        binds.push_back({lbl, m->isEnabled(), vk, m->getCategory()});
    }
    if (binds.empty()) return;

    // Sort: enabled first, then by key
    std::sort(binds.begin(), binds.end(), [](const KB& a, const KB& b) {
        if (a.active != b.active) return a.active > b.active;
        return a.vk < b.vk;
    });

    int n = (int)binds.size();
    float maxW = 160.0f;
    ImFont* f = Renderer::getHudFont();
    float fSize = Renderer::getHudFontSize();
    for (auto& b : binds) {
        float tw = f ? f->CalcTextSizeA(fSize, FLT_MAX, 0.0f, b.label.c_str()).x
                     : ImGui::CalcTextSize(b.label.c_str()).x;
        if (tw + 20.0f > maxW) maxW = tw + 20.0f;
    }
    float rowH = fSize + 4.0f;
    float h = n * rowH + 10.0f;

    if (!beginElement(*el, maxW, h)) return;
    drawPanel(*el, maxW, h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 on  = (ImU32)((el->accentColor & 0x00FFFFFF) |
                ((ImU32)((el->accentColor >> 24) * el->animAlpha) << 24));
    ImU32 off = IM_COL32(100, 108, 125, (int)(180 * el->animAlpha));
    ImU32 keyClr = IM_COL32(180, 190, 215, (int)(200 * el->animAlpha));

    float y = p.y + 5*sx;
    for (auto& b : binds) {
        // Active indicator dot
        ImU32 dotCol;
        if (b.active) {
            switch (b.cat) {
                case ModuleCategory::COMBAT:   dotCol = IM_COL32(255, 80, 80, 255); break;
                case ModuleCategory::MOVEMENT: dotCol = IM_COL32(80, 200, 120, 255); break;
                case ModuleCategory::RENDER:   dotCol = IM_COL32(140, 100, 255, 255); break;
                case ModuleCategory::PLAYER:   dotCol = IM_COL32(255, 180, 80, 255); break;
                default: dotCol = IM_COL32(120, 140, 200, 255); break;
            }
            dl->AddCircleFilled(ImVec2(p.x + 8*sx, y + rowH/2*sx), 3.0f*sx, dotCol);
        } else {
            dl->AddCircleFilled(ImVec2(p.x + 8*sx, y + rowH/2*sx), 2.5f*sx, off);
        }

        ImU32 txtCol = b.active ? on : off;
        hudText(dl, ImVec2(p.x + 18*sx, y), txtCol, b.label.c_str());
        y += rowH * sx;
    }

    ImGui::Dummy(ImVec2(maxW, h));
    endElement();
}

// ── BOTTOM RIGHT: Session Info ───────────────────────────────────────
void HUD::renderSession() {
    auto* el = getElement("##HudSess"); if (!el || !el->visible) return;
    if (!beginElement(*el, 170, 52)) return;
    drawPanel(*el, 170, 52);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    float sx = el->scale * el->animScale;
    ImU32 txt = (ImU32)((el->textColor & 0x00FFFFFF) |
                ((ImU32)((el->textColor >> 24) * el->animAlpha) << 24));
    ImU32 dim = IM_COL32(130, 145, 170, (int)(180 * el->animAlpha));

    // Session time
    auto now = std::chrono::steady_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(now - s_sessionStart).count();
    int hh = (int)(secs/3600), mm = (int)((secs%3600)/60), ss = (int)(secs%60);

    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 2*sx), dim, "TIME", 14.0f*sx);
    hudText(dl, ImVec2(p.x + 42*sx, p.y + 2*sx), txt, buf, 14.0f*sx);

    // Kills / Deaths / KD
    float kd = s_deaths > 0 ? (float)s_kills / s_deaths : (float)s_kills;
    snprintf(buf, sizeof(buf), "K %d  D %d  KD %.1f", s_kills, s_deaths, kd);
    hudText(dl, ImVec2(p.x + 6*sx, p.y + 28*sx), txt, buf, 12.0f*sx);

    ImGui::Dummy(ImVec2(170, 52));
    endElement();
}

// ═══════════════════════════════════════════════════════════════════════
// Main Render Loop
// ═══════════════════════════════════════════════════════════════════════
void HUD::render() {
    initElements();

    // FPS tracking
    s_frameCount++;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastFPS).count();
    if (elapsed >= 500) {
        s_realFPS = (int)(s_frameCount * 1000.0 / elapsed);
        s_realCPS = s_leftClicks * 2;
        s_leftClicks = 0;
        s_frameCount = 0;
        s_lastFPS = now;
    }
    s_fpsSmooth = Anim::smooth(s_fpsSmooth, (float)s_realFPS, 0.12f);
    s_cpsSmooth = Anim::smooth(s_cpsSmooth, (float)s_realCPS, 0.12f);

    // Render all elements in layout order
    renderWatermark();     // TOP RIGHT: DRAXO
    renderFPS();           // TOP LEFT
    renderPing();          // TOP LEFT
    renderTPS();           // TOP LEFT
    renderClock();         // TOP LEFT
    renderServerIP();      // TOP CENTER
    renderPlayerCount();   // TOP CENTER
    renderArrayList();     // LEFT MID
    renderCoords();        // BOTTOM LEFT
    renderKeystrokes();    // BOTTOM LEFT
    renderCPS();           // BOTTOM LEFT
    renderCombo();         // BOTTOM LEFT
    renderKeybindList();   // BOTTOM LEFT
    renderSession();       // BOTTOM RIGHT
    renderDirection();     // BOTTOM RIGHT
}

// ═══════════════════════════════════════════════════════════════════════
// Config UI — HUD Settings Panel (shown in menu's CONFIG tab)
// ═══════════════════════════════════════════════════════════════════════
void HUD::renderConfigUI() {
    initElements();
    ImGui::TextColored(ImVec4(0.20f, 0.55f, 1.00f, 1.00f), "HUD EDITOR");
    ImGui::Spacing();
    ImGui::TextDisabled("Drag elements in-game to reposition. Use the sliders below for fine-tuning.");
    ImGui::Spacing();

    // ── Global Panel Defaults ───────────────────────────────────────
    ImGui::Text("PANEL DEFAULTS"); ImGui::Spacing();
    ImGui::SliderFloat("Rounding", &s_panelRounding, 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Alpha", &s_panelAlpha, 0.05f, 1.0f, "%.2f");
    ImGui::Checkbox("Shadow", &s_panelShadow);
    ImGui::SameLine(120); ImGui::Checkbox("Border", &s_panelBorder);
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Quick Actions ───────────────────────────────────────────────
    if (ImGui::Button("Save All", ImVec2(100, 24))) saveConfig();
    ImGui::SameLine();
    if (ImGui::Button("Reset All", ImVec2(100, 24))) {
        // Reset all positions to fresh defaults
        s_elementsInit = false;
        s_elements.clear();
        initElements();
        saveConfig();
    }
    ImGui::SameLine();
    if (ImGui::Button("Panels On", ImVec2(90, 24))) {
        for (auto& el : s_elements) el.panelMode = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Panels Off", ImVec2(90, 24))) {
        for (auto& el : s_elements) el.panelMode = false;
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // ── Per-Element Settings (grouped by section) ───────────────────
    const char* lastSection = nullptr;
    for (auto& el : s_elements) {
        // Section header
        if (!lastSection || strcmp(el.section, lastSection) != 0) {
            if (lastSection) { ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); }
            ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.80f, 1.00f), "%s", el.section);
            ImGui::Spacing();
            lastSection = el.section;
        }

        ImGui::PushID(el.id);

        // Visibility toggle (right side)
        ImGui::Text("%s", el.label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
        bool vis = el.visible;
        if (ImGui::Checkbox("##vis", &vis)) el.visible = vis;

        if (el.visible) {
            ImGui::Indent(12);

            // Panel mode toggle
            ImGui::Checkbox("Panel", &el.panelMode);

            // Scale
            ImGui::SliderFloat("Scale", &el.scale, 0.5f, 3.0f, "%.1f");

            // Text color
            float tc[4] = {
                ((el.textColor >> 0) & 0xFF) / 255.0f,
                ((el.textColor >> 8) & 0xFF) / 255.0f,
                ((el.textColor >> 16) & 0xFF) / 255.0f,
                ((el.textColor >> 24) & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit4("Text", tc,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                el.textColor = IM_COL32((int)(tc[0]*255),(int)(tc[1]*255),
                                        (int)(tc[2]*255),(int)(tc[3]*255));

            // Accent color
            float ac[4] = {
                ((el.accentColor >> 0) & 0xFF) / 255.0f,
                ((el.accentColor >> 8) & 0xFF) / 255.0f,
                ((el.accentColor >> 16) & 0xFF) / 255.0f,
                ((el.accentColor >> 24) & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit4("Accent", ac,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                el.accentColor = IM_COL32((int)(ac[0]*255),(int)(ac[1]*255),
                                          (int)(ac[2]*255),(int)(ac[3]*255));

            ImGui::Unindent(12);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }
}
