#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autofarm.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

AutoFarm::AutoFarm() : Module("AutoFarm", ModuleCategory::WORLD, 0, "Automatically harvests crops within range") {
    setTickInterval(3);
    defineFloat("radius", "Radius", 5.0f, 1.0f, 10.0f, "%.0f");
    defineFloat("delay", "Delay (ms)", 200.0f, 20.0f, 1000.0f, "%.0f");
    addSearchTag("crop");
    addSearchTag("harvest");
}

void AutoFarm::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (ms - m_last < m_floatSettings["delay"]) return;
    m_last = ms;

    HWND h = FindWindowA("GLFW30", nullptr);
    if (!h || GetForegroundWindow() != h) return;
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    m_clickDown = true;
}

void AutoFarm::onRender() {
    if (m_clickDown) {
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        m_clickDown = false;
    }
}
