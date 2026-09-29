#include "pch.h"
#include "core/strcrypt.h"
#include "modules/bedaura.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include "core/ac_bypass.h"

// ═══════════════════════════════════════════════════════════════════════
//  BedAura — Automatischer Bett-Brecher für Bedwars
//
//  Scannt Blöcke im Radius, prüft per Block.getDescriptionId() ob ein
//  Bett-Block vorliegt ("block.minecraft.*_bed"), wählt das beste Tool
//  (Axt > Spitzhacke) und bricht das Bett mit human-ähnlichem Timing.
// ═══════════════════════════════════════════════════════════════════════

BedAura::BedAura() : Module("BedAura", ModuleCategory::COMBAT, 0,
    "Automatically finds and breaks beds in Bedwars. "
    "Auto-selects best tool. Legit mode uses human-like timing.") {
    setTickInterval(1);

    defineMode("mode",    "Mode",       0, {"Legit", "Rage"});
    defineFloat("range",  "Range",      4.5f, 1.0f, 7.0f, "%.1f");
    defineFloat("fov",    "FOV",       60.0f, 30.0f, 180.0f, "%.0f");
    defineFloat("delay_min","Delay Min (ms)", 80.0f, 20.0f, 500.0f, "%.0f");
    defineFloat("delay_max","Delay Max (ms)",140.0f, 40.0f, 600.0f, "%.0f");
    defineBool("auto_tool","Auto Tool Switch", true);
    defineBool("notify",   "Notification",     true);

    addSearchTag("bed");
    addSearchTag("bedwars");
    addSearchTag("break");
    setFavorite(true);
}

void BedAura::onEnable() {
    m_breakTicks  = 0;
    m_bedFound    = false;
    m_targetX = m_targetY = m_targetZ = 0;
    m_tickCounter = 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  JNI-Init — cached class/method/field IDs (one-shot, exists) ───────
// ═══════════════════════════════════════════════════════════════════════

bool BedAura::initJNI(JNIEnv* env) {
    if (s_mcClass && s_playerFld && s_levelFld && s_gmFld) return true;

    // Minecraft instance fields
    if (!s_mcClass) s_mcClass = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (s_mcClass) {
        if (!s_playerFld) s_playerFld = env->GetFieldID(s_mcClass, Mappings::MC_player, Mappings::MC_player_Sig);
        if (!s_levelFld)  s_levelFld  = env->GetFieldID(s_mcClass, Mappings::MC_level,  Mappings::MC_level_Sig);
        if (!s_gmFld)     s_gmFld     = env->GetFieldID(s_mcClass, Mappings::MC_gameMode, Mappings::MC_gameMode_Sig);
    }

    // GameMode methods
    if (!s_gmCls) s_gmCls = JvmWrapper::findClass(Mappings::GameMode_Class);
    if (s_gmCls) {
        if (!s_gm_start)    s_gm_start    = env->GetMethodID(s_gmCls, Mappings::GameMode_startDestroyBlock,    Mappings::GameMode_startDestroyBlock_Sig);
        if (!s_gm_continue) s_gm_continue = env->GetMethodID(s_gmCls, Mappings::GameMode_continueDestroyBlock, Mappings::GameMode_continueDestroyBlock_Sig);
        if (!s_gm_stop)     s_gm_stop     = env->GetMethodID(s_gmCls, Mappings::GameMode_stopDestroyBlock,     Mappings::GameMode_stopDestroyBlock_Sig);
    }

    // Level.getBlockState()
    if (!s_levelCls) s_levelCls = JvmWrapper::findClass(Mappings::Level_Class);
    if (s_levelCls && !s_lvl_getState)
        s_lvl_getState = env->GetMethodID(s_levelCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);

    // BlockState.isAir() + getBlock()
    if (!s_bsCls) s_bsCls = JvmWrapper::findClass(Mappings::BlockState_Class);
    if (s_bsCls) {
        if (!s_bs_isAir)    s_bs_isAir    = env->GetMethodID(s_bsCls, Mappings::BlockStateBase_isAir,  Mappings::BlockStateBase_isAir_Sig);
        if (!s_bs_getBlock) s_bs_getBlock = env->GetMethodID(s_bsCls, Mappings::BlockState_getBlock,   Mappings::BlockState_getBlock_Sig);
    }

    // Block.getDescriptionId()
    if (!s_blockCls) s_blockCls = JvmWrapper::findClass(Mappings::Block_Class);
    if (s_blockCls && !s_block_getDescId)
        s_block_getDescId = env->GetMethodID(s_blockCls, Mappings::Block_getDescriptionId, Mappings::Block_getDescriptionId_Sig);

    // BlockPos("III")V
    if (!s_bpCls) s_bpCls = JvmWrapper::findClass(Mappings::BlockPos_Class);
    if (s_bpCls && !s_bpInit)
        s_bpInit = env->GetMethodID(s_bpCls, Mappings::BlockPos_Init, Mappings::BlockPos_Init_Sig);

    // MutableBlockPos
    if (!s_mbpCls) s_mbpCls = JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
    if (s_mbpCls && !s_mbpInit)
        s_mbpInit = env->GetMethodID(s_mbpCls, Mappings::MutableBlockPos_Init, Mappings::MutableBlockPos_Init_Sig);

    // Direction.UP (static field)
    if (!s_dirCls) s_dirCls = JvmWrapper::findClass(Mappings::Direction_Class);
    if (s_dirCls && !s_dirUp)
        s_dirUp = env->GetStaticFieldID(s_dirCls, Mappings::Direction_UP, Mappings::Direction_UP_Sig);

    // Player.inventory
    if (!s_playerCls) s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
    if (s_playerCls && !s_pl_getInv)
        s_pl_getInv = env->GetMethodID(s_playerCls, Mappings::Player_inventory, Mappings::Player_inventory_Sig);

    // Inventory
    if (!s_invCls) s_invCls = JvmWrapper::findClass(Mappings::Inventory_Class);
    if (s_invCls) {
        if (!s_inv_getSlot) s_inv_getSlot = env->GetMethodID(s_invCls, Mappings::Inventory_getSelectedSlot, Mappings::Inventory_getSelectedSlot_Sig);
        if (!s_inv_setSlot) s_inv_setSlot = env->GetMethodID(s_invCls, Mappings::Inventory_setSelectedSlot, Mappings::Inventory_setSelectedSlot_Sig);
        if (!s_inv_getItem) s_inv_getItem = env->GetMethodID(s_invCls, Mappings::Inventory_getItem,         Mappings::Inventory_getItem_Sig);
    }

    // AxeItem (axe detection for tool priority)
    if (!s_axeCls) {
        s_axeCls = JvmWrapper::findClass("net/minecraft/world/item/AxeItem");
        if (!s_axeCls) s_axeCls = JvmWrapper::findClass("net/minecraft/item/AxeItem");
    }

    return s_mcClass && s_playerFld && s_levelFld && s_gmFld && s_gmCls
        && s_gm_start && s_bpCls && s_bpInit && s_dirUp && s_mbpInit;
}

// ═══════════════════════════════════════════════════════════════════════
//  isBedBlock  —  Block.getDescriptionId() enthält "_bed"
// ═══════════════════════════════════════════════════════════════════════

bool BedAura::isBedBlock(JNIEnv* env, jobject blockState) {
    if (!blockState || !s_bs_getBlock || !s_block_getDescId) return false;

    jobject block = env->CallObjectMethod(blockState, s_bs_getBlock);
    if (!block || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    jstring descId = (jstring)env->CallObjectMethod(block, s_block_getDescId);
    if (!descId || env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(block); return false; }

    const char* cid = env->GetStringUTFChars(descId, nullptr);
    if (!cid) { env->DeleteLocalRef(descId); env->DeleteLocalRef(block); return false; }

    bool isBed = (strstr(cid, "bed") != nullptr
               && (strstr(cid, "block.minecraft.") != nullptr
                || strstr(cid, "tile.bed") != nullptr
                || strstr(cid, "Bed") != nullptr));
    env->ReleaseStringUTFChars(descId, cid);
    env->DeleteLocalRef(descId);
    env->DeleteLocalRef(block);
    return isBed;
}

// ═══════════════════════════════════════════════════════════════════════
//  findBestTool  —  hotbar scan: Axt > Spitzhacke > sonstiges
// ═══════════════════════════════════════════════════════════════════════

int BedAura::findBestTool(JNIEnv* env, jobject player) {
    if (!player || !s_pl_getInv || !s_inv_getSlot || !s_inv_setSlot || !s_inv_getItem) return -1;

    jobject inv = env->CallObjectMethod(player, s_pl_getInv);
    if (!inv || env->ExceptionCheck()) { env->ExceptionClear(); return -1; }

    int bestSlot = -1;
    float bestScore = 0.0f;

    for (int slot = 0; slot < 9; slot++) {
        jobject item = env->CallObjectMethod(inv, s_inv_getItem, (jint)slot);
        if (!item) continue;
        if (env->ExceptionCheck()) { env->ExceptionClear(); break; }

        float score = 0.0f;
        if (s_axeCls && env->IsInstanceOf(item, s_axeCls))
            score = 3.0f;  // Axe — beste Wahl für Betten
        else
            score = 1.0f;  // Jedes andere Tool/Werkzeug

        if (score > bestScore) { bestScore = score; bestSlot = slot; }
        env->DeleteLocalRef(item);
    }

    // Switch hotbar slot if needed
    if (bestSlot >= 0) {
        jint cur = env->CallIntMethod(inv, s_inv_getSlot);
        if (cur != (jint)bestSlot) {
            env->CallVoidMethod(inv, s_inv_setSlot, (jint)bestSlot);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    env->DeleteLocalRef(inv);
    return bestSlot;
}

// ═══════════════════════════════════════════════════════════════════════
//  isValidBedPos — MutableBlockPos → getBlockState → isBedBlock
// ═══════════════════════════════════════════════════════════════════════

bool BedAura::isValidBedPos(JNIEnv* env, jobject world, int bx, int by, int bz) {
    if (!world || !s_mbpInit || !s_lvl_getState) return false;

    jobject mbp = env->NewObject(s_mbpCls, s_mbpInit, (jint)bx, (jint)by, (jint)bz);
    if (!mbp) return false;

    jobject state = env->CallObjectMethod(world, s_lvl_getState, mbp);
    env->DeleteLocalRef(mbp);
    if (!state || env->ExceptionCheck()) { env->ExceptionClear(); return false; }

    // Skip air blocks entirely — faster than calling isBedBlock on air
    if (s_bs_isAir) {
        jboolean isAir = env->CallBooleanMethod(state, s_bs_isAir);
        if (isAir) { env->DeleteLocalRef(state); return false; }
    }

    bool result = isBedBlock(env, state);
    env->DeleteLocalRef(state);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  swingArm — visuelles Feedback (Arm-Schwung-Animation)
// ═══════════════════════════════════════════════════════════════════════

void BedAura::swingArm(JNIEnv* env, jobject player) {
    if (!player) return;

    // One-shot: get swing(MutableBlockPos) method
    static jmethodID s_swingM = nullptr;
    static jclass    s_playerCls = nullptr;
    if (!s_swingM) {
        s_playerCls = JvmWrapper::findClass(Mappings::Player_Class);
        if (s_playerCls) {
            s_swingM = env->GetMethodID(s_playerCls,
                Mappings::Player_swing, Mappings::Player_swing_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_swingM = nullptr; }
        }
    }

    if (!s_swingM) return;

    // Get InteractionHand.MAIN_HAND static field
    static jclass    s_handCls = nullptr;
    static jfieldID  s_mainHand = nullptr;
    if (!s_mainHand) {
        s_handCls = JvmWrapper::findClass(Mappings::InteractionHand_Class);
        if (s_handCls) {
            s_mainHand = env->GetStaticFieldID(s_handCls,
                Mappings::InteractionHand_MAIN_HAND, Mappings::InteractionHand_MAIN_HAND_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_mainHand = nullptr; }
        }
    }

    if (!s_mainHand) { env->CallVoidMethod(player, s_swingM); }  // fallback no-arg
    else {
        jobject hand = env->GetStaticObjectField(s_handCls, s_mainHand);
        if (hand) {
            env->CallVoidMethod(player, s_swingM, hand);
            env->DeleteLocalRef(hand);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// ═══════════════════════════════════════════════════════════════════════
//  onUpdate — main tick: scan → detect bed → break
// ═══════════════════════════════════════════════════════════════════════

void BedAura::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    m_frameCounter++;

    bool rage = (m_intSettings["mode"] == 1);

    // ── One-time JNI init ───────────────────────────────────────────
    if (!initJNI(env)) return;

    // ── Get Minecraft instance and extract player/world/gameMode ────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jobject player = env->GetObjectField(mc, s_playerFld);
    jobject world  = env->GetObjectField(mc, s_levelFld);
    jobject gm     = env->GetObjectField(mc, s_gmFld);
    env->DeleteLocalRef(mc);

    if (!player || !world || !gm || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (player) env->DeleteLocalRef(player);
        if (world)  env->DeleteLocalRef(world);
        if (gm)     env->DeleteLocalRef(gm);
        return;
    }

    // ── Player position ────────────────────────────────────────────
    CEntity playerEnt(player);
    double px = playerEnt.getX();
    double py = playerEnt.getY();
    double pz = playerEnt.getZ();
    double eyeY = py + 1.62;

    // Camera data for FOV check
    auto cam = CMinecraft::getCameraData();
    float camYaw   = cam.valid ? cam.yaw   : 0.0f;
    float camPitch = cam.valid ? cam.pitch : 0.0f;

    // ── Scan for beds ───────────────────────────────────────────────
    float range  = m_floatSettings["range"];
    int   iRange = (int)std::ceil(range);
    float fov    = m_floatSettings["fov"];

    float bestDist = range + 1.0f;
    int   bestX = 0, bestY = 0, bestZ = 0;
    bool  foundBed = false;

    for (int dx = -iRange; dx <= iRange; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            for (int dz = -iRange; dz <= iRange; dz++) {
                int bx = (int)std::floor(px) + dx;
                int by = (int)std::floor(py) + dy;
                int bz = (int)std::floor(pz) + dz;

                float dxf = (bx + 0.5f) - (float)px;
                float dyf = (by + 0.5f) - (float)eyeY;
                float dzf = (bz + 0.5f) - (float)pz;
                float dist = std::sqrt(dxf * dxf + dyf * dyf + dzf * dzf);
                if (dist > range) continue;

                // FOV check
                float yawTo   = std::atan2(dzf, dxf) * 180.0f / 3.14159f;
                float yawDiff = std::abs(camYaw - yawTo);
                if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;
                if (yawDiff > fov / 2.0f) continue;

                // Block check
                if (!isValidBedPos(env, world, bx, by, bz)) continue;
                // Check the block above too (beds are 2 blocks tall)
                if (!isValidBedPos(env, world, bx, by + 1, bz)) continue;

                if (dist < bestDist) {
                    bestDist = dist;
                    bestX = bx; bestY = by; bestZ = bz;
                    foundBed = true;
                }
            }
        }
    }

    // ── Target changed → stop current break ─────────────────────────
    bool sameTarget = (bestX == m_targetX && bestY == m_targetY && bestZ == m_targetZ);
    if (!foundBed || !sameTarget) {
        if (m_breakTicks > 0 && s_gm_stop) {
            env->CallVoidMethod(gm, s_gm_stop);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        m_breakTicks = 0;
    }

    if (!foundBed) {
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(gm);
        return;
    }

    // ── New target ──────────────────────────────────────────────────
    if (!sameTarget) {
        m_targetX = bestX; m_targetY = bestY; m_targetZ = bestZ;
        m_breakTicks = 0;
    }

    m_bedFound = true;

    // ── Auto-tool ───────────────────────────────────────────────────
    if (m_boolSettings["auto_tool"]) {
        findBestTool(env, player);
    }

    // ── Tick-based delay ────────────────────────────────────────────
    m_tickCounter++;
    float dMin = m_floatSettings["delay_min"] / 50.0f;  // ms → ticks (20 tps)
    float dMax = m_floatSettings["delay_max"] / 50.0f;
    if (rage) { dMin *= 0.5f; dMax *= 0.5f; }

    int tickDelay = (int)dMin;
    if ((int)dMax > (int)dMin)
        tickDelay += rand() % ((int)dMax - (int)dMin + 1);
    if (tickDelay < 1) tickDelay = 1;

    // ── Start / continue breaking ───────────────────────────────────
    if (m_breakTicks == 0) {
        jobject bp = env->NewObject(s_bpCls, s_bpInit, (jint)m_targetX, (jint)m_targetY, (jint)m_targetZ);
        if (!bp) { goto cleanup; }

        jobject dirUp = env->GetStaticObjectField(s_dirCls, s_dirUp);
        if (!dirUp) { env->DeleteLocalRef(bp); goto cleanup; }

        env->CallBooleanMethod(gm, s_gm_start, bp, dirUp);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else {
            swingArm(env, player);
            m_breakTicks = 1;
        }

        env->DeleteLocalRef(dirUp);
        env->DeleteLocalRef(bp);
    } else {
        // Continue break every tick to maintain block-breaking progress
        m_breakTicks++;
        if (s_gm_continue) {
            jobject bp = env->NewObject(s_bpCls, s_bpInit, (jint)m_targetX, (jint)m_targetY, (jint)m_targetZ);
            if (bp) {
                jobject dirUp = env->GetStaticObjectField(s_dirCls, s_dirUp);
                if (dirUp) {
                    env->CallBooleanMethod(gm, s_gm_continue, bp, dirUp);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(dirUp);
                }
                env->DeleteLocalRef(bp);
            }
        }
    }

cleanup:
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(gm);
}
