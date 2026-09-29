#pragma once
#include "pch.h"
#include "modules/module.h"

class InventoryCleaner : public Module {
public:
    InventoryCleaner();
    void onEnable() override;
    void onUpdate(JNIEnv* env) override;

private:
    // ── Cached JNI ──────────────────────────────────────────────────
    static inline jclass    s_mcCls         = nullptr;
    static inline jfieldID  s_gmField       = nullptr;

    static inline jclass    s_playerCls     = nullptr;
    static inline jfieldID  s_invField      = nullptr;
    static inline jfieldID  s_menuField     = nullptr;

    static inline jclass    s_invCls        = nullptr;
    static inline jmethodID s_getItem       = nullptr;
    static inline jmethodID s_getSelected   = nullptr;
    static inline jmethodID s_setSelected   = nullptr;

    static inline jclass    s_stackCls      = nullptr;
    static inline jmethodID s_stackIsEmpty  = nullptr;
    static inline jmethodID s_stackGetHover = nullptr;

    static inline jclass    s_compCls       = nullptr;
    static inline jmethodID s_compGetStr    = nullptr;

    static inline jclass    s_gmCls         = nullptr;
    static inline jmethodID s_handleClick   = nullptr;
    static inline jclass    s_ctCls         = nullptr;
    static inline jfieldID  s_swapField     = nullptr;
    static inline jclass    s_acmCls        = nullptr;
    static inline jfieldID  s_ctrIdField    = nullptr;

    // ── State (multi-tick state machine) ────────────────────────────
    int   m_srcSlot      = -1;
    int   m_phase        = 0;    // 0=scan, 1=swap, 2=drop, 3=restore
    int   m_origSlot     = -1;
    int   m_dstHotbar    = 0;
    int   m_tickWait     = 0;
    static inline bool s_jni = false;

    // ── Helpers ─────────────────────────────────────────────────────
    bool initJNI(JNIEnv* env);
    bool isJunkItem(JNIEnv* env, jobject stack);
    bool swapToHotbar(JNIEnv* env, jobject mc, jobject player, int srcIdx, int dstHotbar);
    void doDrop(JNIEnv* env, jobject inv, int slot);
};
