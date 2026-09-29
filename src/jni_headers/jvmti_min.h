#pragma once
/*
 * Minimal JVMTI header — Standard HotSpot vtable layout.
 * JVMTI spec uses 1-based numbering: function N is at C array index N-1.
 *
 * Confirmed anchors:
 *   GetClassSignature = function 48 → C index 47 (probe-verified)
 *   GetLoadedClasses  = function 78 → C index 77
 *   Deallocate        = function 47 → C index 46
 */

#include "jni.h"

#define JVMTI_VERSION_1_0  0x30010000
#define JVMTI_VERSION_1_1  0x30010100
#define JVMTI_VERSION_1_2  0x30010200
#define JVMTI_ERROR_NONE   0
typedef jint jvmtiError;

struct jvmtiEnv_;
typedef jvmtiEnv_ jvmtiEnv;

/*
 * Standard JVMTI 1.2+ function table layout (C indices, 0-based):
 *   46: Deallocate
 *   47: GetClassSignature
 *   48: GetClassStatus
 *   49: GetSourceFileName
 *   50: GetClassModifiers
 *   51: GetClassMethods
 *   52: GetClassFields
 *   53-58: (other class/object functions)
 *   59: GetFieldName
 *   60: GetFieldDeclaringClass
 *   61: GetFieldModifiers
 *   62: IsFieldSynthetic
 *   63: GetMethodName
 *   64: GetMethodDeclaringClass
 *   65: GetMethodModifiers
 *   ...
 *   77: GetLoadedClasses
 */

struct jvmtiInterface_1_ {
    void* reserved1[46];                                                // 0-45
    jint(JNICALL *Deallocate)(jvmtiEnv*, unsigned char*);               // 46
    jint(JNICALL *GetClassSignature)(jvmtiEnv*, jclass, char**, char**); // 47
    void* reserved2[3];                                                 // 48-50
    jint(JNICALL *GetClassMethods)(jvmtiEnv*, jclass, jint*, jmethodID**); // 51
    jint(JNICALL *GetClassFields)(jvmtiEnv*, jclass, jint*, jfieldID**);  // 52
    void* reserved3[6];                                                 // 53-58
    jint(JNICALL *GetFieldName)(jvmtiEnv*, jclass, jfieldID, char**, char**, char**); // 59
    void* reserved4[1];                                                 // 60
    jint(JNICALL *GetFieldModifiers)(jvmtiEnv*, jclass, jfieldID, jint*); // 61
    void* reserved5[1];                                                 // 62
    jint(JNICALL *GetMethodName)(jvmtiEnv*, jmethodID, char**, char**, char**); // 63
    void* reserved6[1];                                                 // 64
    jint(JNICALL *GetMethodModifiers)(jvmtiEnv*, jmethodID, jint*);     // 65
    void* reserved7[11];                                                // 66-76
    jint(JNICALL *GetLoadedClasses)(jvmtiEnv*, jint*, jclass**);        // 77
    void* reserved8[100];                                               // 78+
};

struct jvmtiEnv_ {
    const jvmtiInterface_1_* functions;

    jint GetLoadedClasses(jint* count, jclass** classes) {
        return functions->GetLoadedClasses(this, count, classes);
    }
    jint GetClassSignature(jclass cls, char** sig, char** generic) {
        return functions->GetClassSignature(this, cls, sig, generic);
    }
    jint GetClassMethods(jclass cls, jint* count, jmethodID** methods) {
        return functions->GetClassMethods(this, cls, count, methods);
    }
    jint GetClassFields(jclass cls, jint* count, jfieldID** fields) {
        return functions->GetClassFields(this, cls, count, fields);
    }
    jint GetMethodName(jmethodID method, char** name, char** sig, char** generic) {
        return functions->GetMethodName(this, method, name, sig, generic);
    }
    jint GetMethodModifiers(jmethodID method, jint* mods) {
        return functions->GetMethodModifiers(this, method, mods);
    }
    jint GetFieldName(jclass cls, jfieldID field, char** name, char** sig, char** generic) {
        return functions->GetFieldName(this, cls, field, name, sig, generic);
    }
    jint GetFieldModifiers(jclass cls, jfieldID field, jint* mods) {
        return functions->GetFieldModifiers(this, cls, field, mods);
    }
    jint Deallocate(unsigned char* mem) {
        return functions->Deallocate(this, mem);
    }
};
