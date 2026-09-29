#include "pch.h"
#include "core/strcrypt.h"
#include "modules/nopumpkin.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

NoPumpkin::NoPumpkin() : Module("NoPumpkin", ModuleCategory::RENDER, 0,
    "Removes the pumpkin overlay when wearing a pumpkin on your head.") {
    setTickInterval(1);
    addSearchTag("pumpkin");
    addSearchTag("overlay");
    addSearchTag("blur");
}

void NoPumpkin::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // Find Gui instance via Minecraft.gui field
    static jfieldID s_guiFld = nullptr;
    static jmethodID s_getItem = nullptr;
    if (!s_guiFld) {
        jclass mcC = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (mcC) {
            s_guiFld = env->GetFieldID(mcC, "gui", "Lnet/minecraft/client/gui/Gui;");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }
    if (!s_guiFld) return;

    jobject mc = CMinecraft::getInstance();
    if (!mc) return;
    jobject gui = env->GetObjectField(mc, s_guiFld);
    env->DeleteLocalRef(mc);
    if (!gui) return;

    // In modern MC: Gui doesn't expose pumpkin control, but ScreenEffectRenderer does.
    // Alternative: clear the helmet slot during render (server won't see it).
    // Use Entity.getSlot() / setItemSlot for the head slot (slot 5 = 103 for equipment)
    // Simpler: use the entity setSharedFlag approach for "invisible" effect — no.

    // Most portable approach for 1.17+:
    // Gui has a field "pumpkinOverlay" / the head equipment check
    // For 26.x: try disabling via the player's equipment

    static jfieldID s_invFld = nullptr;
    static jclass s_playerCls = nullptr;
    if (!s_playerCls) s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerCls && !s_invFld)
        s_invFld = env->GetFieldID(s_playerCls, Mappings::Player_inventory, Mappings::Player_inventory_Sig);

    if (!s_invFld) { env->DeleteLocalRef(gui); return; }

    jobject player = CMinecraft::getPlayer();
    if (!player) { env->DeleteLocalRef(gui); return; }

    // Check head slot (inventory index 39 = head equipment)
    static jclass s_invCls = nullptr;
    static jmethodID s_invGetItem = nullptr;
    if (!s_invCls) s_invCls = JvmWrapper::findClass(Mappings::Inventory_Class);
    if (s_invCls && !s_invGetItem)
        s_invGetItem = env->GetMethodID(s_invCls, Mappings::Inventory_getItem, Mappings::Inventory_getItem_Sig);

    if (!s_invGetItem) { env->DeleteLocalRef(player); env->DeleteLocalRef(gui); return; }

    jobject inv = env->GetObjectField(player, s_invFld);
    if (!inv) { env->DeleteLocalRef(player); env->DeleteLocalRef(gui); return; }

    // Slot 39 = head armor in player inventory
    jobject helmet = env->CallObjectMethod(inv, s_invGetItem, (jint)39);
    env->DeleteLocalRef(inv);
    if (!helmet || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(gui);
        return;
    }

    // Check if helmet item description contains "pumpkin"
    static jclass s_stackCls = nullptr;
    static jmethodID s_stackGetItem = nullptr;
    if (!s_stackCls) s_stackCls = JvmWrapper::findClass(Mappings::ItemStack_Class);
    if (s_stackCls && !s_stackGetItem)
        s_stackGetItem = env->GetMethodID(s_stackCls, Mappings::ItemStack_getItem, Mappings::ItemStack_getItem_Sig);

    if (s_stackGetItem) {
        jobject item = env->CallObjectMethod(helmet, s_stackGetItem);
        if (item && !env->ExceptionCheck()) {
            static jclass s_itemCls = nullptr;
            static jmethodID s_descId = nullptr;
            if (!s_itemCls) s_itemCls = JvmWrapper::findClass(Mappings::Item_Class);
            if (s_itemCls && !s_descId)
                s_descId = env->GetMethodID(s_itemCls, Mappings::Block_getDescriptionId, Mappings::Block_getDescriptionId_Sig);

            if (s_descId) {
                jstring desc = (jstring)env->CallObjectMethod(item, s_descId);
                if (desc && !env->ExceptionCheck()) {
                    const char* cdesc = env->GetStringUTFChars(desc, nullptr);
                    if (cdesc && strstr(cdesc, "pumpkin")) {
                        // Set the Gui pumpkin blur to 0 via the ScreenEffectRenderer
                        // For cross-version: set a system property or toggle
                        // Simplest that works: set player portalCooldown = 0 pattern
                        // Actually — just toggle the Gui.renderPumpkin condition
                        // We can set the player's "isPumpkinHead" flag
                        // In 1.21.x: the blur reads from the helmet render check
                        // Most portable: set the Gui pumpkinOverlay field to 0f
                        static jfieldID s_pumpScale = nullptr;
                        static bool s_tried = false;
                        if (!s_tried) {
                            s_tried = true;
                            jclass guiCls = env->GetObjectClass(gui);
                            s_pumpScale = env->GetFieldID(guiCls, "pumpkinOverlay", "F");
                            if (env->ExceptionCheck()) {
                                env->ExceptionClear();
                                s_pumpScale = env->GetFieldID(guiCls, "pumpkinBlur", "F");
                                if (env->ExceptionCheck()) env->ExceptionClear();
                            }
                        }
                        if (s_pumpScale)
                            env->SetFloatField(gui, s_pumpScale, 0.0f);
                    }
                    env->ReleaseStringUTFChars(desc, cdesc);
                }
                if (desc) env->DeleteLocalRef(desc);
            }
            env->DeleteLocalRef(item);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    env->DeleteLocalRef(helmet);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(gui);
}
