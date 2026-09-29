#pragma once
#include "pch.h"
#include "sdk/entity.h"

/*
 * CMinecraft — Singleton accessor for the Minecraft instance.
 */
class CMinecraft {
public:
    static bool initIDs();

    // Get the Minecraft singleton (local ref — call within PushLocalFrame)
    static jobject getInstance();

    // Get current player (local ref)
    static jobject getPlayer();

    // Get current world (local ref)
    static jobject getWorld();

    // ── Camera data (interpolated render position) ──────────────────
    struct CameraData {
        double x, y, z;
        float  yaw, pitch;
        float  fov;
        int    screenW, screenH;  // Minecraft's actual framebuffer dimensions
        bool   hasMatrices;
        float  viewMatrix[16];    // 4x4 column-major ModelView Matrix
        float  projMatrix[16];    // 4x4 column-major Projection Matrix
        bool   valid;
    };
    // Returns the render camera's interpolated position/rotation + FOV
    static CameraData getCameraData();

    // Aktuelle Server-IP via Minecraft.getCurrentServer()->ServerData.ip
    // ("singleplayer" wenn kein Server). Nutzt die Mappings::-Konstanten,
    // damit sie in obfuskierten Versionen korrekt uebersetzt wird.
    static std::string getServerIp();

    // World-Seed (für Structure ESP) — liest Level.getSeed() via JNI
    static long long getWorldSeed();

    // Dimension-Erkennung ("overworld" / "the_nether" / "the_end")
    static std::string getDimension();

    // ── Debug-Getter (für das Debug-Fenster) ───────────────────────
    static bool idsResolved()      { return (s_getInstance || s_instanceField) && s_player && s_level; }
    static bool cameraResolved()   { return s_camPosition && s_camYRot && s_camXRot; }
    static bool matricesResolved() { return (s_rsGetProj || s_getGRProjMatrix) && s_mat4_m00; }
    static bool projTakesFloat()   { return s_grProjTakesFloat; }
    static bool fovResolved()      { return s_optFov && s_optInstGet; }
    static bool instanceResolved() { return s_getInstance != nullptr || s_instanceField != nullptr; }

private:
    static inline jclass    s_mcClass        = nullptr;
    static inline jmethodID s_getInstance    = nullptr;
    static inline jfieldID  s_instanceField  = nullptr;  // static 'instance' field
    static inline jfieldID  s_player         = nullptr;
    static inline jfieldID  s_level          = nullptr;
    static inline jfieldID  s_gameRenderer   = nullptr;
    static inline jfieldID  s_options        = nullptr;

    // GameRenderer
    static inline jclass    s_grClass        = nullptr;
    static inline jmethodID s_getMainCamera    = nullptr;
    static inline jmethodID s_getGRProjMatrix  = nullptr;
    static inline jmethodID s_getFov           = nullptr;
    // getProjectionMatrix: 1.21.1 = (D)double, 1.21.2+ = (F)float partialTicks
    static inline bool      s_grProjTakesFloat = true;

    // Camera: wir lesen die FELDER position/yRot/xRot direkt. Die Felder existieren
    // in 1.21.1 (eigene Klasse) und 1.21.2+ (Camera ist seit 1.21.2 ein Entity-Subclass,
    // die Felder sind dort weiterhin vorhanden/vererbt).
    static inline jclass    s_camClass       = nullptr;
    static inline jfieldID  s_camPosition    = nullptr;  // Vec3 position
    static inline jfieldID  s_camYRot        = nullptr;  // float yRot
    static inline jfieldID  s_camXRot        = nullptr;  // float xRot

    // Vec3 fields (cached from CEntity — reuse)
    // Options FOV
    static inline jclass    s_optionsClass   = nullptr;
    static inline jfieldID  s_optFov         = nullptr;  // OptionInstance<Integer>
    static inline jmethodID s_optInstGet     = nullptr;  // OptionInstance.get()

    // Window (Minecraft.getWindow() → framebuffer dimensions)
    static inline jfieldID  s_window         = nullptr;  // Minecraft.window field
    static inline jclass    s_windowClass    = nullptr;
    static inline jmethodID s_getWidth       = nullptr;  // Window.getWidth()
    static inline jmethodID s_getHeight      = nullptr;  // Window.getHeight()

    // RenderSystem and Matrix4f
    static inline jclass    s_rsClass        = nullptr;
    static inline jmethodID s_rsGetProj      = nullptr;
    static inline jmethodID s_rsGetModelView = nullptr;
    static inline jclass    s_mat4Class      = nullptr;
    
    // Matrix4f fields (column-major)
    static inline jfieldID  s_mat4_m00 = nullptr, s_mat4_m01 = nullptr, s_mat4_m02 = nullptr, s_mat4_m03 = nullptr;
    static inline jfieldID  s_mat4_m10 = nullptr, s_mat4_m11 = nullptr, s_mat4_m12 = nullptr, s_mat4_m13 = nullptr;
    static inline jfieldID  s_mat4_m20 = nullptr, s_mat4_m21 = nullptr, s_mat4_m22 = nullptr, s_mat4_m23 = nullptr;
    static inline jfieldID  s_mat4_m30 = nullptr, s_mat4_m31 = nullptr, s_mat4_m32 = nullptr, s_mat4_m33 = nullptr;
};
