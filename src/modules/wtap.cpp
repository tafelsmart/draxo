#include "pch.h"
#include "core/strcrypt.h"
#include "modules/wtap.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"

WTap::WTap() : Module("WTap", ModuleCategory::COMBAT, 0, "Automatically W-taps after hits to keep your combos") {
    defineFloat("delay", "Delay (ms)", 200.0f, 50.0f, 600.0f, "%.0f");
    addSearchTag("combo");
    addSearchTag("sprint");
}

void WTap::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!lmb) return;

    // ── ⚠️ Detect warnings ─────────────────────────────────────────────
    float delay = m_floatSettings["delay"];
    if (delay < 80.0f)
        addDetectWarning("Delay < 80 ms", true, "W-Tap faster than 80ms is inhuman. GrimAC detects identical keystroke timing. Raise to 100-200.");
    if (delay < 50.0f)
        addDetectWarning("Delay < 50 ms", true, "Impossible keystroke speed. All ACs detect this. Raise to 100+ ms.");

    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (ms - m_last < delay) return;
    m_last = ms;

    keybd_event('W', 0, KEYEVENTF_KEYUP, 0);
    m_tapPending = true;
}

void WTap::onRender() {
    if (m_tapPending) {
        keybd_event('W', 0, 0, 0);
        m_tapPending = false;
    }
}
