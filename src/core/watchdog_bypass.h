#pragma once
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

// WATCHDOG_BYPASS -- Anti-pattern-breaking utilities.
//
// Watchdog (and most anticheats) look for:
//   1. Perfectly rhythmic packet intervals          -> PacketScheduler
//   2. Rotation that snaps instantly or is static   -> SineJitter
//   3. TP-Reach position landing on grid coords     -> MicroOffset

namespace wd {

// Seeded RNG (one per module, thread-safe via static local)
inline std::mt19937& rng() {
    static std::mt19937 r(std::random_device{}());
    return r;
}

// PacketScheduler -- breaks rhythmic attack patterns
//
// Watchdog detects "20 attacks at exactly 50ms intervals" instantly.
// This scheduler adds a per-attack non-uniform delay that follows a
// beta-like distribution: most delays are near the target, but some
// are significantly longer or shorter. The average stays correct.
class PacketScheduler {
public:
    void reset() {
        m_burstCount = 0;
        m_burstStart = std::chrono::steady_clock::now();
        m_lastAt = 0;
        m_phase = 0.0f;
    }

    // nextDelay -- compute delay (ms) until next packet.
    //   baseMs    = 1000 / CPS (e.g. 1000/18 = 55ms)
    //   jitterPct = 0..1 (e.g. 0.25 = 25 percent jitter)
    // Returns ms to wait, always 20ms to 3x baseMs.
    long long nextDelay(long long baseMs, float jitterPct) {
        if (baseMs < 1) baseMs = 1;

        m_burstCount++;
        auto now = std::chrono::steady_clock::now();
        auto burstEl = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_burstStart).count();

        // Every 300-800ms of continuous attack, insert a breather delay
        std::uniform_int_distribution<int> burstGate(300, 800);
        if (burstEl > burstGate(rng()) && m_burstCount > 3) {
            m_burstCount = 0;
            m_burstStart = now;
            std::uniform_real_distribution<float> d(1.4f, 2.5f);
            long long breather = (long long)(baseMs * d(rng()));
            std::uniform_real_distribution<float> phaseShift(0.0f, 0.4f);
            m_phase = phaseShift(rng());
            return breather;
        }

        // Core jitter: beta-like distribution (not uniform)
        float strength = std::max(0.0f, std::min(1.0f, jitterPct));
        float a = distFloat(0.6f, 1.0f);
        float b = distFloat(0.6f, 1.0f);
        float blend = (a + b) * 0.5f;

        float range = baseMs * strength;
        float offset = (blend - 0.8f) * range * 2.5f;

        long long result = baseMs + (long long)offset;
        if (result < 20) result = 20;
        if (result > baseMs * 3) result = baseMs * 3;

        // Phase accumulator shifts effective delay every tick
        std::uniform_real_distribution<float> phaseStep(-0.3f, 0.3f);
        m_phase += phaseStep(rng());
        if (m_phase > 1.0f) m_phase -= 1.0f;
        if (m_phase < -1.0f) m_phase += 1.0f;
        result = (long long)(result * (1.0f + m_phase * 0.15f));
        if (result < 20) result = 20;

        return result;
    }

    void markAttack() {
        m_lastAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

private:
    int m_burstCount = 0;
    std::chrono::steady_clock::time_point m_burstStart = std::chrono::steady_clock::now();
    long long m_lastAt = 0;
    float m_phase = 0.0f;

    float distFloat(float lo, float hi) {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng());
    }
};

// SineJitter -- human-like sinusoidal aim sway
//
// Humans sway at 3-8 Hz. Watchdog flags "locked on target with zero
// movement" as an aimbot signature. This generates continuous sine-based
// offsets that are phase-continuous, frequency-randomized, and never
// stay at zero for more than ~50ms.
class SineJitter {
public:
    void reset() {
        m_freq = distFloat(3.0f, 7.0f);
        m_amp  = 0.0f;
        m_targetAmp = distFloat(0.3f, 1.0f);
        m_time = 0.0f;
        m_phaseYaw   = distFloat(0.0f, 6.283f);
        m_phasePitch = distFloat(0.0f, 6.283f);
    }

    struct Offset { float yaw; float pitch; };

    // tick -- advance sine wave by dt (seconds), return yaw/pitch offset.
    //   dt       = time since last frame (e.g. 0.016 for 60fps)
    //   maxDeg   = cap amplitude (e.g. jitterDeg slider value)
    //   rageMode = if true, use subtler jitter (0.3-1 deg instead of 2-4)
    Offset tick(float dt, float maxDeg, bool rageMode) {
        m_time += dt;

        // Smoothly ramp amplitude toward target (avoids sudden jumps)
        float rampSpeed = 2.5f;
        if (m_amp < m_targetAmp)
            m_amp = std::min(m_targetAmp, m_amp + rampSpeed * dt);
        else
            m_amp = std::max(m_targetAmp, m_amp - rampSpeed * dt);

        // Periodically shift target amplitude (every 1-3 seconds)
        if (fmodf(m_time, distFloat(1.0f, 3.0f)) < dt) {
            m_targetAmp = distFloat(0.2f, rageMode ? 0.7f : maxDeg);
            m_freq = distFloat(3.0f, 7.0f);
        }

        float amp = std::min(m_amp, maxDeg);
        float yawOff   = sinf(m_time * m_freq * 6.283f + m_phaseYaw)   * amp;
        float pitchOff = sinf(m_time * m_freq * 6.283f + m_phasePitch) * amp * 0.6f;

        return {yawOff, pitchOff};
    }

private:
    float m_freq = 5.0f;
    float m_amp  = 0.0f;
    float m_targetAmp = 0.5f;
    float m_time = 0.0f;
    float m_phaseYaw = 0.0f;
    float m_phasePitch = 0.0f;

    float distFloat(float lo, float hi) {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng());
    }
};

// MicroOffset -- tiny random position offsets for TP-Reach
//
// When TP-Reach sets the player exactly at "dist - 2.9" every attack,
// Watchdog sees identical relative coordinates. MicroOffset adds
// +/-0.03 blocks of noise per axis so no two attacks hit from the
// same spot.
struct MicroOffset {
    double x = 0, y = 0, z = 0;

    void regenerate() {
        auto& r = rng();
        std::uniform_real_distribution<double> d(-0.035, 0.035);
        x = (d(r) + d(r)) * 0.5;
        y = (d(r) + d(r)) * 0.5 * 0.3;
        z = (d(r) + d(r)) * 0.5;
    }

    static double wobble(double val, double amount = 0.008) {
        auto& r = rng();
        std::uniform_real_distribution<double> d(-amount, amount);
        return val + d(r);
    }
};

} // namespace wd
