#include "pch.h"
#include "core/strcrypt.h"
#include "modules/enchantglint.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

EnchantGlint::EnchantGlint() : Module("EnchantGlint", ModuleCategory::RENDER, 0,
    "Customizes the enchantment glint color on items.") {
    defineColor("glint","Glint Color",IM_COL32(128,80,255,200));
    addSearchTag("enchant"); addSearchTag("glint");
}

void EnchantGlint::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    ImU32 col = m_colorSettings["glint"];
    // Set item GL state glint color
    // In 1.21+: ItemRenderer changes glint via ItemRenderer.blitGlint / GlintEffect
    // Simplest cross-version: set Minecraft's render state
    static jclass s_irCls=nullptr; static jfieldID s_glintFld=nullptr;
    static bool tr=false;
    if (!tr) { tr=true;
        s_irCls=JvmWrapper::findClass("net/minecraft/client/renderer/entity/ItemRenderer");
        if (s_irCls) {
            // Try to find glint-related field
            s_glintFld=env->GetStaticFieldID(s_irCls,"ENCHANTED_GLINT","I");
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_glintFld=nullptr; }
        }
    }
    // This is a stub — full implementation needs GL state manipulation
    // The color will be applied via GL blend in the render hook
}
