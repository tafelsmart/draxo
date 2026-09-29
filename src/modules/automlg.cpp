#include "pch.h"
#include "core/strcrypt.h"
#include "modules/automlg.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"

// ═══════════════════════════════════════════════════════════════════════
//  AutoMLG — Automatic water bucket / hay bale clutch
//
//  Detects rapid falling, finds water bucket or hay bale in hotbar,
//  aims straight down, and places via GameMode.useItemOn().
// ═══════════════════════════════════════════════════════════════════════

AutoMLG::AutoMLG() : Module("AutoMLG", ModuleCategory::PLAYER, 0,
    "Automatically places water bucket or hay bale before hitting "
    "the ground to negate fall damage.") {
    setTickInterval(1);

    defineMode("mode",     "Mode",     0, {"Legit", "Rage"});
    defineFloat("min_fall","Min Fall",  4.0f, 2.0f, 10.0f, "%.1f blocks");
    defineFloat("max_fall","Max Fall", 30.0f, 5.0f, 60.0f, "%.0f blocks");
    defineBool("water",    "Water Bucket",    true);
    defineBool("hay",      "Hay Bale",        true);
    defineBool("auto_swap","Auto-switch back",true);
    defineBool("notify",   "Notification",    true);

    addSearchTag("mlg");
    addSearchTag("water");
    addSearchTag("clutch");
    addSearchTag("fall");
}

void AutoMLG::onEnable() {
    m_mlgSlot   = -1;
    m_prevSlot  = -1;
    m_mlgActive = false;
    m_tickWait  = 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  JNI Init — one-shot cache of all method/field IDs
// ═══════════════════════════════════════════════════════════════════════

bool AutoMLG::initJNI(JNIEnv* env) {
    if (s_jni) return true;

    // Player.inventory
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

    // GameMode.useItemOn
    if (!s_mcCls) s_mcCls = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (s_mcCls && !s_gmFld)
        s_gmFld = env->GetFieldID(s_mcCls, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);

    if (!s_gmCls) s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
    if (s_gmCls && !s_useItemOn)
        s_useItemOn = env->GetMethodID(s_gmCls, Mappings::GameMode_useItemOn, Mappings::GameMode_useItemOn_Sig);

    // BlockHitResult(Vec3, Direction, BlockPos, boolean)
    if (!s_bhrCls) s_bhrCls = JvmWrapper::findClass(Mappings::BlockHitResult_Class);
    if (s_bhrCls && !s_bhrInit)
        s_bhrInit = env->GetMethodID(s_bhrCls, Mappings::BlockHitResult_Init, Mappings::BlockHitResult_Init_Sig);

    if (!s_vec3Cls) s_vec3Cls = JvmWrapper::findClass(Mappings::Vec3_Class);
    if (s_vec3Cls && !s_vec3Init)
        s_vec3Init = env->GetMethodID(s_vec3Cls, Mappings::Vec3_Init, Mappings::Vec3_Init_Sig);

    if (!s_bpCls) s_bpCls = JvmWrapper::findClass(Mappings::BlockPos_Class);
    if (s_bpCls && !s_bpInit)
        s_bpInit = env->GetMethodID(s_bpCls, Mappings::BlockPos_Init, Mappings::BlockPos_Init_Sig);

    if (!s_dirCls) s_dirCls = JvmWrapper::findClass(Mappings::Direction_Class);
    if (s_dirCls && !s_dirUp)
        s_dirUp = env->GetStaticFieldID(s_dirCls, Mappings::Direction_UP, Mappings::Direction_UP_Sig);

    if (!s_handCls) s_handCls = JvmWrapper::findClass(Mappings::InteractionHand_Class);
    if (s_handCls && !s_mainHand)
        s_mainHand = env->GetStaticFieldID(s_handCls, Mappings::InteractionHand_MAIN_HAND, Mappings::InteractionHand_MAIN_HAND_Sig);

    // Block scanning for ground detection
    if (!s_lvlCls) s_lvlCls = JvmWrapper::findClass(Mappings::Level_Class);
    if (s_lvlCls && !s_getState)
        s_getState = env->GetMethodID(s_lvlCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);

    if (!s_bsCls) s_bsCls = JvmWrapper::findClass(Mappings::BlockState_Class);
    if (s_bsCls && !s_bs_isAir)
        s_bs_isAir = env->GetMethodID(s_bsCls, Mappings::BlockStateBase_isAir, Mappings::BlockStateBase_isAir_Sig);

    if (!s_mbpCls) s_mbpCls = JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
    if (s_mbpCls && !s_mbpInit)
        s_mbpInit = env->GetMethodID(s_mbpCls, Mappings::MutableBlockPos_Init, Mappings::MutableBlockPos_Init_Sig);

    s_jni = s_playerCls && s_invFld && s_invCls && s_getItem
         && s_useItemOn && s_bhrInit && s_vec3Init && s_bpInit
         && s_dirUp && s_mainHand && s_getState && s_mbpInit;
    return s_jni;
}

// ═══════════════════════════════════════════════════════════════════════
//  findMLGItem — scan hotbar 0-8 for water bucket or hay bale
// ═══════════════════════════════════════════════════════════════════════

int AutoMLG::findMLGItem(JNIEnv* env, jobject inv) {
    if (!inv || !s_getItem || !s_stackEmpty || !s_stackGetItem || !s_itemDescId) return -1;

    bool wantWater = m_boolSettings["water"];
    bool wantHay   = m_boolSettings["hay"];
    if (!wantWater && !wantHay) return -1;

    for (int slot = 0; slot < 9; slot++) {
        jobject stack = env->CallObjectMethod(inv, s_getItem, (jint)slot);
        if (!stack || env->ExceptionCheck()) { env->ExceptionClear(); continue; }

        jboolean empty = env->CallBooleanMethod(stack, s_stackEmpty);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(stack); continue; }
        if (empty) { env->DeleteLocalRef(stack); continue; }

        jobject item = env->CallObjectMethod(stack, s_stackGetItem);
        env->DeleteLocalRef(stack);
        if (!item || env->ExceptionCheck()) { env->ExceptionClear(); continue; }

        jstring descStr = (jstring)env->CallObjectMethod(item, s_itemDescId);
        env->DeleteLocalRef(item);
        if (!descStr || env->ExceptionCheck()) { env->ExceptionClear(); continue; }

        const char* desc = env->GetStringUTFChars(descStr, nullptr);
        if (!desc) { env->DeleteLocalRef(descStr); continue; }

        int result = -1;
        if (wantWater && strstr(desc, "water_bucket")) result = slot;
        else if (wantHay && strstr(desc, "hay_block")) result = slot;

        env->ReleaseStringUTFChars(descStr, desc);
        env->DeleteLocalRef(descStr);

        if (result >= 0) return result;
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════
//  isBlockSolid — MutableBlockPos → getBlockState → !isAir
// ═══════════════════════════════════════════════════════════════════════

bool AutoMLG::isBlockSolid(JNIEnv* env, jobject world, int bx, int by, int bz) {
    if (!world || !s_mbpInit || !s_getState || !s_bs_isAir) return false;

    jobject mbp = env->NewObject(s_mbpCls, s_mbpInit, (jint)bx, (jint)by, (jint)bz);
    if (!mbp) return false;

    jobject state = env->CallObjectMethod(world, s_getState, mbp);
    env->DeleteLocalRef(mbp);
    if (!state || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jboolean isAir = env->CallBooleanMethod(state, s_bs_isAir);
    env->DeleteLocalRef(state);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return !isAir;
}

// ═══════════════════════════════════════════════════════════════════════
//  placeWater — GameMode.useItemOn(player, MAIN_HAND, BlockHitResult)
//               Places water/hay on the ground block at (bx,by,bz)
// ═══════════════════════════════════════════════════════════════════════

bool AutoMLG::placeWater(JNIEnv* env, jobject player, jobject mc,
                          int bx, int by, int bz) {
    if (!s_useItemOn || !s_bhrInit || !s_vec3Init || !s_bpInit || !s_dirUp || !s_mainHand)
        return false;

    jobject gm = env->GetObjectField(mc, s_gmFld);
    if (!gm || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jobject dirUp = env->GetStaticObjectField(s_dirCls, s_dirUp);
    if (!dirUp) { env->DeleteLocalRef(gm); return false; }

    jobject hand = env->GetStaticObjectField(s_handCls, s_mainHand);
    if (!hand) { env->DeleteLocalRef(dirUp); env->DeleteLocalRef(gm); return false; }

    // BlockPos target: the ground block
    jobject bp = env->NewObject(s_bpCls, s_bpInit, (jint)bx, (jint)by, (jint)bz);
    if (!bp) {
        env->DeleteLocalRef(hand); env->DeleteLocalRef(dirUp); env->DeleteLocalRef(gm);
        return false;
    }

    // Hit location: center of the ground block's top face
    jobject loc = env->NewObject(s_vec3Cls, s_vec3Init,
        (jdouble)bx + 0.5, (jdouble)(by + 1.0), (jdouble)bz + 0.5);
    if (!loc) {
        env->DeleteLocalRef(bp);
        env->DeleteLocalRef(hand); env->DeleteLocalRef(dirUp); env->DeleteLocalRef(gm);
        return false;
    }

    // BlockHitResult(location, Direction.UP, blockPos, false)
    jobject hit = env->NewObject(s_bhrCls, s_bhrInit, loc, dirUp, bp, JNI_FALSE);
    if (!hit) {
        env->DeleteLocalRef(loc); env->DeleteLocalRef(bp);
        env->DeleteLocalRef(hand); env->DeleteLocalRef(dirUp); env->DeleteLocalRef(gm);
        return false;
    }

    env->CallObjectMethod(gm, s_useItemOn, player, hand, hit);
    if (env->ExceptionCheck()) env->ExceptionClear();

    env->DeleteLocalRef(hit);
    env->DeleteLocalRef(loc);
    env->DeleteLocalRef(bp);
    env->DeleteLocalRef(hand);
    env->DeleteLocalRef(dirUp);
    env->DeleteLocalRef(gm);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
//  onUpdate — fall detection → MLG item → aim down → place
// ═══════════════════════════════════════════════════════════════════════

void AutoMLG::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    if (!initJNI(env)) return;

    // ── Get Minecraft instance ──────────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    // Get player + world fields (cached)
    static jfieldID s_playerFld = nullptr;
    static jfieldID s_levelFld  = nullptr;
    if (!s_playerFld) {
        jclass mcC = env->GetObjectClass(mc);
        s_playerFld = env->GetFieldID(mcC, Mappings::MC_player, Mappings::MC_player_Sig);
        s_levelFld  = env->GetFieldID(mcC, Mappings::MC_level,  Mappings::MC_level_Sig);
        env->DeleteLocalRef(mcC);
    }

    jobject playerObj = env->GetObjectField(mc, s_playerFld);
    if (!playerObj || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (playerObj) env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
        return;
    }

    CEntity player(playerObj);

    // ═══════════════════════════════════════════════════════════════════
    //  Fall detection
    // ═══════════════════════════════════════════════════════════════════

    bool onGround = player.isOnGround();

    if (onGround) {
        // Just landed — reset state + restore slot
        if (m_mlgActive && m_boolSettings["auto_swap"] && m_prevSlot >= 0 && m_mlgSlot >= 0) {
            jobject inv = env->GetObjectField(playerObj, s_invFld);
            if (inv) {
                env->CallVoidMethod(inv, s_setSlot, (jint)m_prevSlot);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(inv);
            }
        }
        m_mlgActive = false;
        m_mlgSlot   = -1;
        m_prevSlot  = -1;
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
        return;
    }

    // Get downward velocity
    auto delta = player.getDeltaMovement();
    double fallSpeed = -delta.y;  // positive = falling down

    if (fallSpeed < 0.2) {
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Find ground distance by scanning down
    // ═══════════════════════════════════════════════════════════════════

    jobject worldObj = env->GetObjectField(mc, s_levelFld);
    if (!worldObj || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (worldObj) env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
        return;
    }

    double px = player.getX();
    double py = player.getY();
    double pz = player.getZ();

    int floorX = (int)std::floor(px);
    int floorY = (int)std::floor(py);
    int floorZ = (int)std::floor(pz);

    // Scan down to find first solid block
    int groundY = floorY - 1;
    int scanMin = floorY - (int)m_floatSettings["max_fall"] - 2;
    if (scanMin < -64) scanMin = -64;

    while (groundY >= scanMin) {
        if (isBlockSolid(env, worldObj, floorX, groundY, floorZ))
            break;
        groundY--;
    }

    // Distance from player feet to top of ground block
    double distToGround = py - (groundY + 1.0);
    if (distToGround < 0) distToGround = 0;

    // ═══════════════════════════════════════════════════════════════════
    //  MLG trigger decision
    // ═══════════════════════════════════════════════════════════════════

    float minFall = m_floatSettings["min_fall"];
    float maxFall = m_floatSettings["max_fall"];

    bool shouldMLG = (distToGround >= (double)minFall
                   && distToGround <= (double)maxFall
                   && fallSpeed > 0.3);

    if (!shouldMLG) {
        if (!m_mlgActive) {
            env->DeleteLocalRef(worldObj);
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(mc);
            return;
        }
        // We were active but conditions changed — abort
        m_mlgActive = false;
        m_mlgSlot   = -1;
        env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(playerObj);
        env->DeleteLocalRef(mc);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Find MLG item on first trigger
    // ═══════════════════════════════════════════════════════════════════

    if (!m_mlgActive) {
        jobject inv = env->GetObjectField(playerObj, s_invFld);
        if (!inv) {
            env->DeleteLocalRef(worldObj);
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(mc);
            return;
        }

        m_mlgSlot = findMLGItem(env, inv);
        if (m_mlgSlot < 0) {
            env->DeleteLocalRef(inv);
            env->DeleteLocalRef(worldObj);
            env->DeleteLocalRef(playerObj);
            env->DeleteLocalRef(mc);
            return;
        }

        m_prevSlot = (int)env->CallIntMethod(inv, s_getSlot);
        if (env->ExceptionCheck()) { env->ExceptionClear(); m_prevSlot = -1; }

        m_mlgActive = true;
        env->DeleteLocalRef(inv);
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Switch to MLG item (if not already selected)
    // ═══════════════════════════════════════════════════════════════════

    if (m_mlgSlot >= 0) {
        jobject inv = env->GetObjectField(playerObj, s_invFld);
        if (inv) {
            jint cur = env->CallIntMethod(inv, s_getSlot);
            if (cur != (jint)m_mlgSlot) {
                env->CallVoidMethod(inv, s_setSlot, (jint)m_mlgSlot);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(inv);
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  Aim straight down + place
    // ═══════════════════════════════════════════════════════════════════

    float oldPitch = player.getPitch();
    player.setPitch(90.0f);

    bool placed = placeWater(env, playerObj, mc, floorX, groundY, floorZ);

    player.setPitch(oldPitch);

    // ═══════════════════════════════════════════════════════════════════
    //  Restore original slot
    // ═══════════════════════════════════════════════════════════════════

    if (placed && m_boolSettings["auto_swap"] && m_prevSlot >= 0 && m_mlgSlot >= 0) {
        jobject inv = env->GetObjectField(playerObj, s_invFld);
        if (inv) {
            env->CallVoidMethod(inv, s_setSlot, (jint)m_prevSlot);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(inv);
        }
    }

    // Reset state
    m_mlgActive = false;
    m_mlgSlot   = -1;
    m_prevSlot  = -1;

    env->DeleteLocalRef(worldObj);
    env->DeleteLocalRef(playerObj);
    env->DeleteLocalRef(mc);
}
