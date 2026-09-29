#include "pch.h"
#include "core/strcrypt.h"
#include "modules/fakelag.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"
#include <vector>

/*
 * FakeLag v2 — Packet-basierte Lag-Simulation.
 *
 * Prinzip: Der CLIENT läuft völlig normal (60fps, flüssige Bewegung).
 * Nur die POSITIONSPAKETE zum Server werden verzögert:
 *  1. Wir speichern die Position, die der Server zuletzt gesehen hat.
 *  2. Während der Delay-Phase senden wir KEINE neuen Positionspakete.
 *  3. Nach Ablauf des Delays senden wir EIN Paket mit der aktuellen
 *     Position → der Server sieht ein „Teleport" zur echten Position.
 *  4. Andere Spieler sehen dich ruckeln/laggen, DU läufst flüssig.
 *
 * Zusätzlich: Gelegentliche „Lag-Spikes" — über einen kurzen Moment
 * wird das Timer.msPerTick kurz hochgesetzt, wodurch der Game-Tick
 * aussetzt (keine Pakete) aber der Render-Thread weiterläuft.
 */

FakeLag::FakeLag() : Module("FakeLag", ModuleCategory::EXPLOIT, 0, "Delays your packets — appears laggy to others while you stay smooth") {
    defineFloat("delay",   "Delay (ms)",       200.0f,  50.0f, 2000.0f, "%.0f");
    defineFloat("spike",   "Spike Chance %",    25.0f,   0.0f, 100.0f, "%.0f");
    defineBool("packets",  "Packet Spoof",      true);
    addSearchTag("ping");
    addSearchTag("lag");
    addSearchTag("blink");
}

void FakeLag::onEnable() {
    m_frozenPos = {-1, -1, -1};
    m_lastSend  = std::chrono::steady_clock::now();
    m_jniInit   = false;
}

void FakeLag::onDisable() {
    m_frozenPos = {-1, -1, -1};
}

void FakeLag::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;

    // ── One-time JNI init ────────────────────────────────────────────
    if (!m_jniInit) {
        s_mcCls    = JvmWrapper::findClass(Mappings::Minecraft_Class);
        s_moveCls  = JvmWrapper::findClass(Mappings::ServerboundMovePlayerPacket_Pos_Class);
        s_ccpliCls = JvmWrapper::findClass(Mappings::ClientCommonPacketListenerImpl_Class);
        s_connCls  = JvmWrapper::findClass(Mappings::Connection_Class);

        if (s_mcCls) {
            s_getConn  = env->GetMethodID(s_mcCls, Mappings::MC_getConnection,
                                          Mappings::MC_getConnection_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getConn = nullptr; }
        }
        if (s_ccpliCls) {
            s_connField = env->GetFieldID(s_ccpliCls, Mappings::CCPLI_connection,
                                          Mappings::CCPLI_connection_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_connField = nullptr; }
        }
        if (s_connCls) {
            s_connSend = env->GetMethodID(s_connCls, Mappings::Connection_send,
                                          Mappings::Connection_send_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_connSend = nullptr; }
        }
        if (s_moveCls) {
            s_moveInit = env->GetMethodID(s_moveCls, Mappings::MovePacketPos_Init,
                                          Mappings::MovePacketPos_Init_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_moveInit = nullptr; }
            if (!s_moveInit) {
                s_moveInit = env->GetMethodID(s_moveCls, Mappings::MovePacketPos_Init,
                                              Mappings::MovePacketPos_Init_Sig_Legacy);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_moveInit = nullptr; }
            }
        }
        // Timer für Lag-Spikes
        s_timerCls = JvmWrapper::findClass(Mappings::Timer_Class);
        if (s_timerCls) {
            s_msPerTick = env->GetFieldID(s_timerCls, Mappings::Timer_msPerTick,
                                          Mappings::Timer_msPerTick_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_msPerTick = nullptr; }
        }

        m_jniInit = true;
        printf(STR_C("[Draxo] FakeLag JNI: mc=%p move=%p connField=%p connSend=%p moveInit=%p timer=%p mspt=%p\n"),
               (void*)s_mcCls, (void*)s_moveCls, (void*)s_connField,
               (void*)s_connSend, (void*)s_moveInit, (void*)s_timerCls, (void*)s_msPerTick);
    }

    if (!s_mcCls || !s_moveCls || !s_moveInit) return;

    // ── Get current player position ──────────────────────────────────
    jobject playerObj = CMinecraft::getPlayer();
    if (!playerObj) return;
    CEntity player(playerObj);
    double px = player.getX(), py = player.getY(), pz = player.getZ();
    float  yaw = player.getYaw(), pitch = player.getPitch();
    bool   onGround = player.isOnGround();
    env->DeleteLocalRef(playerObj);

    float delay   = m_floatSettings["delay"];
    float spikeChance = m_floatSettings["spike"] / 100.0f;
    bool  sendPackets = m_boolSettings["packets"];

    auto now = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastSend).count();

    // ── Lag-Spike: gelegentlicher Timer-Hochsetzer ──────────────────
    // Kurzer Timer-Spike stoppt den Game-Tick (→ keine Pakete) für
    // wenige Frames. Der Render-Thread läuft weiter. Andere Spieler
    // sehen einen kurzen „Freeze", du selbst läufst normal.
    if (spikeChance > 0.001f && s_msPerTick && s_timerCls) {
        static auto lastSpike = std::chrono::steady_clock::now();
        static bool inSpike = false;
        static auto spikeStart = std::chrono::steady_clock::now();
        auto spikeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSpike).count();

        if (!inSpike && spikeElapsed > 500) {
            // Random trigger: spikeChance chance every 500ms to start a spike
            int roll = (int)(m_rng() % 100);
            if (roll < (int)(spikeChance * 100)) {
                inSpike = true;
                spikeStart = now;
            }
            lastSpike = now;
        }

        if (inSpike) {
            auto spikeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - spikeStart).count();
            if (spikeMs < delay * 0.3f) {
                // Spike active: briefly set high msPerTick to stall packets
                jobject mc = CMinecraft::getInstance();
                if (mc && !env->ExceptionCheck()) {
                    jobject timer = nullptr;
                    static jfieldID s_timerField = nullptr;
                    if (!s_timerField) {
                        s_timerField = env->GetFieldID(env->GetObjectClass(mc), Mappings::MC_getDeltaTracker,
                                                       Mappings::MC_getDeltaTracker_Sig);
                        // Fallback: direktes timer-Feld
                        if (env->ExceptionCheck()) { env->ExceptionClear();
                            s_timerField = env->GetFieldID(env->GetObjectClass(mc),
                                Mappings::MC_timer, Mappings::MC_timer_Sig);
                            if (env->ExceptionCheck()) { env->ExceptionClear(); s_timerField = nullptr; }
                        }
                    }
                    if (s_timerField) {
                        timer = env->GetObjectField(mc, s_timerField);
                        if (timer && !env->ExceptionCheck()) {
                            float origMspt = env->GetFloatField(timer, s_msPerTick);
                            if (!env->ExceptionCheck() && origMspt > 0) {
                                env->SetFloatField(timer, s_msPerTick, origMspt * 30.0f);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                            }
                        }
                        if (timer) env->DeleteLocalRef(timer);
                    }
                    env->DeleteLocalRef(mc);
                }
            } else {
                inSpike = false;
            }
        }
    }

    // ── Packet Spoof: gefrorene Position an Server senden ────────────
    if (sendPackets) {
        if (elapsed >= delay) {
            // Speichere aktuelle Position als „zuletzt gesehen"
            m_frozenPos = {px, py, pz};
            m_lastSend = now;
        }

        // Sende die GEFRORENE Position (nicht die aktuelle) an den Server
        if (m_frozenPos.x > -0.5 && s_ccpliCls && s_connField && s_connSend) {
            jobject mc = CMinecraft::getInstance();
            if (mc && !env->ExceptionCheck()) {
                jobject listener = env->CallObjectMethod(mc, s_getConn);
                if (listener && !env->ExceptionCheck()) {
                    jobject connection = env->GetObjectField(listener, s_connField);
                    if (connection && !env->ExceptionCheck()) {
                        // Baue Positions-Paket mit der GEFRORENEN Position
                        jobject packet = env->NewObject(s_moveCls, s_moveInit,
                            (jdouble)m_frozenPos.x, (jdouble)m_frozenPos.y, (jdouble)m_frozenPos.z,
                            (jboolean)onGround, (jboolean)false);
                        if (packet && !env->ExceptionCheck()) {
                            env->CallVoidMethod(connection, s_connSend, packet);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(packet);
                        } else {
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        env->DeleteLocalRef(connection);
                    }
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(listener);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(mc);
            }
        }
    }
}
