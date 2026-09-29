#pragma once
#include "modules/module.h"

class ESP : public Module {
public:
    ESP();
    void onRender() override;

private:
    // ── ChestESP JNI cache ──────────────────────────────────────────
    static inline bool     s_chestJni = false;
    static inline jclass   s_lvlCls   = nullptr;
    static inline jmethodID s_getState = nullptr;
    static inline jclass   s_bsCls    = nullptr;
    static inline jmethodID s_bs_getBlock = nullptr;
    static inline jclass   s_blockCls = nullptr;
    static inline jmethodID s_block_descId = nullptr;
    static inline jclass   s_mbpCls   = nullptr;
    static inline jmethodID s_mbpInit  = nullptr;

    bool initChestJNI(JNIEnv* env);
    int  classifyContainer(const char* descId);

    // ── ChestESP scan cache (throttled) ─────────────────────────────
    // Der Block-Scan macht pro Frame ~100k+ JNI-Calls und verursacht die
    // extremen Lags. Stattdessen: nur alle ~600ms neu scannen, Treffer
    // cachen und jede Frame billig aus dem Cache zeichnen.
    struct ChestHit { int type; int x, y, z; };
    std::vector<ChestHit> m_chestCache;
    unsigned long long m_chestLastScan = 0;
    int m_chestPx = 0x7fffffff, m_chestPy = 0x7fffffff, m_chestPz = 0x7fffffff;
};
