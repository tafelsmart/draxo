#pragma once
#include "modules/module.h"
#include <chrono>
#include <random>

/*
 * FakeLag v2 — Packet-basierte Lag-Simulation.
 *
 * Packet Spoof: sendet die GEFRORENE Position an den Server,
 * während du dich lokal völlig normal bewegst. Andere Spieler
 * sehen dich an der alten Position „festgefroren", bis das
 * Delay abläuft und du an die neue Position „teleportierst".
 *
 * Lag-Spikes: gelegentliche Timer-Manipulation (msPerTick * 30)
 * für ~30% der Delay-Dauer — stoppt den Game-Tick kurz, der
 * Render-Thread läuft normal weiter. Optional über „Spike Chance".
 */
class FakeLag : public Module {
public:
    FakeLag();
    void onEnable() override;
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;

private:
    // Packet Spoof state
    struct { double x, y, z; } m_frozenPos{-1, -1, -1};
    std::chrono::steady_clock::time_point m_lastSend;
    bool m_jniInit = false;
    std::mt19937 m_rng{std::random_device{}()};

    // JNI cache
    static inline jclass    s_mcCls    = nullptr;
    static inline jmethodID s_getConn  = nullptr;
    static inline jclass    s_ccpliCls = nullptr;
    static inline jfieldID  s_connField = nullptr;
    static inline jclass    s_connCls  = nullptr;
    static inline jmethodID s_connSend = nullptr;
    static inline jclass    s_moveCls  = nullptr;
    static inline jmethodID s_moveInit = nullptr;

    // Timer spike
    static inline jclass    s_timerCls  = nullptr;
    static inline jfieldID  s_msPerTick = nullptr;
};
