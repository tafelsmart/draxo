#include "pch.h"
#include "core/strcrypt.h"
#include "modules/scaffold.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include <cmath>

// ─────────────────────────────────────────────────────────────────────
// Scaffold v2
//
// Modes:
//   Legit     — classic sneak-release bridging. The camera is rotated 180°
//               (backward-bridging look) and a sneak/release cycle keeps the
//               player glued to the bridge edge.
//   Semi      — same 180° rotation, no sneaking. Blocks are placed perfectly
//               under/behind the player so you never fall.
//   Rage      — no rotation lock; blocks are placed continuously directly
//               under the player while you sprint forward.
//   GodBridge — forward bridging while looking almost straight down at the
//               feet. A fast crouch-timing cycle (hold → release → place →
//               glide) slides the player over freshly placed blocks.
//   Breezily  — speed bridging: auto-sprint + auto-jump while blocks are
//               placed directly under the player. You never stop moving.
//   Eagle     — safe auto-sneak bridging. Looks ahead at a relaxed angle
//               and holds sneak ONLY at block edges (SafeWalk style), so
//               you never fall while flat-ground walking stays normal.
//
// Safety: the block directly under the player is always checked first.
// If it is air, a block is placed there immediately — you can never fall
// while Scaffold is active.
// ─────────────────────────────────────────────────────────────────────

Scaffold::Scaffold() : Module("Scaffold", ModuleCategory::MOVEMENT, 0, "Auto-builds a bridge beneath you — 6 bridging modes") {
    defineGroup("Mode");
    defineMode("mode", "Mode", 0, {"Legit", "Semi", "Rage", "GodBridge", "Breezily", "Eagle"});
    defineBool("tower", "Tower", true);
    defineFloat("tower_delay", "Tower Jump Delay (ms)", 250.0f, 50.0f, 600.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Placement");
    defineFloat("rot_speed", "Rotation Speed", 18.0f, 1.0f, 60.0f, "%.1f");
    defineFloat("delay_min", "Delay Min (ms)", 70.0f, 10.0f, 400.0f, "%.0f");
    defineFloat("delay_max", "Delay Max (ms)", 140.0f, 10.0f, 400.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Sneak / Sprint");
    defineFloat("sneak_hold", "Sneak Hold (ms)", 320.0f, 50.0f, 1000.0f, "%.0f");
    defineFloat("sneak_release", "Sneak Release (ms)", 90.0f, 20.0f, 500.0f, "%.0f");
    defineFloat("god_hold", "GB Crouch Hold (ms)", 220.0f, 40.0f, 800.0f, "%.0f");
    defineFloat("god_release", "GB Release (ms)", 60.0f, 15.0f, 300.0f, "%.0f");
    defineFloat("breezily_jump", "Breezily Jump Delay (ms)", 140.0f, 40.0f, 500.0f, "%.0f");
    defineGroupEnd();

    defineGroup("Inventory");
    defineBool("auto_switch", "Auto Block Switch", true);
    defineBool("inventory_move", "Inventory Move", true);
    defineFloat("inv_move_delay", "Inv Move Delay (ms)", 120.0f, 30.0f, 500.0f, "%.0f");
    defineGroupEnd();
}

void Scaffold::onEnable() {
    m_hasTgt = false;
    m_baseInit = false;
    m_prevKeyState = 0;
    m_clickDown = false;
    m_sneakDown = false;
    m_sprintDown = false;
    m_lastPlace = std::chrono::steady_clock::now();
    m_lastSneakFlip = m_lastPlace;
    m_lastInvMove = m_lastPlace;
    m_lastJump = m_lastPlace;
    m_placeDelay = 100;
    m_previewName.clear();
    m_previewCount = 0;
    m_hasBlock = false;
}

void Scaffold::onDisable() {
    m_hasTgt = false;
    if (m_clickDown) { sendRightUp(); m_clickDown = false; }
    if (m_sneakDown) {
        keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
        m_sneakDown = false;
    }
    if (m_sprintDown) {
        keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
        m_sprintDown = false;
    }
}

// ── JNI init — all names via Mappings:: so the builder obfuscates them ──
void Scaffold::initJNI(JNIEnv* env) {
    if (sjni) return;

    s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerCls) {
        s_invField = env->GetFieldID(s_playerCls, Mappings::Player_inventory, Mappings::Player_inventory_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    s_invCls = JvmWrapper::findClass(Mappings::Inventory_Class);
    if (s_invCls) {
        s_getSelected = env->GetMethodID(s_invCls, Mappings::Inventory_getSelectedSlot, Mappings::Inventory_getSelectedSlot_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_setSelected = env->GetMethodID(s_invCls, Mappings::Inventory_setSelectedSlot, Mappings::Inventory_setSelectedSlot_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_getItem = env->GetMethodID(s_invCls, Mappings::Inventory_getItem, Mappings::Inventory_getItem_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    s_stackCls = JvmWrapper::findClass(Mappings::ItemStack_Class);
    if (s_stackCls) {
        s_stackIsEmpty = env->GetMethodID(s_stackCls, Mappings::ItemStack_isEmpty, Mappings::ItemStack_isEmpty_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_stackGetCount = env->GetMethodID(s_stackCls, Mappings::ItemStack_getCount, Mappings::ItemStack_getCount_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_stackGetItem = env->GetMethodID(s_stackCls, Mappings::ItemStack_getItem, Mappings::ItemStack_getItem_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_stackGetHoverName = env->GetMethodID(s_stackCls, Mappings::ItemStack_getHoverName, Mappings::ItemStack_getHoverName_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    s_compCls = JvmWrapper::findClass(Mappings::Component_Class);
    if (s_compCls) {
        s_compGetString = env->GetMethodID(s_compCls, Mappings::Component_getString, Mappings::Component_getString_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    s_itemCls = JvmWrapper::findClass(Mappings::Item_Class);
    s_blockItemCls = JvmWrapper::findClass(Mappings::BlockItem_Class);

    // Block air-check
    s_bpCls = JvmWrapper::findClass(Mappings::BlockPos_Class);
    if (s_bpCls) {
        s_bpInit = env->GetMethodID(s_bpCls, Mappings::BlockPos_Init, Mappings::BlockPos_Init_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    s_lvlCls = JvmWrapper::findClass(Mappings::ClientLevel_Class);
    if (s_lvlCls) {
        s_getState = env->GetMethodID(s_lvlCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    s_stateCls = JvmWrapper::findClass(Mappings::BlockState_Class);
    if (s_stateCls) {
        s_isAir = env->GetMethodID(s_stateCls, Mappings::BlockStateBase_isAir, Mappings::BlockStateBase_isAir_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Container click (InventoryMove) ────────────────────────────────
    s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (s_mcCls) {
        s_gmField = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
    if (s_gmCls) {
        s_handleClick = env->GetMethodID(s_gmCls, Mappings::GameMode_handleInventoryMouseClick,
                                         Mappings::GameMode_handleInventoryMouseClick_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    s_clickTypeCls = JvmWrapper::findClass(Mappings::ClickType_Class);
    if (s_clickTypeCls) {
        s_swapField = env->GetStaticFieldID(s_clickTypeCls, Mappings::ClickType_SWAP, Mappings::ClickType_SWAP_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (s_playerCls) {
        s_containerMenuField = env->GetFieldID(s_playerCls, Mappings::Player_containerMenu, Mappings::Player_containerMenu_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    jclass acmCls = JvmWrapper::findClass(Mappings::AbstractContainerMenu_Class);
    if (acmCls) {
        s_containerIdField = env->GetFieldID(acmCls, Mappings::AbstractContainerMenu_containerId,
                                             Mappings::AbstractContainerMenu_containerId_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Echte Block-Platzierung (Rage) via MultiPlayerGameMode.useItemOn ──
    if (s_gmCls) {
        s_useItemOn = env->GetMethodID(s_gmCls, Mappings::GameMode_useItemOn, Mappings::GameMode_useItemOn_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_useItemOn = nullptr; }
    }
    s_bhrCls = JvmWrapper::findClass(Mappings::BlockHitResult_Class);
    if (s_bhrCls) {
        s_bhrInit = env->GetMethodID(s_bhrCls, Mappings::BlockHitResult_Init, Mappings::BlockHitResult_Init_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_bhrInit = nullptr; }
    }
    s_vec3Cls = JvmWrapper::findClass(Mappings::Vec3_Class);
    if (s_vec3Cls) {
        s_vec3Init = env->GetMethodID(s_vec3Cls, Mappings::Vec3_Init, Mappings::Vec3_Init_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_vec3Init = nullptr; }
    }
    s_dirCls = JvmWrapper::findClass(Mappings::Direction_Class);
    if (s_dirCls) {
        s_dirDownField = env->GetStaticFieldID(s_dirCls, Mappings::Direction_DOWN, Mappings::Direction_DOWN_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_dirDownField = nullptr; }
    }
    s_handCls = JvmWrapper::findClass(Mappings::InteractionHand_Class);
    if (s_handCls) {
        s_mainHandField = env->GetStaticFieldID(s_handCls, Mappings::InteractionHand_MAIN_HAND, Mappings::InteractionHand_MAIN_HAND_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHandField = nullptr; }
    }
    s_irCls = JvmWrapper::findClass(Mappings::InteractionResult_Class);
    if (s_irCls) {
        s_irConsumes = env->GetMethodID(s_irCls, Mappings::InteractionResult_consumesAction, Mappings::InteractionResult_consumesAction_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_irConsumes = nullptr; }
    }

    sjni = true;
    printf(STR_C("[Draxo] Scaffold JNI: inv=%p/%p/%p stack=%p/%p item=%p/%p air=%p useItemOn=%p bhr=%p/%p dir=%p hand=%p ir=%p\n"),
           (void*)s_invField, (void*)s_getSelected, (void*)s_setSelected,
           (void*)s_stackCls, (void*)s_stackGetItem, (void*)s_itemCls,
           (void*)s_blockItemCls, (void*)s_isAir,
           (void*)s_useItemOn, (void*)s_bhrCls, (void*)s_bhrInit,
           (void*)s_dirDownField, (void*)s_mainHandField, (void*)s_irConsumes);
}

// ── Inventory helpers ────────────────────────────────────────────────
jobject Scaffold::getInventory(JNIEnv* env, jobject playerObj) {
    if (!s_invField || !playerObj) return nullptr;
    return env->GetObjectField(playerObj, s_invField);
}

bool Scaffold::stackIsEmpty(JNIEnv* env, jobject stack) {
    if (!stack || !s_stackIsEmpty) return true;
    jboolean r = env->CallBooleanMethod(stack, s_stackIsEmpty);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return r == JNI_TRUE;
}

int Scaffold::stackCount(JNIEnv* env, jobject stack) {
    if (!stack || !s_stackGetCount) return 0;
    jint r = env->CallIntMethod(stack, s_stackGetCount);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return (int)r;
}

bool Scaffold::stackIsBlock(JNIEnv* env, jobject stack) {
    if (!stack || !s_stackGetItem || !s_blockItemCls) return false;
    if (stackIsEmpty(env, stack)) return false;
    if (stackCount(env, stack) <= 0) return false;
    jobject item = env->CallObjectMethod(stack, s_stackGetItem);
    if (!item || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    bool isBlock = env->IsInstanceOf(item, s_blockItemCls) == JNI_TRUE;
    env->DeleteLocalRef(item);
    return isBlock;
}

std::string Scaffold::stackName(JNIEnv* env, jobject stack) {
    if (!stack || !s_stackGetHoverName || !s_compCls || !s_compGetString) return "";
    jobject comp = env->CallObjectMethod(stack, s_stackGetHoverName);
    if (!comp || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return "";
    }
    jstring js = (jstring)env->CallObjectMethod(comp, s_compGetString);
    if (!js || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return "";
    }
    std::string name = JvmWrapper::jstringToString(js);
    env->DeleteLocalRef(js);
    return name;
}

// Scan hotbar slots 0..8 for the first block item
int Scaffold::findBlockSlot(JNIEnv* env, jobject inv) {
    if (!inv || !s_getItem || !s_stackGetItem || !s_blockItemCls) return -1;
    for (int i = 0; i < 9; i++) {
        jobject stack = env->CallObjectMethod(inv, s_getItem, i);
        if (!stack || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            continue;
        }
        bool block = stackIsBlock(env, stack);
        env->DeleteLocalRef(stack);
        if (block) return i;
    }
    return -1;
}

// Scan main inventory slots 9..35 (hotbar is 0..8) for the first block item
int Scaffold::findInventoryBlockSlot(JNIEnv* env, jobject inv) {
    if (!inv || !s_getItem || !s_stackGetItem || !s_blockItemCls) return -1;
    for (int i = 9; i <= 35; i++) {
        jobject stack = env->CallObjectMethod(inv, s_getItem, i);
        if (!stack || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            continue;
        }
        bool block = stackIsBlock(env, stack);
        env->DeleteLocalRef(stack);
        if (block) return i;
    }
    return -1;
}

/*
 * moveStackToHotbar — real container click (ClickType.SWAP).
 *
 * Sends handleInventoryMouseClick(containerId, srcIdx, dstHotbar, SWAP, player)
 * through MultiPlayerGameMode. In the player's own inventory menu:
 *   - slot index == inventory index for main inventory slots 9..35
 *   - ClickType.SWAP with button = hotbar slot (0..8) swaps the clicked slot
 *     with that hotbar slot — exactly what we want.
 * Returns false if the JNI is not ready or no own-inventory menu is open.
 */
bool Scaffold::moveStackToHotbar(JNIEnv* env, jobject playerObj, int srcIdx, int dstHotbar) {
    if (!s_mcCls || !s_gmField || !s_handleClick || !s_clickTypeCls || !s_swapField ||
        !s_containerMenuField || !s_containerIdField) return false;

    jobject mc = CMinecraft::getInstance();
    if (!mc) return false;
    jobject gm = env->GetObjectField(mc, s_gmField);   // Minecraft.gameMode
    if (!gm || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }

    // Only click when the player's OWN inventory menu (containerId 0) is open.
    jobject menu = env->GetObjectField(playerObj, s_containerMenuField);
    if (!menu || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jint containerId = env->GetIntField(menu, s_containerIdField);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (containerId != 0) return false;   // e.g. chest open — don't click into it

    jobject swapType = env->GetStaticObjectField(s_clickTypeCls, s_swapField);
    if (!swapType || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }

    env->CallVoidMethod(gm, s_handleClick, containerId, (jint)srcIdx, (jint)dstHotbar, swapType, playerObj);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return true;
}

bool Scaffold::selectSlot(JNIEnv* env, jobject inv, int slot) {
    if (!inv || !s_setSelected || slot < 0 || slot > 8) return false;
    env->CallVoidMethod(inv, s_setSelected, (jint)slot);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return true;
}

bool Scaffold::blockIsAir(JNIEnv* env, int bx, int by, int bz) {
    if (!s_bpCls || !s_bpInit || !s_lvlCls || !s_getState || !s_stateCls || !s_isAir) return true;
    jobject worldObj = CMinecraft::getWorld();
    if (!worldObj) return true;
    jobject bp = env->NewObject(s_bpCls, s_bpInit, (jint)bx, (jint)by, (jint)bz);
    if (!bp || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return true;
    }
    jobject state = env->CallObjectMethod(worldObj, s_getState, bp);
    if (!state || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(bp);
        return true;
    }
    jboolean air = env->CallBooleanMethod(state, s_isAir);
    if (env->ExceptionCheck()) { env->ExceptionClear(); air = JNI_TRUE; }
    env->DeleteLocalRef(state);
    env->DeleteLocalRef(bp);
    return air == JNI_TRUE;
}

// ── Click state machine — no Sleep() in the tick thread ──────────────
bool Scaffold::sendRightDown() {
    HWND mcHwnd = FindWindowA("GLFW30", nullptr);
    if (!mcHwnd || GetForegroundWindow() != mcHwnd) return false;
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    return true;
}

void Scaffold::sendRightUp() {
    HWND mcHwnd = FindWindowA("GLFW30", nullptr);
    if (!mcHwnd || GetForegroundWindow() != mcHwnd) return;
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
}

// ── Smooth rotation (client-side look) ───────────────────────────────
void Scaffold::smoothRotate(float tYaw, float tPitch, float speed) {
    float diffYaw = tYaw - m_curYaw;
    while (diffYaw > 180.0f) diffYaw -= 360.0f;
    while (diffYaw < -180.0f) diffYaw += 360.0f;
    float step = speed * 0.05f;
    if (step > 1.0f) step = 1.0f;
    m_curYaw += diffYaw * step;
    m_curPitch += (tPitch - m_curPitch) * step;
    // KRITISCH: m_curYaw nach [-180,180] normalisieren. Ohne das akkumuliert
    // die Rotation über Minuten (Legit/Semi dreht um ~180°) zu Werten wie
    // -364.3 oder weniger — das liest die HUD-Richtungs-Anzeige als yaw und
    // crasht (OOB-Array-Index, siehe hud.cpp renderDirection).
    if (m_curYaw > 180.0f) m_curYaw -= 360.0f;
    else if (m_curYaw < -180.0f) m_curYaw += 360.0f;
}

bool Scaffold::placeBlock() {
    if (m_clickDown) return false;   // click already in progress
    if (!sendRightDown()) return false;  // game window not focused
    m_clickDown = true;
    m_clickDownAt = std::chrono::steady_clock::now();
    return true;
}

/*
 * placeBlockAtJNI — platzieren den Block an (bx,by,bz) durch einen echten
 * MultiPlayerGameMode.useItemOn(player, MAIN_HAND, BlockHitResult)-Aufruf.
 *
 * Damit wird das ServerboundUseItemOnPacket direkt an die Ziel-Position
 * gesendet — ganz ohne Mausklick und ohne dass der Spieler hinschauen muss.
 * Für den Rage-Modus (geradeaus laufen, kein Drehen) ist das der einzige
 * zuverlässige Weg: Ein Fake-Mausklick platziert nämlich nur dort, wo der
 * Cursor hinzeigt (bei Geradeaus-Blick also nie unter die Füße).
 *
 * HitResult: wir zielen auf die UNTERKANTE des Blocks, der ÜBER dem Ziel
 * liegt (by+1) — useItemOn platziert dann an blockPos.relative(dir) = (bx,by,bz).
 * Return: true wenn der Aufruf abgesetzt wurde.
 */
bool Scaffold::placeBlockAtJNI(JNIEnv* env, jobject playerObj, int bx, int by, int bz) {
    if (!s_useItemOn || !s_bhrInit || !s_vec3Init || !s_dirDownField || !s_mainHandField) return false;

    jobject mc = CMinecraft::getInstance();
    if (!mc || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jobject gm = env->GetObjectField(mc, s_gmField);
    if (!gm || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject dirDown = env->GetStaticObjectField(s_dirCls, s_dirDownField);
    if (!dirDown || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(gm); env->DeleteLocalRef(mc);
        return false;
    }

    jobject hand = env->GetStaticObjectField(s_handCls, s_mainHandField);
    if (!hand || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(dirDown); env->DeleteLocalRef(gm); env->DeleteLocalRef(mc);
        return false;
    }

    // BlockPos ÜBER dem Ziel (by+1) — Platzierung landet dann auf (bx,by,bz)
    jobject bp = env->NewObject(s_bpCls, s_bpInit, (jint)bx, (jint)(by + 1), (jint)bz);
    if (!bp || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(hand); env->DeleteLocalRef(dirDown); env->DeleteLocalRef(gm); env->DeleteLocalRef(mc);
        return false;
    }

    // Trefferpunkt in der Mitte des Zielblocks
    jobject loc = env->NewObject(s_vec3Cls, s_vec3Init, (jdouble)bx + 0.5, (jdouble)by + 0.5, (jdouble)bz + 0.5);
    if (!loc || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(bp); env->DeleteLocalRef(hand); env->DeleteLocalRef(dirDown);
        env->DeleteLocalRef(gm); env->DeleteLocalRef(mc);
        return false;
    }

    jobject hit = env->NewObject(s_bhrCls, s_bhrInit, loc, dirDown, bp, JNI_FALSE);
    if (!hit || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(loc); env->DeleteLocalRef(bp); env->DeleteLocalRef(hand);
        env->DeleteLocalRef(dirDown); env->DeleteLocalRef(gm); env->DeleteLocalRef(mc);
        return false;
    }

    jobject result = env->CallObjectMethod(gm, s_useItemOn, playerObj, hand, hit);
    if (env->ExceptionCheck()) env->ExceptionClear();

    // War die Platzierung erfolgreich? Nur dann gilt der Klick als "gemacht" —
    // sonst wird der Delay NICHT fortgeschrieben und der nächste Tick
    // versucht es sofort erneut (verhindert das Runterfallen bei Fehlschlag).
    bool consumed = false;
    if (result && s_irConsumes) {
        jboolean c = env->CallBooleanMethod(result, s_irConsumes);
        if (env->ExceptionCheck()) env->ExceptionClear();
        consumed = (c == JNI_TRUE);
    }
    if (result) env->DeleteLocalRef(result);

    env->DeleteLocalRef(hit);
    env->DeleteLocalRef(loc);
    env->DeleteLocalRef(bp);
    env->DeleteLocalRef(hand);
    env->DeleteLocalRef(dirDown);
    env->DeleteLocalRef(gm);
    env->DeleteLocalRef(mc);
    return consumed;
}

// ─────────────────────────────────────────────────────────────────────
void Scaffold::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    if (!sjni) initJNI(env);
    if (!s_invField || !s_getSelected || !s_getItem || !s_stackGetItem || !s_blockItemCls) return;

    env->PushLocalFrame(128);

    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) { env->PopLocalFrame(nullptr); return; }

    CEntity player(playerObj);
    double px = player.getX();
    double py = player.getY();
    double pz = player.getZ();
    float pYaw = player.getYaw();
    float pPitch = player.getPitch();

    bool fwd  = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool back = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool left = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool right= (GetAsyncKeyState('D') & 0x8000) != 0;
    bool jump = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    int mode = m_intSettings["mode"];

    // ── ⚠️ Detect warnings ─────────────────────────────────────────
    if (mode == (int)Mode::Rage && m_floatSettings["delay_min"] < 40.0f)
        addDetectWarning("Rage Delay < 40ms", true, "Sub-tick block placement is inhuman. Increase delay to above 50ms.");
    if (mode == (int)Mode::Breezily)
        addDetectWarning("Breezily detected on some ACs", true, "Auto-jump + sprint bridging is checked by vulcan/verus.");
    if (mode == (int)Mode::GodBridge && m_floatSettings["god_hold"] < 120.0f)
        addDetectWarning("GB Hold < 120ms", true, "Faster than 120ms godbridge is not humanly possible.");

    // ── Inventory + auto block switch ──────────────────────────────
    jobject inv = getInventory(env, playerObj);
    int curSlot = -1;
    if (inv) {
        curSlot = (int)env->CallIntMethod(inv, s_getSelected);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    bool curIsBlock = false;
    jobject curStack = nullptr;
    if (inv && curSlot >= 0 && curSlot <= 8) {
        curStack = env->CallObjectMethod(inv, s_getItem, curSlot);
        if (env->ExceptionCheck()) env->ExceptionClear();
        curIsBlock = stackIsBlock(env, curStack);
    }

    if (!curIsBlock && m_boolSettings["auto_switch"] && inv) {
        int slot = findBlockSlot(env, inv);
        if (slot >= 0) {
            selectSlot(env, inv, slot);
            if (curStack) env->DeleteLocalRef(curStack);
            curStack = env->CallObjectMethod(inv, s_getItem, slot);
            if (env->ExceptionCheck()) env->ExceptionClear();
            curSlot = slot;
            curIsBlock = stackIsBlock(env, curStack);
        }
    }

    // Update HUD preview data
    if (curStack) {
        m_previewName = stackName(env, curStack);
        m_previewCount = stackCount(env, curStack);
        m_hasBlock = curIsBlock;
        // deterministic color from item name
        unsigned h = 2166136261u;
        for (char c : m_previewName) { h ^= (unsigned char)c; h *= 16777619u; }
        m_previewColor = IM_COL32(60 + (h % 140), 60 + ((h >> 5) % 140), 60 + ((h >> 9) % 140), 255);
    } else {
        m_hasBlock = false;
        m_previewName.clear();
        m_previewCount = 0;
    }

    // ── No blocks in hotbar ─────────────────────────────────────────
    // 1st: InventoryMove — pull a block stack from the main inventory into
    //      the hotbar via a real container click (ClickType.SWAP).
    // 2nd: if nothing is available anywhere, hold the player in place so
    //      you cannot fall off the edge until blocks arrive / module off.
    if (!m_hasBlock) {
        bool invMoved = false;
        if (m_boolSettings["inventory_move"] && inv) {
            auto now = std::chrono::steady_clock::now();
            auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastInvMove).count();
            if (el >= (int)m_floatSettings["inv_move_delay"]) {
                int src = findInventoryBlockSlot(env, inv);   // main inv 9..35
                if (src >= 0) {
                    int dst = (curSlot >= 0 && curSlot <= 8) ? curSlot : 0;
                    if (moveStackToHotbar(env, playerObj, src, dst)) {
                        invMoved = true;
                        m_lastInvMove = now;   // only stamp on success → fast retry on failure
                    }
                }
            }
        }
        if (!invMoved) {
            // Stand still so you cannot fall off the edge
            player.setDeltaMovement(0.0, 0.0, 0.0);
            if (m_sneakDown) { keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0); m_sneakDown = false; }
            if (m_sprintDown) { keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0); m_sprintDown = false; }
        }
        if (curStack) env->DeleteLocalRef(curStack);
        env->PopLocalFrame(nullptr);
        return;
    }
    if (curStack) env->DeleteLocalRef(curStack);

    // ── Movement direction (relative to a FROZEN anchor yaw) ─────────
    // The movement keys are interpreted against m_baseYaw, never against
    // the live camera yaw (pYaw). The camera yaw is rotated by this module
    // itself, so deriving the movement from pYaw creates a feedback loop:
    // the camera turns toward the target block → pYaw changes → the target
    // block moves → the camera turns again … the classic "spins in circles
    // while holding W" bug. The anchor only re-anchors when the player
    // actually changes direction keys, so while a key is held the target
    // stays fixed, the camera converges and holds (one clean 180° flip).
    int keyState = (fwd ? 1 : 0) | (back ? 2 : 0) | (left ? 4 : 0) | (right ? 8 : 0);
    if (!m_baseInit || (keyState != 0 && keyState != m_prevKeyState)) {
        m_baseYaw = pYaw;
        m_baseInit = true;
    }
    if (keyState != 0) m_prevKeyState = keyState;

    double yr = m_baseYaw * (3.14159265358979323846 / 180.0);
    double sy = sin(yr), cy = cos(yr);
    double mx = 0, mz = 0;
    if (fwd)  { mx -= sy; mz += cy; }
    if (back) { mx += sy; mz -= cy; }
    if (left) { mx += cy; mz += sy; }
    if (right){ mx -= cy; mz -= sy; }
    double len = sqrt(mx*mx + mz*mz);
    if (len > 0) { mx /= len; mz /= len; }

    bool moving = fwd || back || left || right || jump;

    // ── Backward bridging (Legit / Semi) ─────────────────────────────
    // The user looks in the direction they want to bridge. We rotate the
    // character 180° so they walk BACKWARD over their own bridge while the
    // camera faces the blocks being placed (classic sneak-release bridging).
    // GodBridge / Breezily / Eagle bridge FORWARD (no flip).
    bool backward = (mode == (int)Mode::Legit || mode == (int)Mode::Semi);
    if (backward) {
        mx = -mx; mz = -mz;
    }

    // ── Target block selection ──────────────────────────────────────
    int tx, ty, tz;
    if (mode == (int)Mode::Rage) {
        // Rage: Block DIREKT UNTER dem Spieler auf Fußhöhe-1 — kein Block
        // voraus, keine Treppe. Der Block wird dort platziert, wo der Spieler
        // gerade steht. Mit dem halbierten Delay (35-70 ms) wird jede Bewegung
        // sofort unterbaut, auch an der Kante.
        tx = (int)floor(px);
        ty = (int)floor(py) - 1;
        tz = (int)floor(pz);
    } else if (mode == (int)Mode::Breezily) {
        // Breezily: 1 Block VORAUS (Speed-Jump-Bridging). Der Block muss
        // VOR dem Spieler liegen, da er beim Sprint-Jump den Landepunkt
        // voraus braucht.
        tx = (int)floor(px + mx);
        ty = (int)floor(py) - 1;
        tz = (int)floor(pz + mz);
    } else if (mode == (int)Mode::GodBridge) {
        // One block ahead at foot level — you look straight down at your
        // feet and the block appears right under/behind your next step.
        tx = (int)floor(px + mx * 0.35);
        ty = (int)floor(py) - 1;
        tz = (int)floor(pz + mz * 0.35);
    } else if (mode == (int)Mode::Eagle) {
        // One step ahead at foot level - 1 (forward, no rotation flip).
        tx = (int)floor(px + mx * 0.9);
        ty = (int)floor(py) - 1;
        tz = (int)floor(pz + mz * 0.9);
    } else {
        // Legit/Semi: one step ahead in movement direction at foot level - 1
        if (moving) {
            tx = (int)floor(px + mx * 0.75);
            ty = (int)floor(py) - 1;
            tz = (int)floor(pz + mz * 0.75);
        } else {
            tx = (int)floor(px);
            ty = (int)floor(py) - 1;
            tz = (int)floor(pz);
        }
    }

    if (m_boolSettings["tower"] && jump) {
        tx = (int)floor(px); tz = (int)floor(pz);
    }

    // Safety: the block directly under the player must never be air
    int underX = (int)floor(px), underY = (int)floor(py) - 1, underZ = (int)floor(pz);
    bool underAir = blockIsAir(env, underX, underY, underZ);

    // Final target: prefer under-player block when it is air, else the step target
    int fx = tx, fy = ty, fz = tz;
    bool canPlace = underAir;
    if (!canPlace) {
        // step target must be air to be placeable
        canPlace = blockIsAir(env, tx, ty, tz);
    }

    // ── Tower: paced jump whenever space is held and on the ground ──
    // Hoisted OUT of the placement gate — otherwise Tower would do
    // nothing while standing on solid ground (no placement needed there).
    auto towerNow = std::chrono::steady_clock::now();
    if (m_boolSettings["tower"] && jump && player.isOnGround()) {
        auto jEl = std::chrono::duration_cast<std::chrono::milliseconds>(towerNow - m_lastJump).count();
        if (jEl >= (int)m_floatSettings["tower_delay"]) {
            player.setJumping(true);
            m_lastJump = towerNow;
        }
    }

    if (!canPlace) {
        // Nothing to place this tick — still finish any pending click and
        // release any held keys (never leak sneak/sprint through a return).
        if (m_clickDown) { sendRightUp(); m_clickDown = false; }
        if (m_sneakDown) { keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0); m_sneakDown = false; }
        if (m_sprintDown) { keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0); m_sprintDown = false; }
        env->PopLocalFrame(nullptr);
        return;
    }
    // Sicherheits-Override: Block direkt unter den Füßen NUR wenn der Spieler
    // stillsteht. Beim Bewegen (Breezily) ist der Block VORAUS das richtige
    // Ziel — ein Block unter einem fallenden Spieler wird wegen AABB-Kollision
    // vom Client verworfen (genau das „Runterfallen" trotz Rage).
    // Rage (= unter dem Spieler) braucht den Override nicht.
    bool rageMoving = (mode == (int)Mode::Rage);
    if (underAir && !rageMoving) { fx = underX; fy = underY; fz = underZ; }

    // ── Rotation: aim at the target block ──────────────────────────
    // The camera faces the block being placed. In Legit/Semi this is
    // ~180° from the player's original look (backward bridging).
    double bx = fx + 0.5, by = fy + 1.0, bz = fz + 0.5;
    double dx = bx - px, dy = by - (py + 1.62), dz = bz - pz;
    double hd = sqrt(dx*dx + dz*dz);
    float tYaw = (float)(atan2(-dx, dz) * 180.0 / 3.14159265358979323846);
    float tPitch = (float)(-atan2(dy, hd) * 180.0 / 3.14159265358979323846);

    // Rage: KEINE Rotation — der Spieler schaut einfach geradeaus und läuft.
    // Die Blöcke werden per JNI (useItemOn) direkt unter die Füße gesetzt.
    bool rageMode = (mode == (int)Mode::Rage);
    if (!rageMode) {
        if (!m_hasTgt) { m_curYaw = pYaw; m_curPitch = pPitch; m_hasTgt = true; }
        smoothRotate(tYaw, tPitch, m_floatSettings["rot_speed"]);
        player.setYaw(m_curYaw);
        player.setPitch(m_curPitch);
    }

    // ── Sneak / sprint per mode ────────────────────────────────────
    HWND mcHwnd = FindWindowA("GLFW30", nullptr);
    bool focused = mcHwnd && GetForegroundWindow() == mcHwnd;

    if (mode == (int)Mode::Legit) {
        // classic sneak/release cycle
        auto now = std::chrono::steady_clock::now();
        auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSneakFlip).count();
        float holdMs = m_floatSettings["sneak_hold"];
        float relMs  = m_floatSettings["sneak_release"];

        if (m_sneakDown) {
            if (el >= holdMs) {
                if (focused) keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
                m_sneakDown = false;
                m_lastSneakFlip = now;
            }
        } else {
            if (el >= relMs) {
                if (focused) keybd_event(VK_LSHIFT, 0, 0, 0);
                m_sneakDown = true;
                m_lastSneakFlip = now;
            }
        }
    } else if (mode == (int)Mode::GodBridge) {
        // GodBridge crouch-timing: hold sneak while gliding, release briefly
        // to place the next block, then re-sneak. Own timings (god_hold/rel).
        auto now = std::chrono::steady_clock::now();
        auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSneakFlip).count();
        float holdMs = m_floatSettings["god_hold"];
        float relMs  = m_floatSettings["god_release"];

        if (m_sneakDown) {
            if (el >= holdMs) {
                if (focused) keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
                m_sneakDown = false;
                m_lastSneakFlip = now;
            }
        } else {
            if (el >= relMs) {
                if (focused) keybd_event(VK_LSHIFT, 0, 0, 0);
                m_sneakDown = true;
                m_lastSneakFlip = now;
            }
        }
    } else if (mode == (int)Mode::Eagle) {
        // Eagle = auto-sneak ONLY at block edges. If the block one step
        // ahead (at foot level) is air, we're at an edge → sneak. Otherwise
        // walk freely without sneaking. Skip the checks entirely when idle.
        if (moving) {
            int aheadX = (int)floor(px + mx);
            int aheadY = (int)floor(py) - 1;
            int aheadZ = (int)floor(pz + mz);
            bool atEdge = blockIsAir(env, aheadX, aheadY, aheadZ) ||
                          blockIsAir(env, aheadX, aheadY + 1, aheadZ);
            if (atEdge && !m_sneakDown) {
                if (focused) keybd_event(VK_LSHIFT, 0, 0, 0);
                m_sneakDown = true;
            } else if (!atEdge && m_sneakDown) {
                if (focused) keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
                m_sneakDown = false;
            }
        } else if (m_sneakDown) {
            if (focused) keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
            m_sneakDown = false;
        }
    } else if (mode == (int)Mode::Breezily) {
        // Breezily: auto-sprint (hold Ctrl) while moving forward.
        bool wantSprint = fwd && !back;
        if (wantSprint && !m_sprintDown) {
            if (focused) { keybd_event(VK_LCONTROL, 0, 0, 0); m_sprintDown = true; }  // only mark when actually sent
        } else if (!wantSprint && m_sprintDown) {
            if (focused) keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0);
            m_sprintDown = false;
        }
        if (m_sneakDown) {
            keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
            m_sneakDown = false;
        }
    } else {
        // Semi / Rage — release anything another mode may have held
        if (m_sneakDown) { keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0); m_sneakDown = false; }
        if (m_sprintDown) { keybd_event(VK_LCONTROL, 0, KEYEVENTF_KEYUP, 0); m_sprintDown = false; }
    }

    // ── Breezily: auto-jump while sprinting forward ────────────────
    if (mode == (int)Mode::Breezily && fwd && player.isOnGround()) {
        auto now = std::chrono::steady_clock::now();
        auto jEl = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastJump).count();
        if (jEl >= (int)m_floatSettings["breezily_jump"]) {
            player.setJumping(true);
            m_lastJump = now;
        }
    }

    // ── Placement with randomized timing + two-phase click ─────────
    // GodBridge authenticity: only place while the sneak is RELEASED (the
    // crouch-timing window), exactly like manual godbridging.
    auto now = std::chrono::steady_clock::now();
    if (rageMode) {
        // Rage v3: NUR JNI-Placement (useItemOn). Kein Mouse-Click-Fallback —
        // mouse_event platziert dort wo der Cursor hinzeigt (bei Geradeauslauf
        // = vor dem Spieler), was die „Treppe nach oben" verursacht.
        //
        // JNI sendet das ServerboundUseItemOnPacket direkt ans Ziel (unter den
        // Spieler) — dafür braucht es keine Kameradrehung, keinen Cursor.
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPlace).count();
        static int rageFailStreak = 0;
        if (ms >= m_placeDelay) {
            bool placed = false;
            if (s_useItemOn && s_bhrInit && s_vec3Init && s_dirDownField && s_mainHandField) {
                placed = placeBlockAtJNI(env, playerObj, fx, fy, fz);
                if (placed) {
                    m_lastPlace = now;
                    m_placeDelay = 6 + (int)(m_rng() % 5u);  // 6-10ms, nahezu jeder Tick
                    rageFailStreak = 0;
                } else {
                    // Placement blocked (already solid, invalid target, etc.) —
                    // retry NEXT tick. Do NOT advance the delay so we try
                    // again immediately (prevents falling at the edge).
                    rageFailStreak++;
                    if (rageFailStreak > 12) {
                        // 12+ ticks of failing = edge case. Slightly back off.
                        m_placeDelay = 30 + (int)(m_rng() % 10u);
                        rageFailStreak = 0;
                    }
                }
            }
        }
        // Safety: release any stuck mouse buttons (never leak input)
        if (m_clickDown) { sendRightUp(); m_clickDown = false; }
        // Edge safety: if falling (underAir) and placement keeps failing,
        // freeze the player in place — never let them fall off.
        if (underAir && rageFailStreak > 3) {
            player.setDeltaMovement(0.0, 0.0, 0.0);
        }
    } else {
        bool gbGate = (mode == (int)Mode::GodBridge) ? !m_sneakDown : true;
        if (m_clickDown) {
            auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_clickDownAt).count();
            if (el >= 25) {  // release after ~1 tick so the click registers
                sendRightUp();
                m_clickDown = false;
            }
        } else if (gbGate) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPlace).count();
            if (ms >= m_placeDelay) {
                if (placeBlock()) {
                    m_lastPlace = now;
                    int range = (int)(m_floatSettings["delay_max"] - m_floatSettings["delay_min"]);
                    m_placeDelay = (int)m_floatSettings["delay_min"] + (int)(m_rng() % (unsigned)(range > 0 ? range : 1));
                }
            }
        }
    }

    env->PopLocalFrame(nullptr);
}

// ─────────────────────────────────────────────────────────────────────
// HUD: block preview above the hotbar
// ─────────────────────────────────────────────────────────────────────
void Scaffold::onRender() {
    if (!m_enabled) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    float cx = disp.x * 0.5f;
    float y = disp.y - 96.0f;   // just above the hotbar

    const float w = 190.0f, h = 30.0f;
    float x = cx - w * 0.5f;

    // Panel background
    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(16, 17, 22, 200), 6.0f);
    draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, 36), 6.0f);

    // Block icon (colored square)
    draw->AddRectFilled(ImVec2(x + 5, y + 5), ImVec2(x + 25, y + 25), m_previewColor, 3.0f);
    draw->AddRect(ImVec2(x + 5, y + 5), ImVec2(x + 25, y + 25), IM_COL32(255, 255, 255, 60), 3.0f);

    // Text
    char buf[160];
    if (m_hasBlock && !m_previewName.empty()) {
        snprintf(buf, sizeof buf, "%s  x%d", m_previewName.c_str(), m_previewCount);
        draw->AddText(ImVec2(x + 30, y + 8), IM_COL32(235, 235, 245, 255), buf);
    } else {
        draw->AddText(ImVec2(x + 30, y + 8), IM_COL32(255, 90, 90, 255), "No Blocks!");
    }

    // Mode label
    const char* modeLbl = "LEGIT";
    int mode = m_intSettings["mode"];
    if (mode == 1) modeLbl = "SEMI";
    else if (mode == 2) modeLbl = "RAGE";
    else if (mode == 3) modeLbl = "GODBRIDGE";
    else if (mode == 4) modeLbl = "BREEZILY";
    else if (mode == 5) modeLbl = "EAGLE";
    ImVec2 ts = ImGui::CalcTextSize(modeLbl);
    draw->AddText(ImVec2(x + w - ts.x - 8, y + 8), IM_COL32(140, 160, 255, 220), modeLbl);
}
