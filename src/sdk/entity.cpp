#include "pch.h"
#include "core/strcrypt.h"
#include "sdk/entity.h"

bool CEntity::initIDs() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return false;

    // ── Find classes ────────────────────────────────────────────────
    s_entityClass  = JvmWrapper::findClass(Mappings::Entity_Class);
    s_livingClass  = JvmWrapper::findClass(Mappings::LivingEntity_Class);
    s_playerClass  = JvmWrapper::findClass(Mappings::Player_Class);
    s_itemClass    = JvmWrapper::findClass(Mappings::ItemEntity_Class);
    s_armorStandClass = JvmWrapper::findClass(Mappings::ArmorStand_Class);
    // MinecartChest moved to vehicle/minecart package in 1.21.2+
    s_minecartChestClass = JvmWrapper::findClass(Mappings::MinecartChest_Class);
    if (!s_minecartChestClass) s_minecartChestClass = JvmWrapper::findClass(Mappings::MinecartChest_Class_New);
    s_vec3Class    = JvmWrapper::findClass(Mappings::Vec3_Class);
    s_aabbClass    = JvmWrapper::findClass(Mappings::AABB_Class);

    if (!s_entityClass || !s_livingClass || !s_vec3Class || !s_aabbClass) {
        printf(STR_C("[Draxo] ERROR: Failed to find entity-related classes\n"));
        return false;
    }

    // ── DIAGNOSTIC: dump actual signatures ───────────────────────────
    // DISABLED: JvmWrapper::dumpClassInfo(s_entityClass, "Entity", 60, 20);
    // DISABLED: JvmWrapper::dumpClassInfo(s_livingClass, "LivingEntity", 20, 10);
    
    jclass clientLevelCls = JvmWrapper::findClass(Mappings::ClientLevel_Class);
    if (clientLevelCls) {
        // DISABLED: JvmWrapper::dumpClassInfo(clientLevelCls, "ClientLevel", 30, 20);
    }
    
    // DISABLED: JvmWrapper::dumpClassInfo(s_vec3Class, "Vec3", 10, 10);
    // DISABLED: JvmWrapper::dumpClassInfo(s_aabbClass, "AABB", 10, 10);

    // ── Entity methods ──────────────────────────────────────────────
    s_getX     = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getX, Mappings::Entity_getX_Sig);
    if (!s_getX) printf(STR_C("[Draxo] FAILED to resolve Entity::getX\n"));
    s_getY     = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getY, Mappings::Entity_getY_Sig);
    if (!s_getY) printf(STR_C("[Draxo] FAILED to resolve Entity::getY\n"));
    s_getZ     = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getZ, Mappings::Entity_getZ_Sig);
    if (!s_getZ) printf(STR_C("[Draxo] FAILED to resolve Entity::getZ\n"));
    
    s_getYRot  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getYRot, Mappings::Entity_getYRot_Sig);
    if (!s_getYRot) printf(STR_C("[Draxo] FAILED to resolve Entity::getYRot\n"));
    s_getXRot  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getXRot, Mappings::Entity_getXRot_Sig);
    if (!s_getXRot) printf(STR_C("[Draxo] FAILED to resolve Entity::getXRot\n"));

    s_setYRot  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_setYRot, Mappings::Entity_setYRot_Sig);
    if (!s_setYRot) printf(STR_C("[Draxo] FAILED to resolve Entity::setYRot\n"));
    s_setXRot  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_setXRot, Mappings::Entity_setXRot_Sig);
    if (!s_setXRot) printf(STR_C("[Draxo] FAILED to resolve Entity::setXRot\n"));

    s_isInWater = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_isInWater, Mappings::Entity_isInWater_Sig);
    if (!s_isInWater) printf(STR_C("[Draxo] FAILED to resolve Entity::isInWater\n"));
    
    s_isAlive  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_isAlive, Mappings::Entity_isAlive_Sig);
    if (!s_isAlive) printf(STR_C("[Draxo] FAILED to resolve Entity::isAlive\n"));
    s_getId    = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getId, Mappings::Entity_getId_Sig);
    if (!s_getId) printf(STR_C("[Draxo] FAILED to resolve Entity::getId\n"));
    
    s_getBBox  = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getBoundingBox, Mappings::Entity_getBoundingBox_Sig);
    if (!s_getBBox) printf(STR_C("[Draxo] FAILED to resolve Entity::getBoundingBox\n"));
    
    s_getScoreboardName = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getScoreboardName, Mappings::Entity_getScoreboardName_Sig);
    if (!s_getScoreboardName) printf(STR_C("[Draxo] FAILED to resolve Entity::getScoreboardName\n"));

    s_getName = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getName, Mappings::Entity_getName_Sig);
    if (!s_getName) printf(STR_C("[Draxo] FAILED to resolve Entity::getName\n"));

    s_componentClass = JvmWrapper::findClass(Mappings::Component_Class);
    if (s_componentClass) {
        s_componentGetString = JvmWrapper::getMethodID(s_componentClass, Mappings::Component_getString, Mappings::Component_getString_Sig);
        if (!s_componentGetString) printf(STR_C("[Draxo] FAILED to resolve Component::getString\n"));
    } else {
        printf(STR_C("[Draxo] FAILED to find Component class\n"));
    }

    s_getDeltaMov = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getDeltaMovement, Mappings::Entity_getDeltaMovement_Sig);
    if (!s_getDeltaMov) printf(STR_C("[Draxo] FAILED to resolve Entity::getDeltaMovement\n"));
    s_setDeltaMov = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_setDeltaMovement, Mappings::Entity_setDeltaMovement_Sig);
    if (!s_setDeltaMov) printf(STR_C("[Draxo] FAILED to resolve Entity::setDeltaMovement\n"));
    
    s_setSharedFlag = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_setSharedFlag, Mappings::Entity_setSharedFlag_Sig);
    if (!s_setSharedFlag) printf(STR_C("[Draxo] FAILED to resolve Entity::setSharedFlag\n"));
    s_getSharedFlag = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_getSharedFlag, Mappings::Entity_getSharedFlag_Sig);
    if (!s_getSharedFlag) printf(STR_C("[Draxo] FAILED to resolve Entity::getSharedFlag\n"));

    // ── LivingEntity methods ────────────────────────────────────────
    s_getHealth    = JvmWrapper::getMethodID(s_livingClass, Mappings::LivingEntity_getHealth, Mappings::LivingEntity_getHealth_Sig);
    if (!s_getHealth) printf(STR_C("[Draxo] FAILED to resolve LivingEntity::getHealth\n"));
    s_getMaxHealth = JvmWrapper::getMethodID(s_livingClass, Mappings::LivingEntity_getMaxHealth, Mappings::LivingEntity_getMaxHealth_Sig);
    if (!s_getMaxHealth) printf(STR_C("[Draxo] FAILED to resolve LivingEntity::getMaxHealth\n"));

    // ── Vec3 fields ─────────────────────────────────────────────────
    s_vec3x = JvmWrapper::getFieldID(s_vec3Class, Mappings::Vec3_x, Mappings::Vec3_D_Sig);
    s_vec3y = JvmWrapper::getFieldID(s_vec3Class, Mappings::Vec3_y, Mappings::Vec3_D_Sig);
    s_vec3z = JvmWrapper::getFieldID(s_vec3Class, Mappings::Vec3_z, Mappings::Vec3_D_Sig);

    // ── AABB fields ─────────────────────────────────────────────────
    s_aabbMinX = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_minX, Mappings::AABB_D_Sig);
    s_aabbMinY = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_minY, Mappings::AABB_D_Sig);
    s_aabbMinZ = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_minZ, Mappings::AABB_D_Sig);
    s_aabbMaxX = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_maxX, Mappings::AABB_D_Sig);
    s_aabbMaxY = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_maxY, Mappings::AABB_D_Sig);
    s_aabbMaxZ = JvmWrapper::getFieldID(s_aabbClass, Mappings::AABB_maxZ, Mappings::AABB_D_Sig);

    s_onGround = JvmWrapper::getFieldID(s_entityClass, Mappings::Entity_onGround, Mappings::Entity_onGround_Sig);
    s_hurtTime = JvmWrapper::getFieldID(s_livingClass, Mappings::LivingEntity_hurtTime, Mappings::LivingEntity_hurtTime_Sig);
    s_horizontalCollision = JvmWrapper::getFieldID(s_entityClass, Mappings::Entity_horizontalCollision, Mappings::Entity_horizontalCollision_Sig);
    s_jumping = JvmWrapper::getFieldID(s_livingClass, Mappings::LivingEntity_jumping, Mappings::LivingEntity_jumping_Sig);
    s_discardFriction = JvmWrapper::getFieldID(s_livingClass, Mappings::LivingEntity_discardFriction, Mappings::LivingEntity_discardFriction_Sig);
    s_isShiftKeyDown = JvmWrapper::getMethodID(s_entityClass, Mappings::Entity_isShiftKeyDown, Mappings::Entity_isShiftKeyDown_Sig);

    bool ok = s_getX && s_getY && s_getZ && s_getYRot && s_getXRot && s_setYRot && s_setXRot && s_isAlive && s_getId && s_getBBox && s_getDeltaMov && s_setDeltaMov && s_setSharedFlag && s_getSharedFlag && s_isInWater && s_onGround && s_horizontalCollision && s_jumping && s_discardFriction && s_isShiftKeyDown;
    
    // LivingEntity
    ok = ok && s_getHealth && s_getMaxHealth;

    // Vec3/AABB
    ok = ok && s_vec3x && s_vec3y && s_vec3z;
    ok = ok && s_aabbMinX && s_aabbMinY && s_aabbMinZ && s_aabbMaxX && s_aabbMaxY && s_aabbMaxZ;

    printf(STR_C("[Draxo] Entity IDs initialized: %s\n"), ok ? "OK" : "PARTIAL");
    if (!ok) {
        printf(STR_C("[Draxo]   FAILED: Some required Entity/LivingEntity/Vec3/AABB IDs were NULL\n"));
    }
    return ok;
}

double CEntity::getX() const {
    if (!m_obj || !s_getX) return 0.0;
    return JvmWrapper::getEnv()->CallDoubleMethod(m_obj, s_getX);
}

double CEntity::getY() const {
    if (!m_obj || !s_getY) return 0.0;
    return JvmWrapper::getEnv()->CallDoubleMethod(m_obj, s_getY);
}

double CEntity::getZ() const {
    if (!m_obj || !s_getZ) return 0.0;
    return JvmWrapper::getEnv()->CallDoubleMethod(m_obj, s_getZ);
}

float CEntity::getYaw() const {
    if (!m_obj || !s_getYRot) return 0.0f;
    return JvmWrapper::getEnv()->CallFloatMethod(m_obj, s_getYRot);
}

float CEntity::getPitch() const {
    if (!m_obj || !s_getXRot) return 0.0f;
    return JvmWrapper::getEnv()->CallFloatMethod(m_obj, s_getXRot);
}

void CEntity::setYaw(float yaw) {
    if (!m_obj || !s_setYRot) return;
    JvmWrapper::getEnv()->CallVoidMethod(m_obj, s_setYRot, yaw);
}

void CEntity::setPitch(float pitch) {
    if (!m_obj || !s_setXRot) return;
    JvmWrapper::getEnv()->CallVoidMethod(m_obj, s_setXRot, pitch);
}

bool CEntity::isAlive() const {
    if (!m_obj || !s_isAlive) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->CallBooleanMethod(m_obj, s_isAlive) == JNI_TRUE : false;
}

bool CEntity::isLiving() const {
    if (!m_obj || !s_livingClass) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->IsInstanceOf(m_obj, s_livingClass) == JNI_TRUE : false;
}

bool CEntity::isPlayer() const {
    if (!m_obj || !s_playerClass) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->IsInstanceOf(m_obj, s_playerClass) == JNI_TRUE : false;
}

bool CEntity::isItem() const {
    if (!m_obj || !s_itemClass) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->IsInstanceOf(m_obj, s_itemClass) == JNI_TRUE : false;
}

bool CEntity::isArmorStand() const {
    if (!m_obj || !s_armorStandClass) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->IsInstanceOf(m_obj, s_armorStandClass) == JNI_TRUE : false;
}

bool CEntity::isMinecartChest() const {
    if (!m_obj || !s_minecartChestClass) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    return env ? env->IsInstanceOf(m_obj, s_minecartChestClass) == JNI_TRUE : false;
}

int CEntity::getId() const {
    if (!m_obj || !s_getId) return -1;
    return JvmWrapper::getEnv()->CallIntMethod(m_obj, s_getId);
}

float CEntity::getHealth() const {
    if (!m_obj || !s_getHealth) return 0.0f;
    JNIEnv* env = JvmWrapper::getEnv();
    // LivingEntity check
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return 0.0f;
    return env->CallFloatMethod(m_obj, s_getHealth);
}

float CEntity::getMaxHealth() const {
    if (!m_obj || !s_getMaxHealth) return 0.0f;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return 0.0f;
    return env->CallFloatMethod(m_obj, s_getMaxHealth);
}

int CEntity::getHurtTime() const {
    if (!m_obj || !s_hurtTime) return 0;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return 0;
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return 0;
    int v = env->GetIntField(m_obj, s_hurtTime);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return v;
}

bool CEntity::isOnGround() const {
    if (!m_obj || !s_onGround) return false;
    return JvmWrapper::getEnv()->GetBooleanField(m_obj, s_onGround) == JNI_TRUE;
}

void CEntity::setOnGround(bool onGround) {
    if (!m_obj || !s_onGround) return;
    JvmWrapper::getEnv()->SetBooleanField(m_obj, s_onGround, onGround ? JNI_TRUE : JNI_FALSE);
}

bool CEntity::horizontalCollision() const {
    if (!m_obj || !s_horizontalCollision) return false;
    return JvmWrapper::getEnv()->GetBooleanField(m_obj, s_horizontalCollision) == JNI_TRUE;
}

bool CEntity::isShiftKeyDown() const {
    if (!m_obj || !s_isShiftKeyDown) return false;
    return JvmWrapper::getEnv()->CallBooleanMethod(m_obj, s_isShiftKeyDown) == JNI_TRUE;
}

bool CEntity::isJumping() const {
    if (!m_obj || !s_jumping) return false;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return false;
    return env->GetBooleanField(m_obj, s_jumping) == JNI_TRUE;
}

void CEntity::setJumping(bool jumping) {
    if (!m_obj || !s_jumping) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return;
    env->SetBooleanField(m_obj, s_jumping, jumping ? JNI_TRUE : JNI_FALSE);
}

void CEntity::setDiscardFriction(bool discard) {
    if (!m_obj || !s_discardFriction) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env->IsInstanceOf(m_obj, s_livingClass)) return;
    env->SetBooleanField(m_obj, s_discardFriction, discard ? JNI_TRUE : JNI_FALSE);
}

std::string CEntity::getName() const {
    if (!m_obj || (!s_getName && !s_getScoreboardName)) return "Unknown";

    JNIEnv* env = JvmWrapper::getEnv();
    std::string result = "";

    // Try Component.getString() first
    if (s_getName && s_componentGetString) {
        jobject component = env->CallObjectMethod(m_obj, s_getName);
        if (component && !JvmWrapper::checkException()) {
            jstring jname = static_cast<jstring>(env->CallObjectMethod(component, s_componentGetString));
            if (jname && !JvmWrapper::checkException()) {
                result = JvmWrapper::jstringToString(jname);
                env->DeleteLocalRef(jname);
            }
            env->DeleteLocalRef(component);
        } else {
            JvmWrapper::checkException();
        }
    }

    // Fallback to getScoreboardName if Component fails or is empty
    if (result.empty() && s_getScoreboardName) {
        jstring jname = static_cast<jstring>(env->CallObjectMethod(m_obj, s_getScoreboardName));
        if (jname && !JvmWrapper::checkException()) {
            result = JvmWrapper::jstringToString(jname);
            env->DeleteLocalRef(jname);
        } else {
            JvmWrapper::checkException();
        }
    }

    if (result.empty()) return "Unknown";

    // Strip Minecraft formatting codes (e.g. §c)
    // In Modified UTF-8 (from GetStringUTFChars), § is encoded as 0xC2 0xA7
    std::string stripped;
    for (size_t i = 0; i < result.length(); ++i) {
        if (i + 2 < result.length() && 
            (unsigned char)result[i] == 0xC2 && 
            (unsigned char)result[i+1] == 0xA7) {
            i += 2; // skip the 2-byte § and the 1-byte format code that follows it
        } else {
            stripped += result[i];
        }
    }

    // Sometimes servers just use standard ASCII & for colors in raw strings if not parsed
    std::string finalStr;
    for (size_t i = 0; i < stripped.length(); ++i) {
        if (i + 1 < stripped.length() && stripped[i] == '&' && 
            (std::string("0123456789abcdefklmnor").find(stripped[i+1]) != std::string::npos)) {
            i += 1; // skip the & and the format code
        } else {
            finalStr += stripped[i];
        }
    }

    if (finalStr.empty()) return "Unknown";
    return finalStr;
}

CEntity::AABB CEntity::getBoundingBox() const {
    AABB result{};
    if (!m_obj || !s_getBBox) return result;

    JNIEnv* env = JvmWrapper::getEnv();
    jobject bbox = env->CallObjectMethod(m_obj, s_getBBox);
    if (!bbox || JvmWrapper::checkException()) return result;

    if (s_aabbMinX) result.minX = env->GetDoubleField(bbox, s_aabbMinX);
    if (s_aabbMinY) result.minY = env->GetDoubleField(bbox, s_aabbMinY);
    if (s_aabbMinZ) result.minZ = env->GetDoubleField(bbox, s_aabbMinZ);
    if (s_aabbMaxX) result.maxX = env->GetDoubleField(bbox, s_aabbMaxX);
    if (s_aabbMaxY) result.maxY = env->GetDoubleField(bbox, s_aabbMaxY);
    if (s_aabbMaxZ) result.maxZ = env->GetDoubleField(bbox, s_aabbMaxZ);

    env->DeleteLocalRef(bbox);
    return result;
}

bool CEntity::isInWater() const {
    if (!m_obj || !s_isInWater) return false;
    return JvmWrapper::getEnv()->CallBooleanMethod(m_obj, s_isInWater) == JNI_TRUE;
}

void CEntity::setGlowingTag(bool glowing) {
    if (!m_obj || !s_setSharedFlag) return;
    // 6 is the index for FLAG_GLOWING in the Entity class
    JvmWrapper::getEnv()->CallVoidMethod(m_obj, s_setSharedFlag, 6, glowing ? JNI_TRUE : JNI_FALSE);
}

bool CEntity::hasGlowingTag() const {
    if (!m_obj || !s_getSharedFlag) return false;
    // 6 is the index for FLAG_GLOWING in the Entity class
    return JvmWrapper::getEnv()->CallBooleanMethod(m_obj, s_getSharedFlag, 6) == JNI_TRUE;
}

CEntity::Vec3 CEntity::getDeltaMovement() const {
    Vec3 result{};
    if (!m_obj || !s_getDeltaMov) return result;

    JNIEnv* env = JvmWrapper::getEnv();
    jobject vec = env->CallObjectMethod(m_obj, s_getDeltaMov);
    if (!vec || JvmWrapper::checkException()) return result;

    if (s_vec3x) result.x = env->GetDoubleField(vec, s_vec3x);
    if (s_vec3y) result.y = env->GetDoubleField(vec, s_vec3y);
    if (s_vec3z) result.z = env->GetDoubleField(vec, s_vec3z);

    env->DeleteLocalRef(vec);
    return result;
}

void CEntity::setDeltaMovement(double x, double y, double z) {
    if (!m_obj || !s_setDeltaMov || !s_vec3Class) return;

    JNIEnv* env = JvmWrapper::getEnv();

    jmethodID vec3Ctor = env->GetMethodID(s_vec3Class, Mappings::Vec3_Init, Mappings::Vec3_Init_Sig);
    if (!vec3Ctor || JvmWrapper::checkException()) return;

    jobject newVec = env->NewObject(s_vec3Class, vec3Ctor, x, y, z);
    if (!newVec || JvmWrapper::checkException()) return;

    env->CallVoidMethod(m_obj, s_setDeltaMov, newVec);
    JvmWrapper::checkException();
    env->DeleteLocalRef(newVec);
}
