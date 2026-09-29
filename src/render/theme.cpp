#include "pch.h"
#include "render/theme.h"
#include "core/config.h"
#include "render/menu.h"

static constexpr int kThemeVersion = 5;

void ThemeEngine::init() {
    s_defaults = s_theme;
    loadFromConfig();
}

void ThemeEngine::applyToImGui() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = s_theme.windowRounding;
    style.FrameRounding     = s_theme.frameRounding;
    style.GrabRounding      = s_theme.frameRounding;
    style.PopupRounding     = s_theme.popupRounding;
    style.ScrollbarRounding = s_theme.scrollRounding;
    style.TabRounding       = s_theme.frameRounding;
    style.WindowPadding     = ImVec2(s_theme.windowPaddingX, s_theme.windowPaddingY);
    style.FramePadding      = ImVec2(s_theme.framePaddingX, s_theme.framePaddingY);
    style.ItemSpacing       = ImVec2(s_theme.itemSpacingX, s_theme.itemSpacingY);
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.WindowMinSize     = ImVec2(420, 320);

    colors[ImGuiCol_WindowBg]         = s_theme.bgPanel;
    colors[ImGuiCol_ChildBg]          = s_theme.bgContent;
    colors[ImGuiCol_PopupBg]          = ImVec4(s_theme.bgPanel.x, s_theme.bgPanel.y, s_theme.bgPanel.z, 0.98f);
    colors[ImGuiCol_Border]           = s_theme.border;
    colors[ImGuiCol_BorderShadow]     = ImVec4(0,0,0,0);
    colors[ImGuiCol_Text]             = s_theme.textPrimary;
    colors[ImGuiCol_TextDisabled]     = s_theme.textSecondary;
    colors[ImGuiCol_TextSelectedBg]   = ImVec4(s_theme.accent.x, s_theme.accent.y, s_theme.accent.z, 0.35f);
    colors[ImGuiCol_CheckMark]        = s_theme.accent;
    colors[ImGuiCol_SliderGrab]       = s_theme.accent;
    colors[ImGuiCol_SliderGrabActive] = s_theme.accentHover;
    colors[ImGuiCol_NavHighlight]     = s_theme.accent;
    colors[ImGuiCol_FrameBg]          = ImVec4(0.17f,0.18f,0.21f,1.00f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.22f,0.24f,0.28f,1.00f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.27f,0.29f,0.34f,1.00f);
    colors[ImGuiCol_Button]           = ImVec4(0.20f,0.22f,0.26f,1.00f);
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.28f,0.31f,0.37f,1.00f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(s_theme.accentDim.x, s_theme.accentDim.y, s_theme.accentDim.z, 0.9f);
    colors[ImGuiCol_Header]           = ImVec4(0.18f,0.20f,0.24f,1.00f);
    colors[ImGuiCol_HeaderHovered]    = ImVec4(0.26f,0.29f,0.35f,1.00f);
    colors[ImGuiCol_HeaderActive]     = ImVec4(0.32f,0.36f,0.44f,1.00f);
    colors[ImGuiCol_Tab]              = ImVec4(0.14f,0.15f,0.18f,1.00f);
    colors[ImGuiCol_TabHovered]       = ImVec4(0.24f,0.27f,0.33f,1.00f);
    colors[ImGuiCol_TabActive]        = s_theme.accent;
    colors[ImGuiCol_TabUnfocused]     = ImVec4(0.14f,0.15f,0.18f,1.00f);
    colors[ImGuiCol_TabUnfocusedActive]=ImVec4(0.20f,0.30f,0.45f,1.00f);
    colors[ImGuiCol_TitleBg]          = ImVec4(0.08f,0.09f,0.11f,1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.12f,0.14f,0.17f,1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f,0.09f,0.11f,1.00f);
    colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.08f,0.09f,0.11f,0.60f);
    colors[ImGuiCol_ScrollbarGrab]    = s_theme.scrollbar;
    colors[ImGuiCol_ScrollbarGrabHovered]=s_theme.scrollbarHover;
    colors[ImGuiCol_ScrollbarGrabActive] =s_theme.accent;
    colors[ImGuiCol_Separator]        = s_theme.separator;
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.45f,0.49f,0.58f,1.00f);
    colors[ImGuiCol_SeparatorActive]  = s_theme.accent;
    colors[ImGuiCol_MenuBarBg]        = s_theme.bgSidebar;
    colors[ImGuiCol_ResizeGrip]       = ImVec4(0.30f,0.33f,0.40f,0.30f);
    colors[ImGuiCol_ResizeGripHovered]= ImVec4(0.45f,0.49f,0.58f,0.50f);
    colors[ImGuiCol_ResizeGripActive] = s_theme.accent;
}

void ThemeEngine::saveToConfig() {
    Config::setInt("Theme", "_version", kThemeVersion);
    Config::setString("Theme", "name", s_theme.name);
    Config::setColor("Theme", "accent", IM_COL32((int)(s_theme.accent.x*255),(int)(s_theme.accent.y*255),(int)(s_theme.accent.z*255),(int)(s_theme.accent.w*255)));
    Config::setColor("Theme", "accentHover", IM_COL32((int)(s_theme.accentHover.x*255),(int)(s_theme.accentHover.y*255),(int)(s_theme.accentHover.z*255),(int)(s_theme.accentHover.w*255)));
    Config::setColor("Theme", "accentDim", IM_COL32((int)(s_theme.accentDim.x*255),(int)(s_theme.accentDim.y*255),(int)(s_theme.accentDim.z*255),(int)(s_theme.accentDim.w*255)));
    Config::setColor("Theme", "bgPanel", IM_COL32((int)(s_theme.bgPanel.x*255),(int)(s_theme.bgPanel.y*255),(int)(s_theme.bgPanel.z*255),(int)(s_theme.bgPanel.w*255)));
    Config::setColor("Theme", "bgContent", IM_COL32((int)(s_theme.bgContent.x*255),(int)(s_theme.bgContent.y*255),(int)(s_theme.bgContent.z*255),(int)(s_theme.bgContent.w*255)));
    Config::setColor("Theme", "bgCard", IM_COL32((int)(s_theme.bgCard.x*255),(int)(s_theme.bgCard.y*255),(int)(s_theme.bgCard.z*255),(int)(s_theme.bgCard.w*255)));
    Config::setColor("Theme", "bgSidebar", IM_COL32((int)(s_theme.bgSidebar.x*255),(int)(s_theme.bgSidebar.y*255),(int)(s_theme.bgSidebar.z*255),(int)(s_theme.bgSidebar.w*255)));
    Config::setColor("Theme", "textPrimary", IM_COL32((int)(s_theme.textPrimary.x*255),(int)(s_theme.textPrimary.y*255),(int)(s_theme.textPrimary.z*255),(int)(s_theme.textPrimary.w*255)));
    Config::setColor("Theme", "textSecondary", IM_COL32((int)(s_theme.textSecondary.x*255),(int)(s_theme.textSecondary.y*255),(int)(s_theme.textSecondary.z*255),(int)(s_theme.textSecondary.w*255)));
    Config::setColor("Theme", "textMuted", IM_COL32((int)(s_theme.textMuted.x*255),(int)(s_theme.textMuted.y*255),(int)(s_theme.textMuted.z*255),(int)(s_theme.textMuted.w*255)));
    Config::setColor("Theme", "border", IM_COL32((int)(s_theme.border.x*255),(int)(s_theme.border.y*255),(int)(s_theme.border.z*255),(int)(s_theme.border.w*255)));
    Config::setColor("Theme", "separator", IM_COL32((int)(s_theme.separator.x*255),(int)(s_theme.separator.y*255),(int)(s_theme.separator.z*255),(int)(s_theme.separator.w*255)));
    Config::setFloat("Theme", "blurAmount", s_theme.blurAmount);
    Config::setFloat("Theme", "glowAmount", s_theme.glowAmount);
    Config::setFloat("Theme", "shadowAmount", s_theme.shadowAmount);
    Config::setFloat("Theme", "windowRounding", s_theme.windowRounding);
    Config::setFloat("Theme", "frameRounding", s_theme.frameRounding);
    Config::setFloat("Theme", "popupRounding", s_theme.popupRounding);
    Config::setFloat("Theme", "scrollRounding", s_theme.scrollRounding);
    Config::setInt("Theme", "fontSize", s_theme.fontSize);
    Config::setFloat("Theme", "fontSpacing", s_theme.fontSpacing);
    Config::setBool("Theme", "fontBold", s_theme.fontBold);
    Config::setFloat("Theme", "itemSpacingX", s_theme.itemSpacingX);
    Config::setFloat("Theme", "itemSpacingY", s_theme.itemSpacingY);
    Config::setFloat("Theme", "framePaddingX", s_theme.framePaddingX);
    Config::setFloat("Theme", "framePaddingY", s_theme.framePaddingY);
    Config::setFloat("Theme", "windowPaddingX", s_theme.windowPaddingX);
    Config::setFloat("Theme", "windowPaddingY", s_theme.windowPaddingY);
}

void ThemeEngine::loadFromConfig() {
    int savedVer = Config::getInt("Theme", "_version", 0);
    if (savedVer < kThemeVersion) {
        s_theme = s_defaults;
        saveToConfig();
        return;
    }
    s_theme.name = Config::getString("Theme", "name", s_defaults.name);
    ImU32 ac = Config::getColor("Theme", "accent", IM_COL32(51,140,255,255));
    s_theme.accent = ImVec4(((ac>>0)&0xFF)/255.0f,((ac>>8)&0xFF)/255.0f,((ac>>16)&0xFF)/255.0f,((ac>>24)&0xFF)/255.0f);
    ImU32 ah = Config::getColor("Theme", "accentHover", IM_COL32(89,165,255,255));
    s_theme.accentHover = ImVec4(((ah>>0)&0xFF)/255.0f,((ah>>8)&0xFF)/255.0f,((ah>>16)&0xFF)/255.0f,((ah>>24)&0xFF)/255.0f);
    ImU32 ad = Config::getColor("Theme", "accentDim", IM_COL32(33,96,178,255));
    s_theme.accentDim = ImVec4(((ad>>0)&0xFF)/255.0f,((ad>>8)&0xFF)/255.0f,((ad>>16)&0xFF)/255.0f,((ad>>24)&0xFF)/255.0f);
    ImU32 bp = Config::getColor("Theme", "bgPanel", IM_COL32(20,22,28,247));
    s_theme.bgPanel = ImVec4(((bp>>0)&0xFF)/255.0f,((bp>>8)&0xFF)/255.0f,((bp>>16)&0xFF)/255.0f,((bp>>24)&0xFF)/255.0f);
    ImU32 bc = Config::getColor("Theme", "bgContent", IM_COL32(25,28,35,255));
    s_theme.bgContent = ImVec4(((bc>>0)&0xFF)/255.0f,((bc>>8)&0xFF)/255.0f,((bc>>16)&0xFF)/255.0f,((bc>>24)&0xFF)/255.0f);
    ImU32 bcr = Config::getColor("Theme", "bgCard", IM_COL32(28,30,38,255));
    s_theme.bgCard = ImVec4(((bcr>>0)&0xFF)/255.0f,((bcr>>8)&0xFF)/255.0f,((bcr>>16)&0xFF)/255.0f,((bcr>>24)&0xFF)/255.0f);
    ImU32 bs = Config::getColor("Theme", "bgSidebar", IM_COL32(15,17,22,255));
    s_theme.bgSidebar = ImVec4(((bs>>0)&0xFF)/255.0f,((bs>>8)&0xFF)/255.0f,((bs>>16)&0xFF)/255.0f,((bs>>24)&0xFF)/255.0f);
    ImU32 tp = Config::getColor("Theme", "textPrimary", IM_COL32(237,239,244,255));
    s_theme.textPrimary = ImVec4(((tp>>0)&0xFF)/255.0f,((tp>>8)&0xFF)/255.0f,((tp>>16)&0xFF)/255.0f,((tp>>24)&0xFF)/255.0f);
    ImU32 ts = Config::getColor("Theme", "textSecondary", IM_COL32(140,147,165,255));
    s_theme.textSecondary = ImVec4(((ts>>0)&0xFF)/255.0f,((ts>>8)&0xFF)/255.0f,((ts>>16)&0xFF)/255.0f,((ts>>24)&0xFF)/255.0f);
    s_theme.textMuted   = ImVec4(s_theme.textSecondary.x*0.7f, s_theme.textSecondary.y*0.7f, s_theme.textSecondary.z*0.7f, 1.0f);
    ImU32 bd = Config::getColor("Theme", "border", IM_COL32(76,82,100,217));
    s_theme.border = ImVec4(((bd>>0)&0xFF)/255.0f,((bd>>8)&0xFF)/255.0f,((bd>>16)&0xFF)/255.0f,((bd>>24)&0xFF)/255.0f);
    s_theme.blurAmount = Config::getFloat("Theme", "blurAmount", s_defaults.blurAmount);
    s_theme.glowAmount = Config::getFloat("Theme", "glowAmount", s_defaults.glowAmount);
    s_theme.shadowAmount = Config::getFloat("Theme", "shadowAmount", s_defaults.shadowAmount);
    s_theme.windowRounding  = Config::getFloat("Theme", "windowRounding", s_defaults.windowRounding);
    s_theme.frameRounding   = Config::getFloat("Theme", "frameRounding", s_defaults.frameRounding);
    s_theme.popupRounding   = Config::getFloat("Theme", "popupRounding", s_defaults.popupRounding);
    s_theme.scrollRounding  = Config::getFloat("Theme", "scrollRounding", s_defaults.scrollRounding);
    s_theme.fontSize = Config::getInt("Theme", "fontSize", s_defaults.fontSize);
    s_theme.fontSpacing = Config::getFloat("Theme", "fontSpacing", s_defaults.fontSpacing);
    s_theme.fontBold = Config::getBool("Theme", "fontBold", s_defaults.fontBold);
    s_theme.itemSpacingX = Config::getFloat("Theme", "itemSpacingX", s_defaults.itemSpacingX);
    s_theme.itemSpacingY = Config::getFloat("Theme", "itemSpacingY", s_defaults.itemSpacingY);
    s_theme.framePaddingX = Config::getFloat("Theme", "framePaddingX", s_defaults.framePaddingX);
    s_theme.framePaddingY = Config::getFloat("Theme", "framePaddingY", s_defaults.framePaddingY);
    s_theme.windowPaddingX= Config::getFloat("Theme", "windowPaddingX", s_defaults.windowPaddingX);
    s_theme.windowPaddingY= Config::getFloat("Theme", "windowPaddingY", s_defaults.windowPaddingY);
}

void ThemeEngine::resetToDefaults() {
    s_theme = s_defaults;
    applyToImGui();
}

void ThemeEngine::renderConfigUI() {
    ImGui::TextColored(ImVec4(s_theme.accent.x, s_theme.accent.y, s_theme.accent.z, 1.0f), "THEME STUDIO");
    ImGui::Spacing();

    // ── Accent ──────────────────────────────────────────────────────
    float ac[4] = {s_theme.accent.x, s_theme.accent.y, s_theme.accent.z, s_theme.accent.w};
    if (ImGui::ColorEdit4("Accent", ac, ImGuiColorEditFlags_AlphaBar)) {
        s_theme.accent = ImVec4(ac[0], ac[1], ac[2], ac[3]);
        s_theme.accentHover = ImVec4(fminf(ac[0]*1.4f,1.0f), fminf(ac[1]*1.3f,1.0f), fminf(ac[2]*1.15f,1.0f), ac[3]);
        s_theme.accentDim   = ImVec4(ac[0]*0.65f, ac[1]*0.75f, ac[2]*0.9f, ac[3]);
    }

    // ── Backgrounds ─────────────────────────────────────────────────
    float bp[4] = {s_theme.bgPanel.x, s_theme.bgPanel.y, s_theme.bgPanel.z, s_theme.bgPanel.w};
    if (ImGui::ColorEdit4("Panel BG", bp, ImGuiColorEditFlags_AlphaBar))
        s_theme.bgPanel = ImVec4(bp[0], bp[1], bp[2], bp[3]);

    float bc[4] = {s_theme.bgContent.x, s_theme.bgContent.y, s_theme.bgContent.z, s_theme.bgContent.w};
    if (ImGui::ColorEdit4("Content BG", bc, ImGuiColorEditFlags_AlphaBar))
        s_theme.bgContent = ImVec4(bc[0], bc[1], bc[2], bc[3]);

    float bcr[4] = {s_theme.bgCard.x, s_theme.bgCard.y, s_theme.bgCard.z, s_theme.bgCard.w};
    if (ImGui::ColorEdit4("Card BG", bcr, ImGuiColorEditFlags_AlphaBar))
        s_theme.bgCard = ImVec4(bcr[0], bcr[1], bcr[2], bcr[3]);

    float bs[4] = {s_theme.bgSidebar.x, s_theme.bgSidebar.y, s_theme.bgSidebar.z, s_theme.bgSidebar.w};
    if (ImGui::ColorEdit4("Sidebar BG", bs, ImGuiColorEditFlags_AlphaBar))
        s_theme.bgSidebar = ImVec4(bs[0], bs[1], bs[2], bs[3]);

    // ── Text ────────────────────────────────────────────────────────
    float tp[4] = {s_theme.textPrimary.x, s_theme.textPrimary.y, s_theme.textPrimary.z, s_theme.textPrimary.w};
    if (ImGui::ColorEdit4("Text", tp, ImGuiColorEditFlags_AlphaBar))
        s_theme.textPrimary = ImVec4(tp[0], tp[1], tp[2], tp[3]);

    float ts[4] = {s_theme.textSecondary.x, s_theme.textSecondary.y, s_theme.textSecondary.z, s_theme.textSecondary.w};
    if (ImGui::ColorEdit4("Text Dim", ts, ImGuiColorEditFlags_AlphaBar)) {
        s_theme.textSecondary = ImVec4(ts[0], ts[1], ts[2], ts[3]);
        s_theme.textMuted = ImVec4(ts[0]*0.7f, ts[1]*0.7f, ts[2]*0.7f, 1.0f);
    }

    // ── Effects ─────────────────────────────────────────────────────
    ImGui::SliderFloat("Blur", &s_theme.blurAmount, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Glow",  &s_theme.glowAmount, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Shadow",&s_theme.shadowAmount, 0.0f, 1.0f, "%.2f");

    // ── Rounding ────────────────────────────────────────────────────
    ImGui::SliderFloat("Window Rounding", &s_theme.windowRounding, 0.0f, 20.0f, "%.0f");
    ImGui::SliderFloat("Frame Rounding",  &s_theme.frameRounding,  0.0f, 12.0f, "%.0f");
    ImGui::SliderFloat("Popup Rounding",  &s_theme.popupRounding,  0.0f, 12.0f, "%.0f");

    // ── Transparency ────────────────────────────────────────────────
    ImGui::SliderFloat("Panel Alpha", &s_theme.bgPanel.w, 0.3f, 1.0f, "%.2f");
    ImGui::SliderFloat("Content Alpha", &s_theme.bgContent.w, 0.3f, 1.0f, "%.2f");

    // ── Spacing ─────────────────────────────────────────────────────
    ImGui::SliderFloat("Item Spacing X", &s_theme.itemSpacingX, 4, 20, "%.0f");
    ImGui::SliderFloat("Item Spacing Y", &s_theme.itemSpacingY, 2, 16, "%.0f");
    ImGui::SliderFloat("Window Pad X", &s_theme.windowPaddingX, 4, 30, "%.0f");
    ImGui::SliderFloat("Window Pad Y", &s_theme.windowPaddingY, 2, 20, "%.0f");

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (ImGui::Button("Apply", ImVec2(100, 26))) applyToImGui();
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(100, 26))) { saveToConfig(); applyToImGui(); }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(100, 26))) resetToDefaults();
}
