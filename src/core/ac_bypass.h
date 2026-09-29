#pragma once
#include <cmath>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>

// AC_BYPASS -- Multi-Anticheat bypass engine.
// Targets: Watchdog (Hypixel), GrimAC, Vulcan, Matrix, Spartan, NCP

namespace ac {

inline std::mt19937& rng() {
    static std::random_device rd;
    static std::mt19937 r(rd());
    return r;
}
inline float randFloat(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng());
}
inline int randInt(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng());
}

// BodyPart -- Randomized aim point to avoid always targeting head/chest.
// GrimAC and Matrix detect "always aims at head height" as KillAura.
class BodyPartTarget {
public:
    enum Part { HEAD, CHEST, WAIST, LEGS, OFF_TARGET };
    void reset() {
        m_part = (Part)randInt(0, 4);
        m_stickFrames = randInt(8, 45);
        m_frameCount = 0;
    }
    float getYOffset() {
        m_frameCount++;
        if (m_frameCount >= m_stickFrames) {
            m_part = (Part)randInt(0, 4);
            m_stickFrames = randInt(8, 45);
            m_frameCount = 0;
        }
        switch (m_part) {
            case HEAD: return 1.55f + randFloat(-0.08f, 0.08f);
            case CHEST: return 1.25f + randFloat(-0.10f, 0.10f);
            case WAIST: return 0.85f + randFloat(-0.12f, 0.12f);
            case LEGS: return 0.40f + randFloat(-0.15f, 0.15f);
            case OFF_TARGET: return 0.60f + randFloat(-0.50f, 0.80f);
        }
        return 1.0f;
    }
    float getHorizontalJitter() {
        if (m_part == OFF_TARGET) return randFloat(-0.45f, 0.45f);
        return randFloat(-0.10f, 0.10f);
    }
private:
    Part m_part = CHEST;
    int m_stickFrames = 30;
    int m_frameCount = 0;
};

// AccelRotation -- Mouse-like acceleration/deceleration curve.
// GrimAC detects constant-speed rotation as non-human.
class AccelRotation {
public:
    void reset(float, float) {
        m_phase = 0; m_phaseTime = 0.0f;
        m_reactDuration = 0.03f + randFloat(0.0f, 0.06f);
        m_accelDuration = 0.08f + randFloat(0.0f, 0.15f);
    }
    float step(float dt) {
        m_phaseTime += dt;
        if (m_phase == 0 && m_phaseTime >= m_reactDuration) { m_phase = 1; m_phaseTime = 0.0f; }
        if (m_phase == 1 && m_phaseTime >= m_accelDuration) { m_phase = 2; m_phaseTime = 0.0f; }
        switch (m_phase) {
            case 0: return 0.1f + 0.3f * (m_phaseTime / std::max(m_reactDuration, 0.001f));
            case 1: return 0.4f + 0.6f * (m_phaseTime / std::max(m_accelDuration, 0.001f));
            case 2: return std::max(0.05f, 1.0f - m_phaseTime * 3.0f);
        }
        return 1.0f;
    }
private:
    int m_phase = 0;
    float m_phaseTime = 0.0f;
    float m_reactDuration = 0.05f;
    float m_accelDuration = 0.15f;
};

// ContextualMiss -- Miss based on target state, not just random chance.
class ContextualMiss {
public:
    bool shouldMiss(float speed, float dist, float fovPct, bool justSwitched) {
        float baseChance = 0.03f;
        if (speed > 0.5f) baseChance += (speed - 0.5f) * 0.02f;
        if (dist > 2.5f) baseChance += (dist - 2.5f) * 0.05f;
        if (fovPct > 0.6f) baseChance += (fovPct - 0.6f) * 0.50f;
        if (justSwitched) baseChance += 0.15f;
        if (baseChance > 0.60f) baseChance = 0.60f;
        return randFloat(0.0f, 1.0f) < baseChance;
    }
};

// ─────────────────────────────────────────────────────────────────────
// GlobalPacketScheduler — cross-module attack-rate governor.
//
// Different anticheats have different detection windows for action
// bursts. This scheduler tracks all suspicious actions (attacks,
// block placements, rotations) in a single sliding window and enforces
// per-AC rate limits. Modules share the same window so that combining
// KillAura + AutoClicker doesn't double the action rate.
//
// Profiles:
//   Watchdog:  max 12 actions / 2000ms  (point accumulation)
//   GrimAC:   max 8  actions / 1500ms  (physics-check burst)
//   Vulcan:   max 6  actions / 1000ms  (strict timing check)
//   Matrix:   max 5  actions / 800ms   (aggressive flagging)
//   Spartan:  max 4  actions / 600ms   (rule-based window)
// ─────────────────────────────────────────────────────────────────────
class GlobalPacketScheduler {
public:
    enum AC { WATCHDOG, GRIMAC, VULCAN, MATRIX, SPARTAN, COUNT };

    void reset() {
        m_actions.clear();
        m_actionCount = 0;
    }

    // canAct — check whether an action is allowed under ALL active AC profiles.
    // Returns true if the action can proceed, false if it should be skipped.
    bool canAct() {
        auto now = std::chrono::steady_clock::now();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        pruneOld(nowMs);
        return m_actionCount < m_globalLimit;
    }

    // recordAction — call AFTER performing the action
    void recordAction() {
        auto now = std::chrono::steady_clock::now();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        m_actions.push_back(nowMs);
        m_actionCount++;
        pruneOld(nowMs);
    }

    // setWindow — configure combined limits (set once during init)
    void setWindow(int maxActions, int windowMs) {
        m_globalLimit = maxActions;
        m_windowMs = windowMs;
    }

private:
    void pruneOld(long long nowMs) {
        m_actions.erase(
            std::remove_if(m_actions.begin(), m_actions.end(),
                [&](long long t) { return nowMs - t > m_windowMs; }),
            m_actions.end());
        m_actionCount = (int)m_actions.size();
    }

    std::vector<long long> m_actions;
    int m_actionCount = 0;
    int m_globalLimit = 8;
    int m_windowMs = 1500;
};

// PerlinJitter — 3-octave layered sine-wave jitter.
//
// Real human aim has layered tremor frequencies:
//   Octave 1 (main sway):      3-5  Hz, amplitude 0.5-2.0 deg
//   Octave 2 (micro-jitter):   7-12 Hz, amplitude 0.1-0.4 deg
//   Octave 3 (finger tremor): 15-22 Hz, amplitude 0.02-0.1 deg
//
// The sum of all three octaves creates a smooth, non-repeating,
// naturally irregular pattern that no anticheat can fingerprint
// as artificial (unlike pure random snaps or simple sine waves).
class PerlinJitter {
public:
    struct Offset { float yaw; float pitch; };

    void reset(float baseAmplitude = 1.0f) {
        m_time = 0.0f;
        m_baseAmp = baseAmplitude;
        // Randomize per-octave phases so no two resets produce identical patterns
        m_phase1  = randFloat(0.0f, 6.283f);
        m_phase2  = randFloat(0.0f, 6.283f);
        m_phase3  = randFloat(0.0f, 6.283f);
        m_freq1   = randFloat(2.5f, 6.0f);
        m_freq2   = randFloat(7.0f, 13.0f);
        m_freq3   = randFloat(15.0f, 23.0f);
        m_nextFreqChange = randFloat(1.0f, 4.0f);
        m_freqTimer = 0.0f;
    }

    Offset tick(float dt, float maxAmplitude, bool subtle = false) {
        m_time += dt;
        m_freqTimer += dt;

        // Periodically shift octave frequencies (every 1-4s)
        if (m_freqTimer >= m_nextFreqChange) {
            m_freqTimer = 0.0f;
            m_nextFreqChange = randFloat(1.0f, 4.0f);
            m_freq1 = randFloat(2.5f, 6.0f);
            m_freq2 = randFloat(7.0f, 13.0f);
            m_freq3 = randFloat(15.0f, 23.0f);
        }

        float ampMult = subtle ? 0.35f : 1.0f;
        float a1 = m_baseAmp * 0.65f * ampMult;
        float a2 = m_baseAmp * 0.25f * ampMult;
        float a3 = m_baseAmp * 0.10f * ampMult;

        // Clamp total amplitude
        float totalAmp = a1 + a2 + a3;
        if (totalAmp > maxAmplitude) {
            float scale = maxAmplitude / totalAmp;
            a1 *= scale; a2 *= scale; a3 *= scale;
        }

        // Layered sine waves with independent yaw/pitch phases
        float yawOff =
            sinf(m_time * m_freq1 * 6.283f + m_phase1)     * a1 +
            sinf(m_time * m_freq2 * 6.283f + m_phase2 + 1.7f) * a2 +
            sinf(m_time * m_freq3 * 6.283f + m_phase3 + 3.1f) * a3;

        float pitchOff =
            sinf(m_time * m_freq1 * 6.283f + m_phase1 + 0.8f) * a1 * 0.6f +
            sinf(m_time * m_freq2 * 6.283f + m_phase2 + 2.2f) * a2 * 0.6f +
            sinf(m_time * m_freq3 * 6.283f + m_phase3 + 4.5f) * a3 * 0.6f;

        return {yawOff, pitchOff};
    }

private:
    float m_time = 0.0f;
    float m_baseAmp = 1.0f;
    float m_phase1 = 0.0f, m_phase2 = 0.0f, m_phase3 = 0.0f;
    float m_freq1 = 4.0f, m_freq2 = 9.0f, m_freq3 = 18.0f;
    float m_freqTimer = 0.0f;
    float m_nextFreqChange = 2.0f;
};

// TickSpreader -- Limits suspicious actions per time window.
// Watchdog uses point accumulation: too many in a window = ban.
class TickSpreader {
public:
    void reset() { m_timestamps.clear(); }
    bool canAttack(int maxPerWindow, int windowMs) {
        auto now = std::chrono::steady_clock::now();
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        m_timestamps.erase(std::remove_if(m_timestamps.begin(), m_timestamps.end(),
            [&](long long t) { return nowMs - t > windowMs; }), m_timestamps.end());
        return (int)m_timestamps.size() < maxPerWindow;
    }
    void markAttack() {
        auto now = std::chrono::steady_clock::now();
        m_timestamps.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }
private:
    std::vector<long long> m_timestamps;
};

// ─────────────────────────────────────────────────────────────────────
// KBHumanizer — Unified knockback-reduction humanization.
//
// ACs detect constant KB reduction as bot-fingerprint:
//  1. Flat reduction (85% every tick) = non-human
//  2. Identical reduction per hit = no variance
//  3. Abrupt window-end (85→0 in one tick) = mechanical
//  4. Never overshoots = humans overcompensate sometimes
//
// Solution: Multi-phase Reaction Curve + Overshoot + Inertia, all in
//            one reusable class shared by AntiKB and Velocity.
// ─────────────────────────────────────────────────────────────────────
class KBHumanizer {
public:
    struct Reduction { float horizontal; float vertical; };

    void reset() {
        m_ticksLeft      = 0;
        m_kbFrame        = 0;
        m_windowTotal    = 0;
        m_curReductionH  = 0.0f;
        m_curReductionV  = 0.0f;
        m_baseH          = 0.0f;
        m_baseV          = 0.0f;
        m_randFactor     = 1.0f;
        m_overshooting   = false;
        m_overshootTick  = 0;
    }

    // startWindow — called when a KB hit is detected.
    //   windowTicks: number of ticks to apply reduction (e.g. 8 for AntiKB, 6 for Velocity)
    //   baseH/baseV: configured reduction fraction (0.0–1.0)
    //   randomize:   if true, adds ±10% per-hit variance
    void startWindow(int windowTicks, float baseH, float baseV, bool randomize) {
        m_ticksLeft   = windowTicks;
        m_windowTotal = windowTicks;
        m_kbFrame     = 0;
        m_baseH       = baseH;
        m_baseV       = baseV;
        m_randFactor  = randomize ? ac::randFloat(0.90f, 1.10f) : 1.0f;
        // don't reset curReduction — carry over from previous window for smooth transitions
        m_overshooting  = (ac::randFloat(0.0f, 1.0f) < 0.20f);
        m_overshootTick = m_overshooting ? randInt(2, std::max(2, windowTicks / 2)) : 999;
    }

    // tick — call once per game tick. Returns the current humanized reduction.
    Reduction tick() {
        if (m_ticksLeft > 0) {
            m_ticksLeft--;
            m_kbFrame++;

            int total = m_windowTotal;
            int frame = m_kbFrame;
            float curveMult;

            // ── Phase 0 (frame 0-1): REACTION — brain processes hit, minimal counter ──
            if (frame <= 1) {
                curveMult = ac::randFloat(0.20f, 0.40f);
            }
            // ── Phase 1 (frame 2→midpoint): RAMP-UP — accelerate to peak reduction ──
            else if (frame <= total / 2) {
                float t = (float)(frame - 1) / (float)(total / 2 - 1);
                float sCurve = t * t * (3.0f - 2.0f * t);   // smoothstep
                curveMult = 0.35f + sCurve * 0.55f;          // 0.35→0.90
                curveMult += ac::randFloat(-0.06f, 0.06f);
            }
            // ── Phase 2 (midpoint→end): DECAY — human naturally tapers off ──
            else {
                float t = (float)(frame - total / 2) / (float)(total - total / 2);
                curveMult = 0.90f - t * 0.40f;               // 0.90→0.50
                curveMult += ac::randFloat(-0.08f, 0.08f);   // higher jitter in decay
                if (m_ticksLeft <= 2) curveMult *= ac::randFloat(0.30f, 0.55f);
            }

            // ── Overshoot: over-compensate 15-30% for 1-2 ticks, then correct ──
            if (m_overshooting && frame == m_overshootTick) {
                curveMult = std::min(curveMult * ac::randFloat(1.15f, 1.30f), 0.98f);
            }
            if (m_overshooting && frame == m_overshootTick + 1) {
                curveMult *= ac::randFloat(0.75f, 0.90f);   // correction
            }

            // ── Inertia smoothing: no instant jumps, blend toward target ──
            float targetH = m_baseH * m_randFactor * curveMult;
            float targetV = m_baseV * m_randFactor * curveMult;
            float smoothWeight = ac::randFloat(0.30f, 0.55f);
            m_curReductionH += (targetH - m_curReductionH) * smoothWeight;
            m_curReductionV += (targetV - m_curReductionV) * smoothWeight;

            return { m_curReductionH, m_curReductionV };
        } else {
            // Window expired — let reduction naturally decay to zero
            m_kbFrame = 0;
            m_windowTotal = 0;
            if (m_curReductionH > 0.001f || m_curReductionV > 0.001f) {
                m_curReductionH *= ac::randFloat(0.70f, 0.85f);
                m_curReductionV *= ac::randFloat(0.70f, 0.85f);
                if (m_curReductionH < 0.005f) m_curReductionH = 0.0f;
                if (m_curReductionV < 0.005f) m_curReductionV = 0.0f;
            }
            return { m_curReductionH, m_curReductionV };
        }
    }

    // isActive — true while reduction is still being applied or decaying.
    bool isActive() const {
        return m_ticksLeft > 0 || m_curReductionH > 0.001f || m_curReductionV > 0.001f;
    }

private:
    int   m_ticksLeft      = 0;
    int   m_kbFrame        = 0;
    int   m_windowTotal    = 0;
    float m_curReductionH  = 0.0f;
    float m_curReductionV  = 0.0f;
    float m_baseH          = 0.0f;
    float m_baseV          = 0.0f;
    float m_randFactor     = 1.0f;
    bool  m_overshooting   = false;
    int   m_overshootTick  = 0;
};

// GrimAC-safe rotation delta limits
namespace grimac {
    constexpr float MAX_YAW_DELTA   = 28.0f;
    constexpr float MAX_PITCH_DELTA = 20.0f;
    inline float safeYawDelta(float desired) {
        if (desired >  MAX_YAW_DELTA) return  MAX_YAW_DELTA + randFloat(-2.0f, 2.0f);
        if (desired < -MAX_YAW_DELTA) return -MAX_YAW_DELTA + randFloat(-2.0f, 2.0f);
        return desired;
    }
    inline float safePitchDelta(float desired) {
        if (desired >  MAX_PITCH_DELTA) return  MAX_PITCH_DELTA + randFloat(-1.0f, 1.0f);
        if (desired < -MAX_PITCH_DELTA) return -MAX_PITCH_DELTA + randFloat(-1.0f, 1.0f);
        return desired;
    }
    inline bool isSuspicious(float yawDelta, float pitchDelta) {
        return fabs(yawDelta) > MAX_YAW_DELTA || fabs(pitchDelta) > MAX_PITCH_DELTA;
    }
} // namespace grimac

} // namespace ac
