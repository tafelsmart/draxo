#include "pch.h"
#include "core/strcrypt.h"
#include "modules/autofish.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"

// ═══════════════════════════════════════════════════════════════════════
//  AutoFish — Automatic fishing
//
//  Detects fishing rod in hand, casts, waits for bobber, detects bite
//  via deltaMovement spike, reels in, re-casts.
// ═══════════════════════════════════════════════════════════════════════

AutoFish::AutoFish() : Module("AutoFish", ModuleCategory::PLAYER, 0,
    "Automatically fishes: casts, waits for a bite, reels in, re-casts. "
    "Idle-friendly — no input needed.") {
    setTickInterval(2);

    defineFloat("cast_delay",  "Cast Delay (ms)",   600.0f, 200.0f, 2000.0f, "%.0f");
    defineFloat("reel_delay",  "Reel Delay (ms)",   150.0f,   0.0f,  500.0f, "%.0f");
    defineFloat("cooldown",    "Cooldown (ms)",    1200.0f, 500.0f, 3000.0f, "%.0f");
    defineBool("notify",       "Notifications",     true);

    addSearchTag("fish");
    addSearchTag("fishing");
    addSearchTag("bobber");
    addSearchTag("idle");
}

void AutoFish::onEnable() {
    m_phase    = IDLE;
    m_tick     = 0;
    m_cooldownTicks = 0;
    m_hasRod   = false;
}

// ═══════════════════════════════════════════════════════════════════════
//  JNI Init
// ═══════════════════════════════════════════════════════════════════════

bool AutoFish::initJNI(JNIEnv* env) {
    if (s_jni) return true;

    // FishingHook class — try all known paths
    if (!s_fishHookCls) {
        s_fishHookCls = JvmWrapper::findClass("net/minecraft/world/entity/projectile/FishingHook");
        if (!s_fishHookCls)
            s_fishHookCls = JvmWrapper::findClass("net/minecraft/entity/projectile/FishingBobberEntity");
        if (!s_fishHookCls)
            s_fishHookCls = JvmWrapper::findClass("net/minecraft/entity/item/EntityFishingHook");
    }

    // Entity.getDeltaMovement()
    if (!s_entCls) s_entCls = JvmWrapper::findClass(Mappings::Entity_Class);
    if (s_entCls && !s_getDelta)
        s_getDelta = env->GetMethodID(s_entCls, Mappings::Entity_getDeltaMovement, Mappings::Entity_getDeltaMovement_Sig);

    // Entity iteration (ClientLevel.entitiesForRendering)
    if (!s_lvlCls) s_lvlCls = JvmWrapper::findClass(Mappings::ClientLevel_Class);
    if (s_lvlCls && !s_entities)
        s_entities = env->GetMethodID(s_lvlCls, Mappings::ClientLevel_entitiesForRendering, Mappings::ClientLevel_entitiesForRendering_Sig);

    if (!s_iterCls) s_iterCls = JvmWrapper::findClass("java/util/Iterator");
    if (s_iterCls) {
        if (!s_iterHas)  s_iterHas  = env->GetMethodID(s_iterCls, "hasNext", "()Z");
        if (!s_iterNext) s_iterNext = env->GetMethodID(s_iterCls, "next",    "()Ljava/lang/Object;");
    }

    // Inventory + item (for fishing rod check)
    if (!s_playerCls) s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerCls && !s_invFld)
        s_invFld = env->GetFieldID(s_playerCls, Mappings::Player_inventory, Mappings::Player_inventory_Sig);

    if (!s_invCls) s_invCls = JvmWrapper::findClass(Mappings::Inventory_Class);
    if (s_invCls) {
        if (!s_getItem) s_getItem = env->GetMethodID(s_invCls, Mappings::Inventory_getItem, Mappings::Inventory_getItem_Sig);
        if (!s_getSlot) s_getSlot = env->GetMethodID(s_invCls, Mappings::Inventory_getSelectedSlot, Mappings::Inventory_getSelectedSlot_Sig);
        if (!s_setSlot) s_setSlot = env->GetMethodID(s_invCls, Mappings::Inventory_setSelectedSlot, Mappings::Inventory_setSelectedSlot_Sig);
    }

    if (!s_stackCls) s_stackCls = JvmWrapper::findClass(Mappings::ItemStack_Class);
    if (s_stackCls) {
        if (!s_stackEmpty)   s_stackEmpty   = env->GetMethodID(s_stackCls, Mappings::ItemStack_isEmpty, Mappings::ItemStack_isEmpty_Sig);
        if (!s_stackGetItem) s_stackGetItem = env->GetMethodID(s_stackCls, Mappings::ItemStack_getItem, Mappings::ItemStack_getItem_Sig);
    }

    if (!s_itemCls) s_itemCls = JvmWrapper::findClass(Mappings::Item_Class);
    if (s_itemCls && !s_itemDescId)
        s_itemDescId = env->GetMethodID(s_itemCls, Mappings::Block_getDescriptionId, Mappings::Block_getDescriptionId_Sig);

    s_jni = s_fishHookCls && s_entCls && s_getDelta && s_lvlCls && s_entities
         && s_iterCls && s_iterHas && s_iterNext && s_playerCls && s_invFld
         && s_getItem && s_stackGetItem && s_itemDescId;
    return s_jni;
}

// ═══════════════════════════════════════════════════════════════════════
//  hasFishingRod — check if selected hotbar slot is a fishing rod
// ═══════════════════════════════════════════════════════════════════════

bool AutoFish::hasFishingRod(JNIEnv* env, jobject inv) {
    if (!inv || !s_getSlot || !s_getItem || !s_stackEmpty || !s_stackGetItem || !s_itemDescId)
        return false;

    jint slot = env->CallIntMethod(inv, s_getSlot);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jobject stack = env->CallObjectMethod(inv, s_getItem, slot);
    if (!stack || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jboolean empty = env->CallBooleanMethod(stack, s_stackEmpty);
    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(stack); return false; }
    if (empty) { env->DeleteLocalRef(stack); return false; }

    jobject item = env->CallObjectMethod(stack, s_stackGetItem);
    env->DeleteLocalRef(stack);
    if (!item || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jstring descStr = (jstring)env->CallObjectMethod(item, s_itemDescId);
    env->DeleteLocalRef(item);
    if (!descStr || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    const char* desc = env->GetStringUTFChars(descStr, nullptr);
    if (!desc) { env->DeleteLocalRef(descStr); return false; }

    bool isRod = (strstr(desc, "fishing_rod") != nullptr);

    env->ReleaseStringUTFChars(descStr, desc);
    env->DeleteLocalRef(descStr);
    return isRod;
}

// ═══════════════════════════════════════════════════════════════════════
//  findBobber — iterate entitiesForRendering, find FishingHook
// ═══════════════════════════════════════════════════════════════════════

jobject AutoFish::findBobber(JNIEnv* env, jobject world, int playerId) {
    if (!world || !s_fishHookCls || !s_entities || !s_iterHas || !s_iterNext)
        return nullptr;

    jobject iterable = env->CallObjectMethod(world, s_entities);
    if (!iterable || env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }

    // Get Iterator
    static jmethodID s_iterMethod = nullptr;
    static jclass    s_iterableCls = nullptr;
    if (!s_iterableCls) s_iterableCls = JvmWrapper::findClass("java/lang/Iterable");
    if (s_iterableCls && !s_iterMethod)
        s_iterMethod = env->GetMethodID(s_iterableCls, "iterator", "()Ljava/util/Iterator;");

    if (!s_iterMethod) { env->DeleteLocalRef(iterable); return nullptr; }

    jobject iter = env->CallObjectMethod(iterable, s_iterMethod);
    env->DeleteLocalRef(iterable);
    if (!iter || env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }

    jobject found = nullptr;

    while (env->CallBooleanMethod(iter, s_iterHas)) {
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }

        jobject entity = env->CallObjectMethod(iter, s_iterNext);
        if (!entity || env->ExceptionCheck()) { env->ExceptionClear(); continue; }

        if (env->IsInstanceOf(entity, s_fishHookCls)) {
            // Verify this bobber belongs to us (check owner entity ID)
            // FishHook has method getPlayerOwner() or field owner
            // For simplicity, just take the first bobber found near the player
            // More robust: check getX/getZ distance to player

            static jmethodID s_getX = nullptr;
            static jmethodID s_getZ = nullptr;
            if (!s_getX) {
                jclass entC = env->GetObjectClass(entity);
                s_getX = env->GetMethodID(entC, Mappings::Entity_getX, Mappings::Entity_getX_Sig);
                s_getZ = env->GetMethodID(entC, Mappings::Entity_getZ, Mappings::Entity_getZ_Sig);
                env->DeleteLocalRef(entC);
            }

            if (s_getX && s_getZ) {
                jdouble ex = env->CallDoubleMethod(entity, s_getX);
                jdouble ez = env->CallDoubleMethod(entity, s_getZ);
                // The bobber is typically within ~10 blocks of the player
                // We don't have player position here, so just accept any bobber
                found = entity;
                break;
            }
            found = entity;
            break;
        }
        env->DeleteLocalRef(entity);
    }

    env->DeleteLocalRef(iter);
    return found;
}

// ═══════════════════════════════════════════════════════════════════════
//  isBiting — check if bobber's deltaMovement.y indicates a bite
// ═══════════════════════════════════════════════════════════════════════

bool AutoFish::isBiting(JNIEnv* env, jobject bobber) {
    if (!bobber || !s_getDelta) return false;

    static jclass s_vec3Cls = nullptr;
    static jmethodID s_getY = nullptr;

    jobject delta = env->CallObjectMethod(bobber, s_getDelta);
    if (!delta || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    if (!s_getY) {
        jclass dc = env->GetObjectClass(delta);
        s_getY = env->GetMethodID(dc, "y", "()D");
        env->DeleteLocalRef(dc);
    }

    bool biting = false;
    if (s_getY) {
        jdouble dy = env->CallDoubleMethod(delta, s_getY);
        if (env->ExceptionCheck()) env->ExceptionClear();
        // A bite causes a downward spike in the bobber's motion
        // Normal bobber: ~0.0 motionY. Bite: < -0.08 (fish pulls it under)
        biting = (dy < -0.06);
    }

    env->DeleteLocalRef(delta);
    return biting;
}

// ═══════════════════════════════════════════════════════════════════════
//  doCast — right-click to cast the fishing rod
// ═══════════════════════════════════════════════════════════════════════

void AutoFish::doCast() {
    keybd_event(VK_RBUTTON, 0, 0, 0);
    keybd_event(VK_RBUTTON, 0, KEYEVENTF_KEYUP, 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  doReel — right-click to reel in
// ═══════════════════════════════════════════════════════════════════════

void AutoFish::doReel() {
    keybd_event(VK_RBUTTON, 0, 0, 0);
    keybd_event(VK_RBUTTON, 0, KEYEVENTF_KEYUP, 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  onUpdate — state machine
// ═══════════════════════════════════════════════════════════════════════

void AutoFish::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    if (!initJNI(env)) return;

    // Focus check — don't fish when alt-tabbed
    HWND h = FindWindowA("GLFW30", nullptr);
    if (!h || GetForegroundWindow() != h) return;

    // ── Get player + inventory ──────────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    static jfieldID s_playerFld = nullptr;
    static jfieldID s_levelFld  = nullptr;
    if (!s_playerFld) {
        jclass mcC = env->GetObjectClass(mc);
        s_playerFld = env->GetFieldID(mcC, Mappings::MC_player, Mappings::MC_player_Sig);
        s_levelFld  = env->GetFieldID(mcC, Mappings::MC_level,  Mappings::MC_level_Sig);
        env->DeleteLocalRef(mcC);
    }

    jobject playerObj = env->GetObjectField(mc, s_playerFld);
    jobject worldObj  = env->GetObjectField(mc, s_levelFld);
    env->DeleteLocalRef(mc);

    if (!playerObj || !worldObj || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (playerObj) env->DeleteLocalRef(playerObj);
        if (worldObj)  env->DeleteLocalRef(worldObj);
        return;
    }

    CEntity player(playerObj);

    // ── Check if player has fishing rod ─────────────────────────────
    jobject inv = env->GetObjectField(playerObj, s_invFld);
    if (!inv) {
        env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    m_hasRod = hasFishingRod(env, inv);
    env->DeleteLocalRef(inv);

    if (!m_hasRod) {
        m_phase = IDLE;
        env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(playerObj);
        return;
    }

    // ── State machine ───────────────────────────────────────────────
    m_tick++;

    float castMs  = m_floatSettings["cast_delay"];
    float reelMs  = m_floatSettings["reel_delay"];
    float coolMs  = m_floatSettings["cooldown"];
    int castTicks  = (int)(castMs / 50.0f);  // 20 tps → ms / 50 = ticks
    int reelTicks  = (int)(reelMs / 50.0f);
    int coolTicks  = (int)(coolMs / 50.0f);
    if (castTicks  < 1) castTicks  = 1;
    if (reelTicks  < 1) reelTicks  = 1;
    if (coolTicks  < 1) coolTicks  = 1;

    switch (m_phase) {

    case IDLE:
        // Cast immediately
        doCast();
        m_phase = CASTING;
        m_tick  = 0;
        break;

    case CASTING:
        // Wait for bobber to appear
        if (m_tick > 10) {  // give server ~500ms to spawn bobber
            jobject bobber = findBobber(env, worldObj, player.getId());
            if (bobber) {
                m_phase = WAITING;
                m_tick  = 0;
                env->DeleteLocalRef(bobber);
            } else if (m_tick > 40) {
                // No bobber after 2 seconds — re-cast
                doCast();
                m_tick = 0;
            }
        }
        break;

    case WAITING:
        // Monitor bobber for bite
        {
            jobject bobber = findBobber(env, worldObj, player.getId());
            if (!bobber) {
                // Bobber gone — fish caught it or we reeled too late
                m_phase = COOLDOWN;
                m_cooldownTicks = coolTicks;
                m_tick = 0;
            } else if (isBiting(env, bobber)) {
                // Fish on!
                doReel();
                m_phase = REELING;
                m_tick  = 0;
                env->DeleteLocalRef(bobber);
            } else {
                env->DeleteLocalRef(bobber);
            }
        }
        break;

    case REELING:
        // Wait briefly for reel, then cooldown
        if (m_tick >= reelTicks) {
            m_phase = COOLDOWN;
            m_cooldownTicks = coolTicks;
            m_tick = 0;
        }
        break;

    case COOLDOWN:
        if (m_tick >= m_cooldownTicks) {
            m_phase = IDLE;
            m_tick  = 0;
        }
        break;
    }

    env->DeleteLocalRef(worldObj);
    env->DeleteLocalRef(playerObj);
}
