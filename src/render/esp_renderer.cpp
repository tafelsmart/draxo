#include "pch.h"
#include "render/esp_renderer.h"
#include "sdk/minecraft.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Trigonometry-based World-to-Screen.
 *
 * Minecraft coordinate system:
 *   yaw=0 → south (+Z), yaw=90 → west (-X), yaw=180 → north (-Z), yaw=-90 → east (+X)
 *   pitch>0 → looking down, pitch<0 → looking up
 *
 * Camera forward vector: (-sin(yaw), -sin(pitch)*cos(yaw... no, simpler:
 *   forward = (-sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch))
 *
 * View-space transform:
 *   1. Subtract camera position
 *   2. Rotate by +yaw around Y to align forward with +Z
 *   3. Rotate by -pitch around X to level the view
 *   4. Perspective divide using FOV
 */

static int s_debugTick = 0;

void EspRenderer::updateCamera(double camX, double camY, double camZ,
                                float yaw, float pitch,
                                int screenW, int screenH) {
    s_camX = camX;
    s_camY = camY;
    s_camZ = camZ;
    s_yaw  = yaw;
    s_pitch = pitch;
    s_screenW = screenW;
    s_screenH = screenH;
}
void EspRenderer::setMatrices(bool hasMatrices, const float* view, const float* proj) {
    s_hasMatrices = hasMatrices;
    if (hasMatrices) {
        for (int i = 0; i < 16; i++) {
            s_viewMatrix[i] = view[i];
            s_projMatrix[i] = proj[i];
        }
    }
}

EspRenderer::FrameCam EspRenderer::setupFrame() {
    FrameCam out{};
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    out.w = (int)disp.x;
    out.h = (int)disp.y;
    out.fov = 70.0f;

    // Camera yaw/pitch from GameRenderer.getMainCamera() — includes head
    // rotation and is the ACTUAL view direction the player sees.
    // Entity.getYRot() is body rotation only → causes box offset when
    // the player looks sideways while moving straight.
    auto cam = CMinecraft::getCameraData();

    // Player entity position + eye height (more reliable across MC versions
    // than Camera.position Vec3 reads, which can lag behind by one frame).
    JNIEnv* env = JvmWrapper::getEnv();
    jobject pl = env ? CMinecraft::getPlayer() : nullptr;
    if (pl) {
        CEntity p(pl);
        out.x = p.getX(); out.y = p.getY() + 1.62; out.z = p.getZ();
        // Camera rotation is king — always prefer it over entity body rotation
        if (cam.valid) {
            out.yaw   = cam.yaw;
            out.pitch = cam.pitch;
            if (cam.fov > 5.0f) out.fov = cam.fov;
        } else {
            out.yaw   = p.getYaw();
            out.pitch = p.getPitch();
        }
        out.valid = true;
        env->DeleteLocalRef(pl);
    } else if (cam.valid) {
        // Last resort: use entire camera data (no player entity available)
        out.x = cam.x; out.y = cam.y; out.z = cam.z;
        out.yaw = cam.yaw; out.pitch = cam.pitch;
        out.fov = cam.fov;
        out.valid = true;
    }

    // Always use robust trig projection (matrices unreliable across 1.21.x)
    setFOV(out.fov);
    setMatrices(false, nullptr, nullptr);
    updateCamera(out.x, out.y, out.z, out.yaw, out.pitch, out.w, out.h);
    return out;
}

bool EspRenderer::worldToScreen(double x, double y, double z, ImVec2& out) {
    // Relative position (target minus camera)
    double dx = x - s_camX;
    double dy = y - s_camY;
    double dz = z - s_camZ;

    if (s_hasMatrices) {
        // 4x4 Matrix multiplication (column-major)
        // Vector is (dx, dy, dz, 1.0)
        double clipX = dx * s_viewMatrix[0] + dy * s_viewMatrix[4] + dz * s_viewMatrix[8]  + 1.0 * s_viewMatrix[12];
        double clipY = dx * s_viewMatrix[1] + dy * s_viewMatrix[5] + dz * s_viewMatrix[9]  + 1.0 * s_viewMatrix[13];
        double clipZ = dx * s_viewMatrix[2] + dy * s_viewMatrix[6] + dz * s_viewMatrix[10] + 1.0 * s_viewMatrix[14];
        double clipW = dx * s_viewMatrix[3] + dy * s_viewMatrix[7] + dz * s_viewMatrix[11] + 1.0 * s_viewMatrix[15];

        // Now multiply by projection matrix
        double ndcX = clipX * s_projMatrix[0] + clipY * s_projMatrix[4] + clipZ * s_projMatrix[8]  + clipW * s_projMatrix[12];
        double ndcY = clipX * s_projMatrix[1] + clipY * s_projMatrix[5] + clipZ * s_projMatrix[9]  + clipW * s_projMatrix[13];
        double ndcZ = clipX * s_projMatrix[2] + clipY * s_projMatrix[6] + clipZ * s_projMatrix[10] + clipW * s_projMatrix[14];
        double ndcW = clipX * s_projMatrix[3] + clipY * s_projMatrix[7] + clipZ * s_projMatrix[11] + clipW * s_projMatrix[15];

        if (ndcW < 0.1f) return false; // Behind camera

        // Perspective divide
        ndcX /= ndcW;
        ndcY /= ndcW;

        // Convert to screen space (Y is inverted)
        double nx = (ndcX * 0.5 + 0.5) * s_screenW;
        double ny = (1.0 - (ndcY * 0.5 + 0.5)) * s_screenH;
        if (!std::isfinite(nx) || !std::isfinite(ny)) return false;
        out.x = (float)nx;
        out.y = (float)ny;
        return true;
    }

    // Fallback: Trigonometry
    double yawRad   = s_yaw   * (M_PI / 180.0);
    double pitchRad = s_pitch * (M_PI / 180.0);

    double cosYaw   = cos(yawRad);
    double sinYaw   = sin(yawRad);
    double cosPitch = cos(pitchRad);
    double sinPitch = sin(pitchRad);

    double viewX = -(dx * cosYaw + dz * sinYaw);
    double viewY = dy;
    double viewZ = -dx * sinYaw + dz * cosYaw;

    double finalX = viewX;
    double finalY = viewY * cosPitch + viewZ * sinPitch;
    double finalZ = -viewY * sinPitch + viewZ * cosPitch;

    if (!(finalZ > 0.05)) return false;

    double fovRad = s_fov * (M_PI / 180.0);
    if (!(fovRad > 0.01)) return false;   // NaN/FOV-Schutz
    double halfTanFov = tan(fovRad / 2.0);
    double aspect = (s_screenH > 0) ? (double)s_screenW / (double)s_screenH : 1.0;
    double fallbackNdcX =  (finalX / (finalZ * halfTanFov * aspect));
    double fallbackNdcY = -(finalY / (finalZ * halfTanFov));

    double nx = (fallbackNdcX * 0.5 + 0.5) * s_screenW;
    double ny = (fallbackNdcY * 0.5 + 0.5) * s_screenH;
    if (!std::isfinite(nx) || !std::isfinite(ny)) return false;
    out.x = (float)nx;
    out.y = (float)ny;

    return true;
}

bool EspRenderer::tracerToScreen(double x, double y, double z, ImVec2& out) {
    // First try the normal projection
    if (worldToScreen(x, y, z, out)) return true;

    // Behind camera — compute the relative yaw/pitch and extend to screen edge
    double dx = x - s_camX;
    double dy = y - s_camY;
    double dz = z - s_camZ;
    double hDist = std::sqrt(dx*dx + dz*dz);
    if (hDist < 0.001) return false;

    // Minecraft yaw convention: yaw=0 → +Z, yaw=90 → -X
    double yawTo   = std::atan2(-dx, dz) * (180.0 / M_PI);
    double pitchTo = -std::atan2(dy, hDist) * (180.0 / M_PI);

    double yawDiff   = wrapDeg(yawTo   - s_yaw);
    double pitchDiff = pitchTo - s_pitch;

    // Screen-space direction using FOV
    double fovH = s_fov * (M_PI / 180.0);
    double fovV = fovH * ((double)s_screenH / (double)std::max(s_screenW, 1));
    double tanH = std::tan(fovH / 2.0);
    double tanV = std::tan(fovV / 2.0);

    double cx = s_screenW * 0.5;
    double cy = s_screenH * 0.5;
    double nx = cx + std::tan(yawDiff * M_PI / 180.0) / tanH * cx;
    double ny = cy - std::tan(pitchDiff * M_PI / 180.0) / tanV * cy;

    // Clamp to the screen edge along the direction from center
    double vx = nx - cx, vy = ny - cy;
    double len = std::sqrt(vx*vx + vy*vy);
    if (len < 0.001) return false;
    double sxMax = (vx >= 0) ? ((s_screenW - 1) - cx) / std::fabs(vx) : (cx - 1) / std::fabs(vx);
    double syMax = (vy >= 0) ? ((s_screenH - 1) - cy) / std::fabs(vy) : (cy - 1) / std::fabs(vy);
    double scale = std::min(sxMax, syMax);
    out.x = (float)(cx + vx * scale);
    out.y = (float)(cy + vy * scale);
    return true;
}

void EspRenderer::drawEntity(const CEntity& entity, bool drawBox, bool drawName, bool drawHealth, bool drawDistance, const EspColors& colors, bool drawArmor) {
    if (!entity.isValid()) return;

    double ex = entity.getX();
    double ey = entity.getY();
    double ez = entity.getZ();

    auto bbox = entity.getBoundingBox();

    // ── Fallback: Falls AABB degeneriert (alle Werte 0), baue eine
    //     vernünftige Box um die Entity-Position herum. Fix für den
    //     Bug, dass Boxes/Names/Health/Distance nicht rendern. ───────
    if (bbox.minX == 0.0 && bbox.maxX == 0.0 &&
        bbox.minY == 0.0 && bbox.maxY == 0.0 &&
        bbox.minZ == 0.0 && bbox.maxZ == 0.0) {
        float w = entity.isPlayer() ? 0.6f : (entity.isItem() ? 0.25f : 0.6f);
        float h = entity.isPlayer() ? 1.8f : (entity.isItem() ? 0.25f : 1.0f);
        bbox.minX = ex - w/2; bbox.maxX = ex + w/2;
        bbox.minY = ey;       bbox.maxY = ey + h;
        bbox.minZ = ez - w/2; bbox.maxZ = ez + w/2;
    }

    ImVec2 corners[8];
    bool visible[8];
    double pts[8][3] = {
        {bbox.minX, bbox.minY, bbox.minZ},
        {bbox.maxX, bbox.minY, bbox.minZ},
        {bbox.minX, bbox.maxY, bbox.minZ},
        {bbox.maxX, bbox.maxY, bbox.minZ},
        {bbox.minX, bbox.minY, bbox.maxZ},
        {bbox.maxX, bbox.minY, bbox.maxZ},
        {bbox.minX, bbox.maxY, bbox.maxZ},
        {bbox.maxX, bbox.maxY, bbox.maxZ},
    };

    bool anyVisible = false;
    int visCount = 0;
    for (int i = 0; i < 8; i++) {
        visible[i] = worldToScreen(pts[i][0], pts[i][1], pts[i][2], corners[i]);
        if (visible[i]) { anyVisible = true; visCount++; }
    }



    if (!anyVisible) return;

    float minSX = 99999.f, minSY = 99999.f;
    float maxSX = -99999.f, maxSY = -99999.f;
    for (int i = 0; i < 8; i++) {
        if (!visible[i]) continue;
        minSX = std::min(minSX, corners[i].x);
        minSY = std::min(minSY, corners[i].y);
        maxSX = std::max(maxSX, corners[i].x);
        maxSY = std::max(maxSY, corners[i].y);
    }



    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    if (drawBox) {
        // Outer black outline
        draw->AddRect(ImVec2(minSX - 1, minSY - 1), ImVec2(maxSX + 1, maxSY + 1), IM_COL32(0, 0, 0, 120), 0.0f, 0, 1.0f);
        // Inner colored box
        draw->AddRect(ImVec2(minSX, minSY), ImVec2(maxSX, maxSY), colors.boxVisible, 0.0f, 0, 1.5f);
    }

    if (drawName) {
        std::string name = entity.getName();
        if (name.empty()) name = "Player";
        ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
        float textX = (minSX + maxSX) / 2.0f - textSize.x / 2.0f;
        float textY = minSY - textSize.y - 4.0f;

        draw->AddRectFilled(ImVec2(textX - 2, textY - 1), ImVec2(textX + textSize.x + 2, textY + textSize.y + 1), colors.bgColor, 2.0f);
        draw->AddText(ImVec2(textX, textY), colors.text, name.c_str());
    }

    if (drawDistance) {
        double dx = ex - s_camX;
        double dy = ey - s_camY;
        double dz = ez - s_camZ;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        char distStr[32];
        snprintf(distStr, sizeof(distStr), "[%.1fm]", dist);
        
        ImVec2 textSize = ImGui::CalcTextSize(distStr);
        float textX = (minSX + maxSX) / 2.0f - textSize.x / 2.0f;
        float textY = maxSY + 2.0f;

        draw->AddRectFilled(ImVec2(textX - 2, textY - 1), ImVec2(textX + textSize.x + 2, textY + textSize.y + 1), colors.bgColor, 2.0f);
        draw->AddText(ImVec2(textX, textY), colors.text, distStr);
    }

    if (drawHealth) {
        float health = entity.getHealth();
        float maxHP  = entity.getMaxHealth();
        if (maxHP > 0.0f) {
            float ratio = std::max(0.0f, std::min(1.0f, health / maxHP));
            float barWidth = 3.0f;
            float barX = maxSX + 3.0f;
            float barHeight = maxSY - minSY;

            draw->AddRectFilled(ImVec2(barX, minSY), ImVec2(barX + barWidth, maxSY), IM_COL32(0, 0, 0, 150));
            ImU32 color = (ratio > 0.5f) ? 
                IM_COL32((int)((1.0f - ratio) * 2 * 255), 255, 0, 255) : 
                IM_COL32(255, (int)(ratio * 2 * 255), 0, 255);

            draw->AddRectFilled(ImVec2(barX, minSY + barHeight * (1.0f - ratio)), ImVec2(barX + barWidth, maxSY), color);
        }
    }
}

void EspRenderer::draw3DBox(double minX, double minY, double minZ, double maxX, double maxY, double maxZ, ImU32 color, float thickness) {
    ImVec2 corners[8];
    bool visible[8];
    double pts[8][3] = {
        {minX, minY, minZ}, // 0
        {maxX, minY, minZ}, // 1
        {minX, maxY, minZ}, // 2
        {maxX, maxY, minZ}, // 3
        {minX, minY, maxZ}, // 4
        {maxX, minY, maxZ}, // 5
        {minX, maxY, maxZ}, // 6
        {maxX, maxY, maxZ}, // 7
    };

    bool anyVisible = false;
    for (int i = 0; i < 8; i++) {
        visible[i] = worldToScreen(pts[i][0], pts[i][1], pts[i][2], corners[i]);
        if (visible[i]) anyVisible = true;
    }

    if (!anyVisible) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    auto drawLine = [&](int a, int b) {
        if (visible[a] && visible[b]) {
            draw->AddLine(corners[a], corners[b], color, thickness);
        }
    };

    // Bottom face
    drawLine(0, 1);
    drawLine(1, 5);
    drawLine(5, 4);
    drawLine(4, 0);

    // Top face
    drawLine(2, 3);
    drawLine(3, 7);
    drawLine(7, 6);
    drawLine(6, 2);

    // Pillars
    drawLine(0, 2);
    drawLine(1, 3);
    drawLine(4, 6);
    drawLine(5, 7);
}
