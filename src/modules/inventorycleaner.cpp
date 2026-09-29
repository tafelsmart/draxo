#include "pch.h"
#include "core/strcrypt.h"
#include "modules/inventorycleaner.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

// ═══════════════════════════════════════════════════════════════════════
//  InventoryCleaner — Drop junk items from inventory
//
//  Scans player inventory, matches items by display name against a
//  configurable drop list, swaps non-hotbar items to hotbar via
//  handleInventoryMouseClick(SWAP), then presses Q to drop.
// ═══════════════════════════════════════════════════════════════════════

InventoryCleaner::InventoryCleaner() : Module("InventoryCleaner", ModuleCategory::PLAYER, 0,
    "Automatically drops junk items from your inventory. "
    "Configure which items to drop by name.") {
    setTickInterval(2);

    defineMode("mode",  "Mode",  0, {"Legit", "Rage"});
    defineFloat("delay", "Delay (ms)", 200.0f, 50.0f, 1000.0f, "%.0f");
    defineString("drop_list", "Drop List",
        "cobblestone,dirt,sand,gravel,andesite,diorite,granite,netherrack,"
        "wheat_seeds,pumpkin_seeds,melon_seeds,beetroot_seeds,"
        "rotten_flesh,bone,spider_eye,string,arrow,poisonous_potato,"
        "kelp,seagrass,sugar_cane,cactus,flint,egg,snowball,"
        "feather,leather,rabbit_hide,phantom_membrane,ink_sac,"
        "tropical_fish,cod,salmon,pufferfish,"
        "bamboo,stick,dead_bush,vine,lily_pad,moss_carpet,"
        "tuff,calcite,deepslate,cobbled_deepslate,"
        "mud,muddy_mangrove_roots,mangrove_roots,sculk,sculk_vein");
    defineBool("auto_tool", "Restore tool after drop", true);
    defineBool("notify",    "Notifications",          true);

    addSearchTag("inv");
    addSearchTag("clean");
    addSearchTag("drop");
    addSearchTag("inventory");
}

void InventoryCleaner::onEnable() {
    m_srcSlot   = -1;
    m_phase     = 0;
    m_origSlot  = -1;
    m_dstHotbar = 0;
    m_tickWait  = 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  JNI Init
// ═══════════════════════════════════════════════════════════════════════

bool InventoryCleaner::initJNI(JNIEnv* env) {
    if (s_jni) return true;

    // Minecraft.gameMode field
    if (!s_mcCls) s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (s_mcCls && !s_gmField)
        s_gmField = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);

    // Player.inventory + containerMenu fields
    if (!s_playerCls) s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerCls) {
        if (!s_invField)
            s_invField = env->GetFieldID(s_playerCls, Mappings::Player_inventory, Mappings::Player_inventory_Sig);
        if (!s_menuField)
            s_menuField = env->GetFieldID(s_playerCls, Mappings::Player_containerMenu, Mappings::Player_containerMenu_Sig);
    }

    // Inventory methods
    if (!s_invCls) s_invCls = JvmWrapper::findClass(Mappings::Inventory_Class);
    if (s_invCls) {
        if (!s_getItem)
            s_getItem = env->GetMethodID(s_invCls, Mappings::Inventory_getItem, Mappings::Inventory_getItem_Sig);
        if (!s_getSelected)
            s_getSelected = env->GetMethodID(s_invCls, Mappings::Inventory_getSelectedSlot, Mappings::Inventory_getSelectedSlot_Sig);
        if (!s_setSelected)
            s_setSelected = env->GetMethodID(s_invCls, Mappings::Inventory_setSelectedSlot, Mappings::Inventory_setSelectedSlot_Sig);
    }

    // ItemStack
    if (!s_stackCls) s_stackCls = JvmWrapper::findClass(Mappings::ItemStack_Class);
    if (s_stackCls) {
        if (!s_stackIsEmpty)
            s_stackIsEmpty = env->GetMethodID(s_stackCls, Mappings::ItemStack_isEmpty, Mappings::ItemStack_isEmpty_Sig);
        if (!s_stackGetHover)
            s_stackGetHover = env->GetMethodID(s_stackCls, Mappings::ItemStack_getHoverName, Mappings::ItemStack_getHoverName_Sig);
    }

    // Component.getString()
    if (!s_compCls) s_compCls = JvmWrapper::findClass(Mappings::Component_Class);
    if (s_compCls && !s_compGetStr)
        s_compGetStr = env->GetMethodID(s_compCls, Mappings::Component_getString, Mappings::Component_getString_Sig);

    // GameMode.handleInventoryMouseClick + ClickType.SWAP
    if (!s_gmCls) s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
    if (s_gmCls && !s_handleClick)
        s_handleClick = env->GetMethodID(s_gmCls, Mappings::GameMode_handleInventoryMouseClick,
                                         Mappings::GameMode_handleInventoryMouseClick_Sig);

    if (!s_ctCls) s_ctCls = JvmWrapper::findClass(Mappings::ClickType_Class);
    if (s_ctCls && !s_swapField)
        s_swapField = env->GetStaticFieldID(s_ctCls, Mappings::ClickType_SWAP, Mappings::ClickType_SWAP_Sig);

    // AbstractContainerMenu.containerId
    if (!s_acmCls) s_acmCls = JvmWrapper::findClass(Mappings::AbstractContainerMenu_Class);
    if (s_acmCls && !s_ctrIdField)
        s_ctrIdField = env->GetFieldID(s_acmCls, Mappings::AbstractContainerMenu_containerId,
                                       Mappings::AbstractContainerMenu_containerId_Sig);

    s_jni = s_mcCls && s_gmField && s_playerCls && s_invField && s_invCls
         && s_getItem && s_stackCls && s_stackIsEmpty && s_stackGetHover
         && s_compGetStr && s_handleClick && s_swapField && s_ctrIdField;
    return s_jni;
}

// ═══════════════════════════════════════════════════════════════════════
//  isJunkItem — check ItemStack hover name against drop list
// ═══════════════════════════════════════════════════════════════════════

bool InventoryCleaner::isJunkItem(JNIEnv* env, jobject stack) {
    if (!stack || !s_stackIsEmpty || !s_stackGetHover || !s_compGetStr) return false;

    // Skip empty stacks
    jboolean empty = env->CallBooleanMethod(stack, s_stackIsEmpty);
    if (empty || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    // Get hover name (Component)
    jobject hoverName = env->CallObjectMethod(stack, s_stackGetHover);
    if (!hoverName || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    // Get plain string
    jstring jstr = (jstring)env->CallObjectMethod(hoverName, s_compGetStr);
    env->DeleteLocalRef(hoverName);
    if (!jstr || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    const char* name = env->GetStringUTFChars(jstr, nullptr);
    if (!name) { env->DeleteLocalRef(jstr); return false; }

    // Convert name to lowercase for case-insensitive matching
    std::string lower(name);
    env->ReleaseStringUTFChars(jstr, name);
    env->DeleteLocalRef(jstr);
    for (auto& c : lower) c = (char)tolower((unsigned char)c);

    // Check against drop list
    const std::string& dropList = m_stringSettings["drop_list"];
    std::string token;
    for (size_t i = 0; i <= dropList.size(); i++) {
        if (i == dropList.size() || dropList[i] == ',') {
            // Trim whitespace
            while (!token.empty() && token.front() == ' ') token.erase(0, 1);
            while (!token.empty() && token.back()  == ' ') token.pop_back();
            if (!token.empty() && lower.find(token) != std::string::npos)
                return true;
            token.clear();
        } else {
            token += dropList[i];
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
//  swapToHotbar — ClickType.SWAP from inv slot to hotbar slot
// ═══════════════════════════════════════════════════════════════════════

bool InventoryCleaner::swapToHotbar(JNIEnv* env, jobject mc, jobject player,
                                     int srcIdx, int dstHotbar) {
    if (!s_handleClick || !s_swapField || !s_menuField || !s_ctrIdField) return false;

    // Only swap in own inventory (containerId == 0)
    jobject menu = env->GetObjectField(player, s_menuField);
    if (!menu || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jint containerId = env->GetIntField(menu, s_ctrIdField);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (containerId != 0) return false;  // chest/container open — don't touch

    jobject gm = env->GetObjectField(mc, s_gmField);
    if (!gm || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jobject swapType = env->GetStaticObjectField(s_ctCls, s_swapField);
    if (!swapType || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    env->CallVoidMethod(gm, s_handleClick, containerId, (jint)srcIdx,
                        (jint)dstHotbar, swapType, player);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
//  doDrop — select hotbar slot, press Q
// ═══════════════════════════════════════════════════════════════════════

void InventoryCleaner::doDrop(JNIEnv* env, jobject inv, int slot) {
    if (!inv || !s_setSelected) return;

    // Select the slot
    env->CallVoidMethod(inv, s_setSelected, (jint)slot);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

    // Send drop key (Q) via Windows input simulation
    keybd_event(0x51, 0, 0, 0);           // Q down
    keybd_event(0x51, 0, KEYEVENTF_KEYUP, 0); // Q up
}

// ═══════════════════════════════════════════════════════════════════════
//  onUpdate — state machine: scan → swap → drop → restore
// ═══════════════════════════════════════════════════════════════════════

void InventoryCleaner::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── Delay timing ────────────────────────────────────────────────
    auto now = std::chrono::steady_clock::now();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    static long long s_lastMs = 0;
    float delay = m_floatSettings["delay"];
    if (m_phase == 0 && ms - s_lastMs < delay) return;  // only throttle scan
    s_lastMs = ms;

    // ── Window focus check ──────────────────────────────────────────
    HWND h = FindWindowA("GLFW30", nullptr);
    if (!h || GetForegroundWindow() != h) { m_phase = 0; return; }

    if (!initJNI(env)) return;

    bool rage = (m_intSettings["mode"] == 1);

    // ── Get player + inventory ──────────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    static jfieldID s_playerFld = nullptr;
    if (!s_playerFld) {
        jclass mcC = env->GetObjectClass(mc);
        s_playerFld = env->GetFieldID(mcC, Mappings::MC_player, Mappings::MC_player_Sig);
        env->DeleteLocalRef(mcC);
    }
    jobject player = env->GetObjectField(mc, s_playerFld);
    if (!player || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (player) env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

    jobject inv = env->GetObjectField(player, s_invField);
    if (!inv || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (inv) env->DeleteLocalRef(inv);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

    // ── Phase 0: Scan inventory for next junk item ─────────────────
    if (m_phase == 0) {
        // Save original hotbar slot if not saved yet
        if (m_origSlot < 0) {
            m_origSlot = (int)env->CallIntMethod(inv, s_getSelected);
            if (env->ExceptionCheck()) { env->ExceptionClear(); m_origSlot = 0; }
            m_dstHotbar = m_origSlot;  // use current hotbar slot for swaps
        }

        // Scan all inventory slots (0-35 for player inventory)
        for (int slot = 35; slot >= 0; slot--) {
            jobject stack = env->CallObjectMethod(inv, s_getItem, (jint)slot);
            if (!stack || env->ExceptionCheck()) { env->ExceptionClear(); continue; }

            bool isJunk = isJunkItem(env, stack);
            env->DeleteLocalRef(stack);
            if (!isJunk) continue;

            m_srcSlot = slot;
            if (slot >= 0 && slot <= 8) {
                // Hotbar item: just drop it
                m_phase = 2;
            } else {
                // Main inventory: swap to hotbar first
                m_phase = 1;
            }
            m_tickWait = rage ? 0 : 1;
            break;  // one item per tick
        }
    }

    // ── Phase 1: Swap item to hotbar ────────────────────────────────
    if (m_phase == 1 && m_srcSlot >= 9 && m_tickWait <= 0) {
        bool ok = swapToHotbar(env, mc, player, m_srcSlot, m_dstHotbar);
        if (ok) {
            m_phase = 2;
            m_tickWait = rage ? 0 : 2;
        } else {
            m_phase = 0;  // swap failed, back to scan
            m_srcSlot = -1;
        }
    }

    // ── Phase 2: Drop the item ──────────────────────────────────────
    if (m_phase == 2 && m_srcSlot >= 0 && m_tickWait <= 0) {
        int dropSlot = (m_srcSlot >= 9) ? m_dstHotbar : m_srcSlot;
        doDrop(env, inv, dropSlot);
        m_phase = 3;
        m_tickWait = rage ? 0 : 3;
    }

    // ── Phase 3: Restore original slot ──────────────────────────────
    if (m_phase == 3 && m_tickWait <= 0) {
        if (m_origSlot >= 0 && m_boolSettings["auto_tool"]) {
            env->CallVoidMethod(inv, s_setSelected, (jint)m_origSlot);
            if (env->ExceptionCheck()) env->ExceptionClear();
            m_origSlot = -1;
        }
        m_phase = 0;
        m_srcSlot = -1;
    }

    if (m_tickWait > 0) m_tickWait--;

    env->DeleteLocalRef(inv);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
