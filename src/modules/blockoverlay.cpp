#include "pch.h"
#include "core/strcrypt.h"
#include "modules/blockoverlay.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "render/esp_renderer.h"

BlockOverlay::BlockOverlay() : Module("BlockOverlay", ModuleCategory::RENDER, 0,
    "Highlights the block you are looking at with a 3D outline.") {
    defineFloat("thickness", "Thickness",   2.0f, 0.5f,  6.0f, "%.1f");
    defineColor("fill",      "Fill Color",  IM_COL32(255, 255, 255, 30));
    defineColor("outline",   "Outline",     IM_COL32(140, 100, 255, 200));
    addSearchTag("block");
    addSearchTag("outline");
    addSearchTag("target");
}

void BlockOverlay::onRender() {
    if (!m_enabled) return;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    auto cam = EspRenderer::setupFrame();
    if (!cam.valid) return;

    // Get Minecraft.hitResult → BlockHitResult.getBlockPos()
    static jfieldID s_hitFld = nullptr;
    static jclass s_bhrCls = nullptr;
    static jmethodID s_bhrGetPos = nullptr;
    static bool s_init = false;

    if (!s_init) {
        s_init = true;
        jclass mcC = JvmWrapper::findClass(Mappings::Minecraft_Class);
        if (mcC) {
            s_hitFld = env->GetFieldID(mcC, "hitResult", "Lnet/minecraft/world/phys/HitResult;");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                s_hitFld = env->GetFieldID(mcC, "crosshairPickEntity", "Lnet/minecraft/world/phys/HitResult;");
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    s_hitFld = env->GetFieldID(mcC, "hit", "Lnet/minecraft/world/phys/HitResult;");
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
            }
        }
        s_bhrCls = JvmWrapper::findClass(Mappings::BlockHitResult_Class);
        if (s_bhrCls) {
            s_bhrGetPos = env->GetMethodID(s_bhrCls, "getBlockPos",
                "()Lnet/minecraft/core/BlockPos;");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                s_bhrGetPos = env->GetMethodID(s_bhrCls, "getPos",
                    "()Lnet/minecraft/core/BlockPos;");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }

    if (!s_hitFld || !s_bhrCls || !s_bhrGetPos) return;

    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    jobject hit = env->GetObjectField(mc, s_hitFld);
    env->DeleteLocalRef(mc);
    if (!hit || env->ExceptionCheck()) { env->ExceptionClear(); return; }

    // Check if it's a BlockHitResult
    if (!env->IsInstanceOf(hit, s_bhrCls)) { env->DeleteLocalRef(hit); return; }

    jobject bp = env->CallObjectMethod(hit, s_bhrGetPos);
    env->DeleteLocalRef(hit);
    if (!bp || env->ExceptionCheck()) { env->ExceptionClear(); return; }

    // Get x, y, z from BlockPos
    static jmethodID s_bpGetX = nullptr, s_bpGetY = nullptr, s_bpGetZ = nullptr;
    static jclass s_bpCls = nullptr;
    if (!s_bpCls) {
        s_bpCls = JvmWrapper::findClass(Mappings::BlockPos_Class);
        if (s_bpCls) {
            s_bpGetX = env->GetMethodID(s_bpCls, "getX", "()I");
            s_bpGetY = env->GetMethodID(s_bpCls, "getY", "()I");
            s_bpGetZ = env->GetMethodID(s_bpCls, "getZ", "()I");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    if (!s_bpGetX) { env->DeleteLocalRef(bp); return; }

    int bx = env->CallIntMethod(bp, s_bpGetX);
    int by = env->CallIntMethod(bp, s_bpGetY);
    int bz = env->CallIntMethod(bp, s_bpGetZ);
    env->DeleteLocalRef(bp);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

    // Draw 3D box around the block
    float thickness = m_floatSettings["thickness"];
    ImU32 outline   = m_colorSettings["outline"];
    ImU32 fill      = m_colorSettings["fill"];

    // Filled box (slightly inset)
    if ((fill & IM_COL32_A_MASK) > 5) {
        EspRenderer::draw3DBox(bx + 0.01, by + 0.01, bz + 0.01,
                               bx + 0.99, by + 0.99, bz + 0.99, fill, 0.0f);
    }

    // Outline
    EspRenderer::draw3DBox(bx, by, bz, bx + 1, by + 1, bz + 1, outline, thickness);
}
