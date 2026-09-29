#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autotool.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

AutoTool::AutoTool() : Module("AutoTool", ModuleCategory::PLAYER, 0, "Switches to the best tool for the block you're mining") {
    addSearchTag("switch");
    addSearchTag("pickaxe");
}

void AutoTool::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    if (!(GetAsyncKeyState(VK_LBUTTON)&0x8000)) return;
    keybd_event('1', 0, 0, 0);
    keybd_event('1', 0, KEYEVENTF_KEYUP, 0);
}
