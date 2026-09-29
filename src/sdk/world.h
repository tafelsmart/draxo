#pragma once
#include "pch.h"
#include "sdk/entity.h"

/*
 * CWorld — Wraps ClientLevel to iterate player entities.
 */
class CWorld {
public:
    static bool initIDs();

    // Get all players in the world (local refs — call within PushLocalFrame)
    static std::vector<CEntity> getPlayers(jobject worldObj);

    // Get all renderable entities (includes players, animals, mobs, etc.)
    static std::vector<CEntity> getAllEntities(jobject worldObj);

private:
    static inline jclass    s_levelClass   = nullptr;
    
    // For players list
    static inline jclass    s_listClass    = nullptr;
    static inline jfieldID  s_players      = nullptr;
    static inline jmethodID s_listSize     = nullptr;
    static inline jmethodID s_listGet      = nullptr;

    // For all entities iterable
    static inline jmethodID s_entitiesForRendering = nullptr;
    static inline jclass    s_iterableClass = nullptr;
    static inline jmethodID s_iterator      = nullptr;
    static inline jclass    s_iteratorClass = nullptr;
    static inline jmethodID s_hasNext       = nullptr;
    static inline jmethodID s_next          = nullptr;
};
