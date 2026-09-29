#include "pch.h"
#include "core/strcrypt.h"
#include "modules/hitmarkers.h"

HitMarkers::HitMarkers() : Module("HitMarkers", ModuleCategory::RENDER, 0, "Shows a marker whenever your attack connects") {
    addSearchTag("crosshair");
    addSearchTag("indicator");
}
void HitMarkers::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    static bool wasAtk = false;
    bool atk = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (atk && !wasAtk) { /* marker pulse triggers on this edge */ }
    wasAtk = atk;
}
