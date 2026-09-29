#include "pch.h"
#include "render/notifications.h"
#include "render/renderer.h"
#include "core/animation.h"
#include "core/config.h"
#include <cmath>

void Notifications::push(const std::string& title, const std::string& msg) {
    Notification n{title, msg, std::chrono::steady_clock::now(), 0.0f, false};
    s_queue.push_front(n);
    while ((int)s_queue.size() > s_maxVisible) s_queue.pop_back();
}

void Notifications::saveConfig() {
    Config::setInt("Notif", "style", (int)s_style);
    Config::setInt("Notif", "anim", (int)s_anim);
    Config::setFloat("Notif", "x", s_posX);
    Config::setFloat("Notif", "y", s_posY);
    Config::setFloat("Notif", "dur", s_duration);
    Config::setFloat("Notif", "spd", s_animSpeed);
    Config::setFloat("Notif", "w", s_maxWidth);
    Config::setInt("Notif", "max", s_maxVisible);
}
void Notifications::loadConfig() {
    s_style = (NotifStyle)Config::getInt("Notif", "style", 0);
    s_anim = (NotifAnim)Config::getInt("Notif", "anim", 0);
    s_posX = Config::getFloat("Notif", "x", 20);
    s_posY = Config::getFloat("Notif", "y", 0);
    s_duration = Config::getFloat("Notif", "dur", 3.0f);
    s_animSpeed = Config::getFloat("Notif", "spd", 0.15f);
    s_maxWidth = Config::getFloat("Notif", "w", 280.0f);
    s_maxVisible = Config::getInt("Notif", "max", 6);
}

// ── Style renderers ─────────────────────────────────────────────────
static float s_yStack = 0;

void Notifications::renderModern(const Notification& n, float alpha, float x, float y) {
    float w = s_maxWidth, h = s_notifHeight;
    ImVec2 pos(x, y);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32(12,14,18, (int)(230*alpha));
    ImU32 acc = IM_COL32(51,140,255, (int)(180*alpha));
    ImU32 txt = IM_COL32(220,225,240, (int)(255*alpha));
    ImU32 dim = IM_COL32(140,150,170, (int)(200*alpha));
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), bg, 8.0f);
    dl->AddRect(pos, ImVec2(pos.x+w, pos.y+h), acc, 8.0f, 0, 1.5f);
    dl->AddRectFilled(ImVec2(pos.x, pos.y+6), ImVec2(pos.x+3, pos.y+h-6), acc, 2.0f);
    dl->AddText(ImVec2(pos.x+14, pos.y+6), txt, n.title.c_str());
    dl->AddText(ImVec2(pos.x+14, pos.y+26), dim, n.message.c_str());
}

void Notifications::renderWindows(const Notification& n, float alpha, float x, float y) {
    float w = s_maxWidth, h = s_notifHeight;
    ImVec2 pos(x, y);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32(31,31,31, (int)(245*alpha));
    ImU32 titleBg = IM_COL32(0,120,212, (int)(255*alpha));
    ImU32 txt = IM_COL32(255,255,255, (int)(255*alpha));
    ImU32 dim = IM_COL32(180,180,180, (int)(220*alpha));
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), bg, 0.0f);
    dl->AddRect(pos, ImVec2(pos.x+w, pos.y+h), IM_COL32(80,80,80,(int)(200*alpha)), 0.0f, 0, 1.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+24), titleBg, 0.0f);
    dl->AddText(ImVec2(pos.x+10, pos.y+4), txt, n.title.c_str());
    dl->AddText(ImVec2(pos.x+10, pos.y+28), dim, n.message.c_str());
}

void Notifications::renderMacOS(const Notification& n, float alpha, float x, float y) {
    float w = s_maxWidth, h = s_notifHeight;
    ImVec2 pos(x, y);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32(30,32,36, (int)(235*alpha));
    ImU32 txt = IM_COL32(235,237,240, (int)(255*alpha));
    ImU32 dim = IM_COL32(160,165,175, (int)(210*alpha));
    ImU32 border = IM_COL32(60,63,70, (int)(150*alpha));
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), bg, 10.0f);
    dl->AddRect(pos, ImVec2(pos.x+w, pos.y+h), border, 10.0f, 0, 1.0f);
    dl->AddCircleFilled(ImVec2(pos.x+16, pos.y+12), 4.5f, IM_COL32(237,106,94,(int)(255*alpha)));
    dl->AddCircleFilled(ImVec2(pos.x+28, pos.y+12), 4.5f, IM_COL32(245,191,79,(int)(255*alpha)));
    dl->AddCircleFilled(ImVec2(pos.x+40, pos.y+12), 4.5f, IM_COL32(97,197,84,(int)(255*alpha)));
    dl->AddText(ImVec2(pos.x+16, pos.y+26), txt, n.title.c_str());
    dl->AddText(ImVec2(pos.x+16, pos.y+42), dim, n.message.c_str());
}

void Notifications::renderMaterial(const Notification& n, float alpha, float x, float y) {
    float w = s_maxWidth, h = s_notifHeight;
    ImVec2 pos(x, y);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32(28,30,35, (int)(240*alpha));
    ImU32 txt = IM_COL32(230,232,238, (int)(255*alpha));
    ImU32 dim = IM_COL32(150,155,165, (int)(210*alpha));
    ImU32 accent = IM_COL32(98,0,238, (int)(200*alpha));
    dl->AddRectFilled(ImVec2(pos.x+4, pos.y+4), ImVec2(pos.x+w+4, pos.y+h+4), IM_COL32(0,0,0,(int)(60*alpha)), 6.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), bg, 6.0f);
    dl->AddRectFilled(ImVec2(pos.x+6, pos.y+2), ImVec2(pos.x+w-6, pos.y+4), accent, 2.0f);
    dl->AddText(ImVec2(pos.x+12, pos.y+10), txt, n.title.c_str());
    dl->AddText(ImVec2(pos.x+12, pos.y+30), dim, n.message.c_str());
}

void Notifications::renderMinimal(const Notification& n, float alpha, float x, float y) {
    float w = s_maxWidth, h = s_notifHeight;
    ImVec2 pos(x, y);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 txt = IM_COL32(220,225,240, (int)(255*alpha));
    ImU32 dim = IM_COL32(140,150,170, (int)(200*alpha));
    ImU32 border = IM_COL32(70,75,85, (int)(120*alpha));
    dl->AddRect(pos, ImVec2(pos.x+w, pos.y+h), border, 2.0f, 0, 1.0f);
    dl->AddText(ImVec2(pos.x+8, pos.y+6), txt, n.title.c_str());
    dl->AddText(ImVec2(pos.x+8, pos.y+26), dim, n.message.c_str());
}

// ── Main render ─────────────────────────────────────────────────────

void Notifications::render() {
    if (s_queue.empty()) return;
    auto now = std::chrono::steady_clock::now();
    ImVec2 ds = ImGui::GetIO().DisplaySize;

    // Animate and cull
    for (size_t i = 0; i < s_queue.size(); i++) {
        auto& n = s_queue[i];
        float age = std::chrono::duration<float>(now - n.createdAt).count();
        float target = n.dismissing ? 0.0f : 1.0f;
        n.animProgress = Anim::smooth(n.animProgress, target, s_animSpeed);

        // Auto-dismiss
        if (!n.dismissing && age > s_duration) n.dismissing = true;
        if (n.dismissing && n.animProgress < 0.01f) {
            s_queue.erase(s_queue.begin() + i);
            i--; continue;
        }

        float alpha = n.animProgress;
        float offX = 0, offY = 0;

        // Standard-Anker: unten links (s_posY == 0 => automatisch unterer Rand)
        // Neueste Nachricht unten, ältere stapeln sich nach oben.
        float baseY = (s_posY <= 0.0f) ? (ds.y - 12.0f - s_notifHeight) : s_posY;
        float y = baseY - (float)i * (s_notifHeight + 8.0f);

        // Animation modifiers
        switch (s_anim) {
            case NotifAnim::SlideRight: offX = (1.0f - n.animProgress) * (s_maxWidth + 40); break;
            case NotifAnim::SlideUp:    offY = (1.0f - n.animProgress) * 60; break;
            case NotifAnim::Fade:       break;
            case NotifAnim::Pop:        alpha = n.animProgress * n.animProgress; break;
        }

        // Render
        switch (s_style) {
            case NotifStyle::Modern:   renderModern(n, alpha, s_posX + offX, y + offY); break;
            case NotifStyle::Windows:  renderWindows(n, alpha, s_posX + offX, y + offY); break;
            case NotifStyle::macOS:    renderMacOS(n, alpha, s_posX + offX, y + offY); break;
            case NotifStyle::Material: renderMaterial(n, alpha, s_posX + offX, y + offY); break;
            case NotifStyle::Minimal:  renderMinimal(n, alpha, s_posX + offX, y + offY); break;
        }
    }
}

// ── Config UI ───────────────────────────────────────────────────────

void Notifications::renderConfigUI() {
    ImGui::TextColored(ImVec4(0.20f, 0.55f, 1.00f, 1.00f), "NOTIFICATIONS");
    ImGui::Spacing();

    int si = (int)s_style;
    const char* styles[] = {"Modern", "Windows", "macOS", "Material", "Minimal"};
    if (ImGui::Combo("Style", &si, styles, 5)) s_style = (NotifStyle)si;

    int ai = (int)s_anim;
    const char* anims[] = {"Slide Right", "Slide Up", "Fade", "Pop"};
    if (ImGui::Combo("Animation", &ai, anims, 4)) s_anim = (NotifAnim)ai;

    ImGui::SliderFloat("Pos X", &s_posX, 0, 400, "%.0f");
    ImGui::SliderFloat("Pos Y (0 = unten links)", &s_posY, 0, 800, "%.0f");
    ImGui::SliderFloat("Duration", &s_duration, 1.0f, 8.0f, "%.1fs");
    ImGui::SliderFloat("Anim Speed", &s_animSpeed, 0.03f, 0.4f, "%.2f");
    ImGui::SliderFloat("Width", &s_maxWidth, 150.0f, 450.0f, "%.0f");
    ImGui::SliderInt("Max Visible", &s_maxVisible, 1, 10);

    if (ImGui::Button("Test", ImVec2(120, 24)))
        push("DRAXO", "Notification test!");
    ImGui::SameLine();
    ImGui::TextDisabled("Style: %s", notifStyleName(s_style));
}
