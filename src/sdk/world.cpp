#include "pch.h"
#include "core/strcrypt.h"
#include "sdk/world.h"

bool CWorld::initIDs() {
    s_levelClass = JvmWrapper::findClass(Mappings::ClientLevel_Class);
    s_listClass  = JvmWrapper::findClass(Mappings::List_Class);

    s_iterableClass = JvmWrapper::findClass("java/lang/Iterable");
    s_iteratorClass = JvmWrapper::findClass("java/util/Iterator");

    if (!s_levelClass || !s_listClass || !s_iterableClass || !s_iteratorClass) {
        printf(STR_C("[Draxo] ERROR: World/List/Iterable classes not found\n"));
        return false;
    }

    s_players  = JvmWrapper::getFieldID(s_levelClass, Mappings::Level_players, Mappings::Level_players_Sig);
    s_listSize = JvmWrapper::getMethodID(s_listClass, Mappings::List_size, Mappings::List_size_Sig);
    s_listGet  = JvmWrapper::getMethodID(s_listClass, Mappings::List_get, Mappings::List_get_Sig);

    s_entitiesForRendering = JvmWrapper::getMethodID(s_levelClass, Mappings::ClientLevel_entitiesForRendering, Mappings::ClientLevel_entitiesForRendering_Sig);
    s_iterator = JvmWrapper::getMethodID(s_iterableClass, "iterator", "()Ljava/util/Iterator;");
    s_hasNext  = JvmWrapper::getMethodID(s_iteratorClass, "hasNext", "()Z");
    s_next     = JvmWrapper::getMethodID(s_iteratorClass, "next", "()Ljava/lang/Object;");

    bool ok = s_players && s_entitiesForRendering && s_iterator && s_hasNext && s_next;
    printf(STR_C("[Draxo] World IDs initialized: %s\n"), ok ? "OK" : "PARTIAL");
    return ok;
}

std::vector<CEntity> CWorld::getPlayers(jobject worldObj) {
    std::vector<CEntity> result;
    if (!worldObj || !s_players || !s_listSize || !s_listGet) return result;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return result;

    jobject playerList = env->GetObjectField(worldObj, s_players);
    if (!playerList || JvmWrapper::checkException()) return result;

    jint count = env->CallIntMethod(playerList, s_listSize);
    if (JvmWrapper::checkException()) {
        env->DeleteLocalRef(playerList);
        return result;
    }

    result.reserve(count);
    for (jint i = 0; i < count; i++) {
        jobject entity = env->CallObjectMethod(playerList, s_listGet, i);
        if (entity && !JvmWrapper::checkException()) {
            result.emplace_back(entity);
            // Note: we do NOT delete local ref here — caller's PushLocalFrame handles it
        }
    }

    env->DeleteLocalRef(playerList);
    return result;
}

std::vector<CEntity> CWorld::getAllEntities(jobject worldObj) {
    std::vector<CEntity> result;
    if (!worldObj || !s_entitiesForRendering || !s_iterator || !s_hasNext || !s_next) return result;

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return result;

    jobject iterable = env->CallObjectMethod(worldObj, s_entitiesForRendering);
    if (!iterable || JvmWrapper::checkException()) return result;

    jobject iterator = env->CallObjectMethod(iterable, s_iterator);
    if (!iterator || JvmWrapper::checkException()) {
        env->DeleteLocalRef(iterable);
        return result;
    }

    while (env->CallBooleanMethod(iterator, s_hasNext)) {
        jobject entity = env->CallObjectMethod(iterator, s_next);
        if (entity && !JvmWrapper::checkException()) {
            result.emplace_back(entity);
            // Entity refs are LOCAL — they live until the caller's frame pops.
            // All callers (ESP/Tracers/KillAura/AimAssist) iterate entities
            // strictly inside a PushLocalFrame, so the refs are freed there.
        } else {
            JvmWrapper::checkException(); // Clear exception if any
        }
    }

    env->DeleteLocalRef(iterator);
    env->DeleteLocalRef(iterable);
    return result;
}
