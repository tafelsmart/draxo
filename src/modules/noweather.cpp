#include "pch.h"
#include "core/strcrypt.h"
#include "modules/noweather.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"

NoWeather::NoWeather() : Module("NoWeather", ModuleCategory::RENDER, 0, "Removes rain and snow from the world") { setTickInterval(10); }
void NoWeather::onUpdate(JNIEnv* env) {
    if(!m_enabled)return;
    static jfieldID s_rain, s_thunder; static bool init=false;
    if(!init){ jobject mc=CMinecraft::getInstance(); if(mc){ jclass c=env->GetObjectClass(mc);
    jfieldID lvl=env->GetFieldID(c,"level","Lnet/minecraft/client/multiplayer/ClientLevel;");
    if(lvl){ jobject w=env->GetObjectField(mc,lvl); if(w){ jclass wc=env->GetObjectClass(w);
    s_rain=env->GetFieldID(wc,"rainLevel","F"); if(env->ExceptionCheck()){env->ExceptionClear();s_rain=nullptr;}
    s_thunder=env->GetFieldID(wc,"thunderLevel","F"); if(env->ExceptionCheck()){env->ExceptionClear();s_thunder=nullptr;}
    env->DeleteLocalRef(wc); env->DeleteLocalRef(w); } }
    env->DeleteLocalRef(c); env->DeleteLocalRef(mc); } init=true; }
    jobject mc=CMinecraft::getInstance(); if(!mc)return;
    static jfieldID s_lvlF=nullptr; if(!s_lvlF) s_lvlF=env->GetFieldID(env->GetObjectClass(mc),"level","Lnet/minecraft/client/multiplayer/ClientLevel;");
    if(s_lvlF){ jobject w=env->GetObjectField(mc,s_lvlF); if(w){
    if(s_rain)env->SetFloatField(w,s_rain,0);
    if(s_thunder)env->SetFloatField(w,s_thunder,0);
    env->DeleteLocalRef(w); }}
    env->DeleteLocalRef(mc);
}
