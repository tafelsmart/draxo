#pragma once
#include "pch.h"
#include "modules/module.h"
#include "sdk/entity.h" // for CEntity::Vec3

class BedAura : public Module {
public:
    BedAura();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    // ── Cached JNI IDs ──────────────────────────────────────────────
    static inline jclass    s_mcClass   = nullptr;
    static inline jfieldID  s_playerFld = nullptr;
    static inline jfieldID  s_levelFld  = nullptr;
    static inline jfieldID  s_gmFld     = nullptr;

    static inline jclass    s_gmCls     = nullptr;
    static inline jmethodID s_gm_start  = nullptr;
    static inline jmethodID s_gm_continue = nullptr;
    static inline jmethodID s_gm_stop   = nullptr;

    static inline jclass    s_levelCls  = nullptr;
    static inline jmethodID s_lvl_getState = nullptr;

    static inline jclass    s_bsCls     = nullptr;
    static inline jmethodID s_bs_isAir  = nullptr;  // BlockStateBase.isAir()
    static inline jmethodID s_bs_getBlock = nullptr;

    static inline jclass    s_blockCls  = nullptr;
    static inline jmethodID s_block_getDescId = nullptr;

    static inline jclass    s_bpCls     = nullptr;
    static inline jmethodID s_bpInit    = nullptr;

    static inline jclass    s_mbpCls    = nullptr;
    static inline jmethodID s_mbpInit   = nullptr;

    static inline jclass    s_dirCls    = nullptr;
    static inline jfieldID  s_dirUp     = nullptr;

    // Tool switching
    static inline jclass    s_playerCls = nullptr;
    static inline jmethodID s_pl_getInv = nullptr;
    static inline jclass    s_invCls    = nullptr;
    static inline jmethodID s_inv_getSlot = nullptr;
    static inline jmethodID s_inv_setSlot = nullptr;
    static inline jmethodID s_inv_getItem = nullptr;
    static inline jclass    s_axeCls    = nullptr;

    // ── State ───────────────────────────────────────────────────────
    int   m_breakTicks  = 0;
    int   m_targetX     = 0;
    int   m_targetY     = 0;
    int   m_targetZ     = 0;
    bool  m_bedFound    = false;
    int   m_frameCounter= 0;
    int   m_tickCounter = 0;

    // ── Helpers ─────────────────────────────────────────────────────
    bool isBedBlock(JNIEnv* env, jobject blockState);
    int  findBestTool(JNIEnv* env, jobject player);
    bool isValidBedPos(JNIEnv* env, jobject world, int bx, int by, int bz);
    void swingArm(JNIEnv* env, jobject player);
    bool initJNI(JNIEnv* env);
};
