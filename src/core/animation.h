#pragma once
#include "pch.h"
#include <cmath>

/*
 * Animation — Complete easing/animation system (31 curves).
 * Every UI animation uses this — open/close transitions, hover states,
 * notification slides, toggle knobs, star buttons, etc.
 */

// ── Easing functions (31 curves) ────────────────────────────────────
namespace Ease {
    inline float linear    (float t) { return t; }

    // ── Quad ────────────────────────────────────────────────────────
    inline float inQuad    (float t) { return t*t; }
    inline float outQuad   (float t) { return t*(2.0f-t); }
    inline float inOutQuad (float t) { return t<0.5f ? 2*t*t : -1+(4-2*t)*t; }

    // ── Cubic ───────────────────────────────────────────────────────
    inline float inCubic   (float t) { return t*t*t; }
    inline float outCubic  (float t) { return (--t)*t*t+1; }
    inline float inOutCubic(float t) { return t<0.5f ? 4*t*t*t : (t-1)*(2*t-2)*(2*t-2)+1; }

    // ── Quart ───────────────────────────────────────────────────────
    inline float inQuart   (float t) { return t*t*t*t; }
    inline float outQuart  (float t) { return 1-(--t)*t*t*t; }
    inline float inOutQuart(float t) { return t<0.5f ? 8*t*t*t*t : 1-(-2*t+2)*(-2*t+2)*(-2*t+2)*(-2*t+2)/2; }

    // ── Quint ───────────────────────────────────────────────────────
    inline float inQuint   (float t) { return t*t*t*t*t; }
    inline float outQuint  (float t) { return 1+(--t)*t*t*t*t; }
    inline float inOutQuint(float t) { return t<0.5f ? 16*t*t*t*t*t : 1-(-2*t+2)*(-2*t+2)*(-2*t+2)*(-2*t+2)*(-2*t+2)/2; }

    // ── Sine ────────────────────────────────────────────────────────
    inline float inSine    (float t) { return 1-cosf(t*1.5707963f); }
    inline float outSine   (float t) { return sinf(t*1.5707963f); }
    inline float inOutSine (float t) { return -(cosf(3.14159265f*t)-1)/2; }

    // ── Expo ────────────────────────────────────────────────────────
    inline float inExpo    (float t) { return t<=0?0:powf(2,10*t-10); }
    inline float outExpo   (float t) { return t>=1?1:1-powf(2,-10*t); }
    inline float inOutExpo (float t) {
        return t<=0?0:t>=1?1:t<0.5f?powf(2,20*t-10)/2:(2-powf(2,-20*t+10))/2;
    }

    // ── Circ ────────────────────────────────────────────────────────
    inline float inCirc    (float t) { return 1-sqrtf(1-t*t); }
    inline float outCirc   (float t) { return sqrtf(1-(--t)*t); }
    inline float inOutCirc (float t) {
        return t<0.5f ? (1-sqrtf(1-4*t*t))/2 : (sqrtf(1-(-2*t+2)*(-2*t+2))+1)/2;
    }

    // ── Back (overshoot) ────────────────────────────────────────────
    inline float inBack    (float t) { float s=1.70158f; return t*t*((s+1)*t-s); }
    inline float outBack   (float t) { float s=1.70158f; return (--t)*t*((s+1)*t+s)+1; }
    inline float inOutBack (float t) {
        float s=1.70158f*1.525f;
        return t<0.5f ? (t*=2,t*t*((s+1)*t-s)/2) : (t=2*t-2,(t*t*((s+1)*t+s)+2)/2);
    }

    // ── Elastic ─────────────────────────────────────────────────────
    inline float inElastic (float t) {
        if(t<=0||t>=1)return t; return -powf(2,10*t-10)*sinf((t*10-10.75f)*2.0943951f);
    }
    inline float outElastic(float t) {
        if(t<=0||t>=1)return t; return powf(2,-10*t)*sinf((t*10-0.75f)*2.0943951f)+1;
    }
    inline float inOutElastic(float t) {
        if(t<=0||t>=1)return t;
        return t<0.5f ? -(powf(2,20*t-10)*sinf((20*t-11.125f)*1.3962634f))/2
                       : (powf(2,-20*t+10)*sinf((20*t-11.125f)*1.3962634f))/2+1;
    }

    // ── Bounce ──────────────────────────────────────────────────────
    inline float outBounce(float t) {
        if(t<1/2.75f)      return 7.5625f*t*t;
        if(t<2/2.75f)      { t-=1.5f/2.75f;  return 7.5625f*t*t+0.75f; }
        if(t<2.5f/2.75f)   { t-=2.25f/2.75f; return 7.5625f*t*t+0.9375f; }
        t-=2.625f/2.75f;   return 7.5625f*t*t+0.984375f;
    }
    inline float inBounce (float t) { return 1-outBounce(1-t); }
    inline float inOutBounce(float t) {
        return t<0.5f ? (1-outBounce(1-2*t))/2 : (1+outBounce(2*t-1))/2;
    }
} // namespace Ease

// ── Animated value types ────────────────────────────────────────────

struct AnimatedFloat {
    float  value       = 0.0f;
    float  startValue  = 0.0f;
    float  target      = 0.0f;
    float  duration    = 0.3f;     // seconds
    float  elapsed     = 0.0f;     // seconds elapsed
    int    delayFrames = 0;
    int    delayDone   = 0;
    float (*easing)(float) = Ease::outQuad;

    void setTarget(float t, float dur = 0.18f, int delay = 0, float (*ease)(float) = Ease::outQuad) {
        if (fabs(t - target) < 0.0001f) return;
        startValue = value;
        target     = t;
        duration   = dur;
        elapsed    = 0.0f;
        delayFrames= delay;
        delayDone  = 0;
        easing     = ease;
    }
    void snap(float v)   { value = startValue = target = v; elapsed = 999.0f; delayDone = delayFrames+1; }

    float tick(float dt = 1.0f/60.0f) {
        if (delayDone < delayFrames) { delayDone++; return value; }
        if (elapsed >= duration)     { value = target; return value; }
        elapsed += dt;
        float t = (duration > 0.001f) ? (elapsed / duration) : 1.0f;
        if (t > 1.0f) t = 1.0f;
        float eased = easing(t);
        value = startValue + (target - startValue) * eased;
        return value;
    }
    operator float() const { return value; }
    bool done() const { return elapsed >= duration && delayDone >= delayFrames; }
};

struct AnimatedColor {
    ImVec4 value  = ImVec4(1,1,1,1);
    ImVec4 target = ImVec4(1,1,1,1);
    float  speed  = 0.10f;
    void setTarget(const ImVec4& t, float spd = 0.10f) { target = t; speed = spd; }
    void snap(const ImVec4& v) { value = target = v; }
    ImVec4 tick() {
        value.x += (target.x - value.x) * speed;
        value.y += (target.y - value.y) * speed;
        value.z += (target.z - value.z) * speed;
        value.w += (target.w - value.w) * speed;
        return value;
    }
    operator ImVec4() const { return value; }
};

struct AnimatedVec2 {
    ImVec2 value  = ImVec2(0,0);
    ImVec2 target = ImVec2(0,0);
    float  speed  = 0.10f;
    void setTarget(const ImVec2& t, float spd = 0.10f) { target = t; speed = spd; }
    void snap(const ImVec2& v) { value = target = v; }
    ImVec2 tick() {
        value.x += (target.x - value.x) * speed;
        value.y += (target.y - value.y) * speed;
        return value;
    }
    operator ImVec2() const { return value; }
};

// ── Convenience / legacy compat ─────────────────────────────────────
namespace Anim {
    inline float lerp(float a, float b, float t) { return a+(b-a)*t; }

    inline float smooth(float& stored, float target, float speed = 0.08f) {
        stored += (target - stored) * speed; return stored;
    }
    inline float fade(bool visible, float& stored, float speed = 0.12f) {
        stored += (visible ? speed : -speed);
        if (stored < 0.0f) stored = 0.0f; if (stored > 1.0f) stored = 1.0f;
        return stored;
    }
    inline float pop(float& stored, bool active, float speed = 0.2f) {
        float target = active ? 1.08f : 1.0f;
        stored += (target - stored) * speed; return stored;
    }
    inline float slide(float& stored, float target, float speed = 0.1f) {
        stored += (target - stored) * speed;
        if (fabsf(stored - target) < 0.01f) stored = target;
        return stored;
    }
    inline ImU32 lerpColor(ImU32 a, ImU32 b, float t) {
        t = t<0?0:(t>1?1:t);
        return IM_COL32(
            (int)(((a>>0)&0xFF)+(((b>>0)&0xFF)-((a>>0)&0xFF))*t),
            (int)(((a>>8)&0xFF)+(((b>>8)&0xFF)-((a>>8)&0xFF))*t),
            (int)(((a>>16)&0xFF)+(((b>>16)&0xFF)-((a>>16)&0xFF))*t),
            (int)(((a>>24)&0xFF)+(((b>>24)&0xFF)-((a>>24)&0xFF))*t));
    }
} // namespace Anim
