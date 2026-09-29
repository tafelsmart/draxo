#include "pch.h"
#include "core/strcrypt.h"
#include "modules/nohurtcam.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

NoHurtCam::NoHurtCam() : Module("NoHurtCam", ModuleCategory::RENDER, 0, "Removes the camera shake when you take damage") {}
void NoHurtCam::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject mc = CMinecraft::getInstance(); if (!mc) return;
    static jfieldID s_hurtTime = nullptr; static bool init = false;
    if (!init) { jclass c=env->GetObjectClass(mc); s_hurtTime=env->GetFieldID(c,"hurtTime","I");
    if(env->ExceptionCheck()){env->ExceptionClear();s_hurtTime=nullptr;} env->DeleteLocalRef(c); init=true; }
    if(s_hurtTime) env->SetIntField(mc,s_hurtTime,0);
    env->DeleteLocalRef(mc);
}
