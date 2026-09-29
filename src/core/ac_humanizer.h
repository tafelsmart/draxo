#pragma once
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

namespace ac {

inline std::mt19937& humRng() {
    static std::random_device rd;
    static std::mt19937 r(rd());
    return r;
}
inline float humRand(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(humRng());
}

// CPSHumanizer -- Attack-rate decay with micro-pauses.
//  Watchdog detects "flawless CPS for 10+ seconds" as KillAura.
class CPSHumanizer {
public:
    void reset() {
        m_burstStart = std::chrono::steady_clock::now();
        m_inPause = false;
        m_pauseUntil = 0;
        m_nextPauseAt = humRand(8.0f, 15.0f);
    }
    float getEffectiveCps(float targetCps, bool rageMode) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - m_burstStart).count();
        if (!m_inPause && elapsed >= m_nextPauseAt) {
            m_inPause = true;
            m_pauseUntil = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() + (long long)humRand(150, 400);
        }
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        if (m_inPause && nowMs >= m_pauseUntil) { m_inPause = false; m_burstStart = now; m_nextPauseAt = humRand(8.0f, 15.0f); }
        if (m_inPause) return 0.0f;
        float decay;
        if (elapsed < 3.0f)      decay = 1.0f;
        else if (elapsed < 8.0f)  decay = 1.0f - 0.05f * (elapsed - 3.0f) / 5.0f;
        else if (elapsed < 20.0f) decay = 0.95f - 0.10f * (elapsed - 8.0f) / 12.0f;
        else                      decay = humRand(0.70f, 0.90f);
        if (rageMode) decay = 1.0f - (1.0f - decay) * 0.5f;
        return targetCps * decay * (1.0f + humRand(-0.03f, 0.03f));
    }
private:
    std::chrono::steady_clock::time_point m_burstStart{};
    bool m_inPause = false;
    long long m_pauseUntil = 0;
    float m_nextPauseAt = 10.0f;
};

// ReachWobbler -- Per-tick reach variation (2-octave noise).
//  Watchdog sees "always exactly X blocks" and fingerprints it.
class ReachWobbler {
public:
    void reset() {
        m_time = 0.0f;
        m_phase1 = humRand(0.0f, 6.283f);
        m_phase2 = humRand(0.0f, 6.283f);
        m_freq1 = humRand(2.0f, 4.0f);
        m_freq2 = humRand(7.0f, 11.0f);
        m_slipTimer = 0.0f; m_nextSlip = humRand(25.0f, 40.0f);
        m_inSlip = false; m_slipDuration = 0.0f;
    }
    float wobble(float dt) {
        m_time += dt; m_slipTimer += dt;
        if (!m_inSlip && m_slipTimer >= m_nextSlip) { m_inSlip = true; m_slipDuration = 0.0f; }
        if (m_inSlip) { m_slipDuration += dt; if (m_slipDuration > humRand(0.08f, 0.20f)) { m_inSlip = false; m_slipTimer = 0.0f; m_nextSlip = humRand(25.0f, 40.0f); } }
        float drift  = sinf(m_time * m_freq1 * 6.283f + m_phase1) * 0.012f;
        float tremor = sinf(m_time * m_freq2 * 6.283f + m_phase2) * 0.005f;
        float result = drift + tremor + (m_inSlip ? humRand(0.03f, 0.06f) : 0.0f);
        return std::max(-0.025f, std::min(0.025f, result));
    }
private:
    float m_time=0,m_phase1=0,m_phase2=0,m_freq1=3,m_freq2=9;
    float m_slipTimer=0,m_nextSlip=30,m_slipDuration=0;
    bool m_inSlip=false;
};

// TimerNoiser -- Micro-fluctuation for TPS spoofing.
//  GrimAC detects "zero jitter timer" — perfect 20.0 TPS = cheat.
class TimerNoiser {
public:
    void reset() { m_time=0; m_lagTimer=0; m_nextLag=humRand(120.f,300.f); m_inLag=false; m_lagDuration=0; m_lagAmount=0; }
    float noiseMs(float dt) {
        m_time+=dt; m_lagTimer+=dt;
        if (!m_inLag && m_lagTimer>=m_nextLag) { m_inLag=true; m_lagDuration=0; m_lagAmount=humRand(2.f,5.f); }
        if (m_inLag) { m_lagDuration+=dt; if (m_lagDuration>humRand(0.15f,0.40f)) { m_inLag=false; m_lagTimer=0; m_nextLag=humRand(120.f,300.f); } }
        float r=humRand(0.f,1.f), jitter;
        if (r<0.65f) jitter=humRand(-0.2f,0.2f);
        else if (r<0.95f) jitter=humRand(0.2f,0.4f)*(humRand(0,1)?1.f:-1.f);
        else jitter=humRand(0.4f,0.5f)*(humRand(0,1)?1.f:-1.f);
        return jitter+(m_inLag?m_lagAmount:0.f);
    }
private:
    float m_time=0,m_lagTimer=0,m_nextLag=200,m_lagDuration=0,m_lagAmount=0;
    bool m_inLag=false;
};

// FlagRandomizer -- Randomized movement flag transitions.
//  GrimAC sees "sprinting=true in 1 tick" as non-human.
class FlagRandomizer {
public:
    void reset() { m_pendingSprint=m_pendingSneak=false; m_currentSprint=m_currentSneak=false; m_sprintDelay=m_sneakDelay=0; }
    void setDesired(bool wantSprint, bool wantSneak) {
        if (wantSprint != m_currentSprint && !m_pendingSprint) { m_pendingSprint=true; m_sprintDelay=(int)humRand(2,8); }
        if (wantSprint == m_currentSprint) m_pendingSprint=false;
        if (wantSneak != m_currentSneak && !m_pendingSneak) { m_pendingSneak=true; m_sneakDelay=(int)humRand(3,12); }
        if (wantSneak == m_currentSneak) m_pendingSneak=false;
        if (m_pendingSprint && m_sprintDelay>0 && --m_sprintDelay<=0) { m_currentSprint=wantSprint; m_pendingSprint=false; }
        if (m_pendingSneak && m_sneakDelay>0 && --m_sneakDelay<=0) { m_currentSneak=wantSneak; m_pendingSneak=false; }
    }
    bool currentSprint() const { return m_currentSprint; }
    bool currentSneak()  const { return m_currentSneak; }
private:
    bool m_pendingSprint=false,m_pendingSneak=false,m_currentSprint=false,m_currentSneak=false;
    int m_sprintDelay=0,m_sneakDelay=0;
};

} // namespace ac
