#pragma once
#include "pch.h"
#include "sdk/entity.h"

class EspRenderer {
public:
    static void updateCamera(double camX, double camY, double camZ,
                             float yaw, float pitch, int screenW, int screenH);
    static void setFOV(float fov) { s_fov = fov; }
    static void setMatrices(bool hasMatrices, const float* view, const float* proj);

    struct FrameCam {
        double x = 0, y = 0, z = 0;
        float  yaw = 0, pitch = 0, fov = 70.0f;
        int    w = 1920, h = 1080;
        bool   valid = false;
    };
    static FrameCam setupFrame();

    static int getScreenW() { return s_screenW; }
    static int getScreenH() { return s_screenH; }
    static bool hasMatrices() { return s_hasMatrices; }

    static bool worldToScreen(double x, double y, double z, ImVec2& out);
    // Tracer projection that also works for entities BEHIND the camera
    // (extends the line to the screen edge in the direction of the target).
    static bool tracerToScreen(double x, double y, double z, ImVec2& out);

    struct EspColors {
        ImU32 boxVisible;
        ImU32 boxHidden;
        ImU32 text;
        ImU32 bgColor;
    };
    static void drawEntity(const CEntity& entity, bool drawBox, bool drawName,
                           bool drawHealth, bool drawDistance, const EspColors& colors,
                           bool drawArmor = false);
    static void draw3DBox(double minX, double minY, double minZ,
                          double maxX, double maxY, double maxZ,
                          ImU32 color, float thickness = 1.0f);

private:
    static inline double s_camX = 0, s_camY = 0, s_camZ = 0;
    static inline float  s_yaw  = 0, s_pitch = 0;
    static inline int    s_screenW = 1920, s_screenH = 1080;
    static inline float  s_fov  = 70.0f;
    static inline bool   s_hasMatrices = false;
    static inline float  s_viewMatrix[16] = {0};
    static inline float  s_projMatrix[16] = {0};

    // Wrap a yaw angle to [-180, 180]
    static double wrapDeg(double a) {
        while (a > 180.0) a -= 360.0;
        while (a < -180.0) a += 360.0;
        return a;
    }
};
