#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autoarmor.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

AutoArmor::AutoArmor() : Module("AutoArmor", ModuleCategory::PLAYER, 0, "Equips the best armor from your inventory automatically") {
    setTickInterval(3);
    defineFloat("delay", "Delay (ms)", 200.0f, 20.0f, 1000.0f, "%.0f");
    addSearchTag("equip");
    addSearchTag("protect");
}

void AutoArmor::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (ms - m_last < m_floatSettings["delay"]) return;
    m_last = ms;

    HWND h = FindWindowA("GLFW30", nullptr);
    if (!h || GetForegroundWindow() != h) return;
    keybd_event(VK_SHIFT, 0, 0, 0);
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    m_clickDown = true;
}

void AutoArmor::onRender() {
    if (m_clickDown) {
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        m_clickDown = false;
    }
}
