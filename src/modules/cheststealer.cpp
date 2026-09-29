#include "pch.h"
#include "core/strcrypt.h"
#include "modules/cheststealer.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

ChestStealer::ChestStealer() : Module("ChestStealer", ModuleCategory::PLAYER, 0, "Automatically takes items from opened chests") {
    setTickInterval(3);
    defineFloat("delay", "Delay (ms)", 100.0f, 20.0f, 500.0f, "%.0f");
    addSearchTag("loot");
    addSearchTag("steal");
}

void ChestStealer::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (ms - m_last < m_floatSettings["delay"]) return;
    m_last = ms;

    HWND h = FindWindowA("GLFW30", nullptr);
    if (!h || GetForegroundWindow() != h) return;

    // Non-blocking: shift+click without Sleep
    keybd_event(VK_SHIFT, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    m_clickDown = true;
}

void ChestStealer::onRender() {
    if (m_clickDown) {
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        m_clickDown = false;
    }
}
