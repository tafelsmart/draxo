#pragma once
#include "pch.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

/*
 * CEntity — Wraps a Java Entity object via JNI.
 * Holds a local ref (caller must manage lifetime within a frame).
 */
class CEntity {
public:
    explicit CEntity(jobject obj) : m_obj(obj) {}
    CEntity() : m_obj(nullptr) {}

    bool isValid() const { return m_obj != nullptr; }
    jobject getObject() const { return m_obj; }

    double getX() const;
    double getY() const;
    double getZ() const;

    float getYaw() const;
    float getPitch() const;
    void  setYaw(float yaw);
    void  setPitch(float pitch);

    // Motion structure
    struct Vec3 { double x, y, z; };

    bool  isInWater() const;
    void  setDeltaMovement(double x, double y, double z);
    Vec3  getDeltaMovement() const;

    bool isAlive() const;
    bool isLiving() const;
    bool isPlayer() const;
    bool isItem() const;
    bool isArmorStand() const;
    bool isMinecartChest() const;
    int  getId() const;

    float getHealth() const;
    float getMaxHealth() const;
    // LivingEntity.hurtTime — >0 waehrend des Knockback-Fensters nach einem Treffer
    int   getHurtTime() const;

    bool  isOnGround() const;
    void  setOnGround(bool onGround);
    bool  horizontalCollision() const;
    bool  isShiftKeyDown() const;
    bool  isJumping() const;
    void  setJumping(bool jumping);
    void  setDiscardFriction(bool discard);

    std::string getName() const;

    // Bounding box
    struct AABB {
        double minX, minY, minZ;
        double maxX, maxY, maxZ;
    };
    AABB getBoundingBox() const;
    // Vec3 getDeltaMovement() const; // Replaced by new signature
    // void setDeltaMovement(double x, double y, double z); // Replaced by new signature

    // Glowing effect
    void setGlowingTag(bool glowing);
    bool hasGlowingTag() const;

    // ── Cached IDs (init once) ──────────────────────────────────────
    static bool initIDs();

protected:
    jobject m_obj;

    // Cached method/field IDs
    static inline jclass   s_entityClass    = nullptr;
    static inline jclass   s_livingClass    = nullptr;
    static inline jclass   s_playerClass    = nullptr;
    static inline jclass   s_itemClass      = nullptr;
    static inline jclass   s_armorStandClass= nullptr;
    static inline jclass   s_minecartChestClass= nullptr;
    static inline jclass   s_vec3Class      = nullptr;
    static inline jclass   s_aabbClass      = nullptr;

    static inline jmethodID s_getX          = nullptr;
    static inline jmethodID s_getY          = nullptr;
    static inline jmethodID s_getZ          = nullptr;
    static inline jmethodID s_getYRot       = nullptr;
    static inline jmethodID s_getXRot       = nullptr;
    static inline jmethodID s_setYRot       = nullptr;
    static inline jmethodID s_setXRot       = nullptr;
    
    // Physics
    static inline jmethodID s_isInWater         = nullptr;
    static inline jmethodID s_getDeltaMovement  = nullptr;
    static inline jmethodID s_setDeltaMovement  = nullptr;

    // Attributes
    static inline jmethodID s_isAlive       = nullptr;
    static inline jmethodID s_getId         = nullptr;
    static inline jmethodID s_getBBox       = nullptr;
    static inline jmethodID s_getDeltaMov   = nullptr;
    static inline jmethodID s_setDeltaMov   = nullptr;
    static inline jmethodID s_getHealth     = nullptr;
    static inline jmethodID s_getMaxHealth  = nullptr;
    static inline jmethodID s_getScoreboardName = nullptr;
    static inline jmethodID s_getName       = nullptr;
    static inline jclass    s_componentClass = nullptr;
    static inline jmethodID s_componentGetString = nullptr;
    static inline jmethodID s_setSharedFlag = nullptr;
    static inline jmethodID s_getSharedFlag = nullptr;

    static inline jfieldID  s_vec3x         = nullptr;
    static inline jfieldID  s_vec3y         = nullptr;
    static inline jfieldID  s_vec3z         = nullptr;

    static inline jfieldID  s_aabbMinX      = nullptr;
    static inline jfieldID  s_aabbMinY      = nullptr;
    static inline jfieldID  s_aabbMinZ      = nullptr;
    static inline jfieldID  s_aabbMaxX      = nullptr;
    static inline jfieldID  s_aabbMaxY      = nullptr;
    static inline jfieldID  s_aabbMaxZ      = nullptr;

    static inline jfieldID  s_onGround      = nullptr;
    static inline jfieldID  s_hurtTime      = nullptr;
    static inline jfieldID  s_horizontalCollision = nullptr;
    static inline jfieldID  s_jumping       = nullptr;
    static inline jfieldID  s_discardFriction = nullptr;
    static inline jmethodID s_isShiftKeyDown = nullptr;
};
