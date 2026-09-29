#pragma once
#include "modules/module.h"
#include "core/watchdog_bypass.h"
#include "core/ac_bypass.h"
#include "core/ac_humanizer.h"
#include <chrono>
#include <random>

/*
 * KillAura v3 — Multi-AC bypass auto-attack.
 *
 * Bypasses:
 *   Watchdog — Point spreading + burst breathing + CPS decay
 *   GrimAC   — Body-part randomization + accel-based rotation + delta clamp
 *   Vulcan   — Contextual miss rate + FOV falloff
 *   Matrix   — Tick spreading (max actions per window)
 *   Spartan  — All values stay within vanilla bounds
 */
class KillAura : public Module {
public:
    KillAura();
    void onEnable() override;
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;

private:
    enum class Mode : int { Legit = 0, Rage = 1 };

    float wrapAngle(float a);
    float angleDiff(float a, float b);
    // Zufallshelfer (menschliche Schwankungen)
    int   randInt(int lo, int hi);
    float randFloat(float lo, float hi);

    void  initJNI(JNIEnv* env);
    void  jniAttack(JNIEnv* env, jobject player, jobject target); // Attack + Swing
    void  swingOnly(JNIEnv* env, jobject player);                // nur visueller Swing (Miss)
    float attackScale(JNIEnv* env, jobject player); // 0..1, 1.0 = Cooldown fertig

    long long m_lastAttack = 0;   // letzter Zeitpunkt eines Angriffs (ms epoch)
    long long m_nextAttackAt = 0; // frühester erlaubter nächster Angriff (Reaktion/Timing)
    long long m_firstRageAttack = 0; // Startzeit für CPS-Decay
    int       m_lastTargetId = -1;
    float m_curYaw = 0, m_curPitch = 0;
    bool  m_hasTarget = false;

    std::mt19937 m_rng{ std::random_device{}() };

    // Watchdog bypass utilities
    wd::PacketScheduler m_pktScheduler;
    wd::SineJitter      m_sineJitter;
    std::chrono::steady_clock::time_point m_lastSineTick{};

    // Multi-AC bypass utilities
    ac::BodyPartTarget       m_bodyPart;
    ac::AccelRotation        m_accelRot;
    ac::ContextualMiss       m_ctxMiss;
    ac::TickSpreader         m_tickSpread;
    ac::PerlinJitter         m_perlinJitter;
    ac::GlobalPacketScheduler m_globalScheduler;
    ac::CPSHumanizer m_cpsHuman;
    ac::ReachWobbler m_reachWobbler;
    bool m_justSwitched = false;
    double m_lastTargetSpeed = 0.0;
    bool m_usePerlin = true;  // toggle: Sine vs Perlin jitter

    // ── Human aim state (Legit mode only) ───────────────────────
    int    m_aimFrames       = 0;     // frames since we locked onto this target
    int    m_overshootTimer  = 0;     // countdown to next overshoot
    int    m_overshootFrames = 0;     // frames in current overshoot
    float  m_overshootYaw    = 0.0f;  // overshoot offset
    float  m_overshootPitch  = 0.0f;
    float  m_aimInertiaYaw   = 0.0f;  // momentum from previous aim direction
    float  m_aimInertiaPitch = 0.0f;
    float  m_curStepSpeed    = 0.0f;  // current interpolation speed (varies)

    // Gecachte JNI-IDs (init einmalig)
    static inline bool      s_jniInit = false;
    static inline jclass    s_playerClass   = nullptr;
    static inline jmethodID s_playerAttack  = nullptr; // Legacy: Player.attack (leer in 1.21.x)
    static inline jmethodID s_playerSwing   = nullptr;
    static inline jclass    s_handClass     = nullptr;
    static inline jfieldID  s_mainHand      = nullptr;
    static inline jmethodID s_scaleMethod   = nullptr; // getAttackStrengthScale(F)F
    static inline jclass    s_livingClass   = nullptr;
    static inline jfieldID  s_tickerField   = nullptr; // attackStrengthTicker I (Fallback)

    // Echtes Attack-Packet: MultiPlayerGameMode.attack(Player, Entity)
    static inline jfieldID  s_gameModeField = nullptr; // Minecraft.gameMode
    static inline jclass    s_gameModeClass = nullptr;
    static inline jmethodID s_gameModeAttack = nullptr; // (Player, Entity) oder Legacy (Entity)
    static inline bool      s_gameMode2Args  = true;    // welche Signatur aufgelöst wurde
};
