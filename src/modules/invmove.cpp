#include "pch.h"
#include "core/strcrypt.h"
#include "modules/invmove.h"
#include "sdk/minecraft.h"
#include "sdk/entity.h"
#include <cmath>

InvMove::InvMove() : Module("InvMove", ModuleCategory::MOVEMENT, 0, "Lets you walk around while your inventory is open") {
    defineFloat("speed", "Speed", 0.3f, 0.1f, 1.0f, "%.1f");
    addSearchTag("inventory");
    addSearchTag("walk");
}
void InvMove::onUpdate(JNIEnv* env) {
    if (!m_enabled) return;
    jobject p = CMinecraft::getPlayer();
    if (!p) return;
    CEntity pl(p);
    bool fwd=(GetAsyncKeyState('W')&0x8000)!=0, back=(GetAsyncKeyState('S')&0x8000)!=0;
    bool left=(GetAsyncKeyState('A')&0x8000)!=0, right=(GetAsyncKeyState('D')&0x8000)!=0;
    if (!fwd&&!back&&!left&&!right) { env->DeleteLocalRef(p); return; }
    float yaw=pl.getYaw(); double yr=yaw*3.14159265/180.0;
    double sy=sin(yr),cy=cos(yr),mx=0,mz=0;
    if(fwd){mx-=sy;mz-=cy;} if(back){mx+=sy;mz+=cy;}
    if(left){mx-=cy;mz+=sy;} if(right){mx+=cy;mz-=sy;}
    double len=sqrt(mx*mx+mz*mz); if(len>0){mx/=len;mz/=len;}
    pl.setDiscardFriction(true);
    pl.setDeltaMovement(mx*m_floatSettings["speed"], pl.getDeltaMovement().y, mz*m_floatSettings["speed"]);
    env->DeleteLocalRef(p);
}
