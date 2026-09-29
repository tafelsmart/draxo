#pragma once
#include "modules/module.h"

class Reach : public Module {
public:
    Reach();
    void onUpdate(JNIEnv* env) override;
    void onDisable() override;
    void onEnable() override;

private:
    void setReach(JNIEnv* env, double entityReach, double blockReach);
    void sendPositionPacket(JNIEnv* env, jobject connection, double x, double y, double z, float yaw, float pitch, bool onGround);

    // ── Humanization state ───────────────────────────────────────
    int     m_reachFrames         = 0;   // ticks since reach enabled
    int     m_overshootTimer      = 0;   // ticks until next overshoot
    int     m_overshootFrames     = 0;   // remaining overshoot frames
    float   m_reachOvershootAmt   = 0;   // current overshoot amount
    float   m_curReachLevel       = 0;   // smoothed current reach multiplier (0→1)
    float   m_prevReachDelta      = 0;   // inertia carry-over
    int64_t m_lastReachSetMs      = 0;   // ms timestamp of last setReach call

    // Cached JNI IDs for packet sending
    static inline bool s_netInit = false;

    // Attribute system
    static inline jclass    s_attributesClass = nullptr;
    static inline jfieldID  s_entityInteractionRange = nullptr;
    static inline jfieldID  s_blockInteractionRange = nullptr;
    static inline jclass    s_livingEntityClass = nullptr;
    static inline jmethodID s_getAttribute = nullptr;
    static inline jclass    s_attributeInstanceClass = nullptr;
    static inline jmethodID s_setBaseValue = nullptr;

    // Network packet system
    static inline jclass    s_mcClass = nullptr;
    static inline jmethodID s_getConnection = nullptr;
    static inline jclass    s_ccpliClass = nullptr;
    static inline jfieldID  s_connectionField = nullptr;
    static inline jclass    s_connectionClass = nullptr;
    static inline jmethodID s_connectionSend = nullptr;
    static inline jclass    s_movePacketPosClass = nullptr;
    static inline jmethodID s_movePacketPosInit = nullptr;

    // Hit result detection
    static inline jfieldID  s_hitResultField = nullptr;
    static inline jclass    s_entityHitResultClass = nullptr;
    static inline jmethodID s_getTargetEntity = nullptr;

    // Player fields
    static inline jclass    s_playerClass = nullptr;
    static inline jmethodID s_playerAttack = nullptr;
    static inline jmethodID s_playerSwing = nullptr;
    static inline jclass    s_interactionHandClass = nullptr;
    static inline jfieldID  s_mainHandField = nullptr;
};
