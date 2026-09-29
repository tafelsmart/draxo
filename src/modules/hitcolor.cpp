#include "pch.h"
#include "core/strcrypt.h"
#include "modules/hitcolor.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

HitColor::HitColor() : Module("HitColor", ModuleCategory::RENDER, 0,
    "Customizes the color when entities take damage (hurt flash).") {
    defineColor("color","Hurt Color",IM_COL32(255,60,60,255));
    defineBool("custom","Use Custom Color",true);
    addSearchTag("hit"); addSearchTag("hurt"); addSearchTag("damage");
}

void HitColor::onUpdate(JNIEnv* env) {
    // Entity hurt color is controlled by LivingEntity.hurtDuration
    // When > 0, entities render with a red tint.
    // For custom color: set a render-state color via Minecraft's EntityRenderDispatcher
    // This is a GL-render-hook feature — the color can be changed by modifying
    // the GL color mask during entity rendering in hooks.cpp
    if (!m_enabled) return;
    // Stub: full implementation needs GL hook in render pipeline
}
