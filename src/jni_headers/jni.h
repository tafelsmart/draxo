/*
 * Minimal JNI header — sufficient for DLL injection use.
 * Based on Oracle's jni.h, stripped to essentials.
 */
#pragma once

#include <cstdint>
#include <cstdarg>

#ifdef _WIN32
#define JNICALL __stdcall
#define JNIIMPORT __declspec(dllimport)
#define JNIEXPORT __declspec(dllexport)
#else
#define JNICALL
#define JNIIMPORT
#define JNIEXPORT
#endif

typedef uint8_t  jboolean;
typedef int8_t   jbyte;
typedef uint16_t jchar;
typedef int16_t  jshort;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;
typedef jint     jsize;

#define JNI_FALSE 0
#define JNI_TRUE  1

#define JNI_OK        0
#define JNI_ERR       (-1)
#define JNI_EDETACHED (-2)
#define JNI_EVERSION  (-3)

#define JNI_VERSION_1_6 0x00010006
#define JNI_VERSION_1_8 0x00010008

struct _jobject;
typedef _jobject* jobject;
typedef jobject   jclass;
typedef jobject   jstring;
typedef jobject   jarray;
typedef jobject   jthrowable;
typedef jobject   jweak;

typedef jarray    jobjectArray;
typedef jarray    jbooleanArray;
typedef jarray    jbyteArray;
typedef jarray    jcharArray;
typedef jarray    jshortArray;
typedef jarray    jintArray;
typedef jarray    jlongArray;
typedef jarray    jfloatArray;
typedef jarray    jdoubleArray;

struct _jmethodID;
typedef _jmethodID* jmethodID;
struct _jfieldID;
typedef _jfieldID* jfieldID;

union jvalue {
    jboolean z;
    jbyte    b;
    jchar    c;
    jshort   s;
    jint     i;
    jlong    j;
    jfloat   f;
    jdouble  d;
    jobject  l;
};

struct JNINativeMethod {
    const char* name;
    const char* signature;
    void*       fnPtr;
};

struct JavaVMOption {
    char* optionString;
    void* extraInfo;
};

struct JavaVMInitArgs {
    jint         version;
    jint         nOptions;
    JavaVMOption* options;
    jboolean     ignoreUnrecognized;
};

struct JavaVMAttachArgs {
    jint        version;
    const char* name;
    jobject     group;
};

// Forward declarations
struct JNIEnv_;
struct JavaVM_;
typedef JNIEnv_ JNIEnv;
typedef JavaVM_ JavaVM;

// ── JNI Native Interface (function table) ───────────────────────────────
struct JNINativeInterface_ {
    void* reserved0;
    void* reserved1;
    void* reserved2;
    void* reserved3;
    jint        (JNICALL *GetVersion)(JNIEnv*);                                                  // 4
    jclass      (JNICALL *DefineClass)(JNIEnv*, const char*, jobject, const jbyte*, jsize);       // 5
    jclass      (JNICALL *FindClass)(JNIEnv*, const char*);                                      // 6
    jmethodID   (JNICALL *FromReflectedMethod)(JNIEnv*, jobject);                                // 7
    jfieldID    (JNICALL *FromReflectedField)(JNIEnv*, jobject);                                 // 8
    jobject     (JNICALL *ToReflectedMethod)(JNIEnv*, jclass, jmethodID, jboolean);              // 9
    jclass      (JNICALL *GetSuperclass)(JNIEnv*, jclass);                                       // 10
    jboolean    (JNICALL *IsAssignableFrom)(JNIEnv*, jclass, jclass);                            // 11
    jobject     (JNICALL *ToReflectedField)(JNIEnv*, jclass, jfieldID, jboolean);                // 12
    jint        (JNICALL *Throw)(JNIEnv*, jthrowable);                                           // 13
    jint        (JNICALL *ThrowNew)(JNIEnv*, jclass, const char*);                               // 14
    jthrowable  (JNICALL *ExceptionOccurred)(JNIEnv*);                                           // 15
    void        (JNICALL *ExceptionDescribe)(JNIEnv*);                                           // 16
    void        (JNICALL *ExceptionClear)(JNIEnv*);                                              // 17
    void        (JNICALL *FatalError)(JNIEnv*, const char*);                                     // 18
    jint        (JNICALL *PushLocalFrame)(JNIEnv*, jint);                                        // 19
    jobject     (JNICALL *PopLocalFrame)(JNIEnv*, jobject);                                      // 20
    jobject     (JNICALL *NewGlobalRef)(JNIEnv*, jobject);                                       // 21
    void        (JNICALL *DeleteGlobalRef)(JNIEnv*, jobject);                                    // 22
    void        (JNICALL *DeleteLocalRef)(JNIEnv*, jobject);                                     // 23
    jboolean    (JNICALL *IsSameObject)(JNIEnv*, jobject, jobject);                              // 24
    jobject     (JNICALL *NewLocalRef)(JNIEnv*, jobject);                                        // 25
    jint        (JNICALL *EnsureLocalCapacity)(JNIEnv*, jint);                                   // 26
    jobject     (JNICALL *AllocObject)(JNIEnv*, jclass);                                         // 27
    jobject     (JNICALL *NewObject)(JNIEnv*, jclass, jmethodID, ...);                           // 28
    jobject     (JNICALL *NewObjectV)(JNIEnv*, jclass, jmethodID, va_list);                      // 29
    jobject     (JNICALL *NewObjectA)(JNIEnv*, jclass, jmethodID, const jvalue*);                // 30
    jclass      (JNICALL *GetObjectClass)(JNIEnv*, jobject);                                     // 31
    jboolean    (JNICALL *IsInstanceOf)(JNIEnv*, jobject, jclass);                               // 32
    jmethodID   (JNICALL *GetMethodID)(JNIEnv*, jclass, const char*, const char*);               // 33
    jobject     (JNICALL *CallObjectMethod)(JNIEnv*, jobject, jmethodID, ...);                   // 34
    jobject     (JNICALL *CallObjectMethodV)(JNIEnv*, jobject, jmethodID, va_list);              // 35
    jobject     (JNICALL *CallObjectMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);        // 36
    jboolean    (JNICALL *CallBooleanMethod)(JNIEnv*, jobject, jmethodID, ...);                  // 37
    jboolean    (JNICALL *CallBooleanMethodV)(JNIEnv*, jobject, jmethodID, va_list);             // 38
    jboolean    (JNICALL *CallBooleanMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);       // 39
    jbyte       (JNICALL *CallByteMethod)(JNIEnv*, jobject, jmethodID, ...);                     // 40
    jbyte       (JNICALL *CallByteMethodV)(JNIEnv*, jobject, jmethodID, va_list);                // 41
    jbyte       (JNICALL *CallByteMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);          // 42
    jchar       (JNICALL *CallCharMethod)(JNIEnv*, jobject, jmethodID, ...);                     // 43
    jchar       (JNICALL *CallCharMethodV)(JNIEnv*, jobject, jmethodID, va_list);                // 44
    jchar       (JNICALL *CallCharMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);          // 45
    jshort      (JNICALL *CallShortMethod)(JNIEnv*, jobject, jmethodID, ...);                    // 46
    jshort      (JNICALL *CallShortMethodV)(JNIEnv*, jobject, jmethodID, va_list);               // 47
    jshort      (JNICALL *CallShortMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);         // 48
    jint        (JNICALL *CallIntMethod)(JNIEnv*, jobject, jmethodID, ...);                      // 49
    jint        (JNICALL *CallIntMethodV)(JNIEnv*, jobject, jmethodID, va_list);                 // 50
    jint        (JNICALL *CallIntMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);           // 51
    jlong       (JNICALL *CallLongMethod)(JNIEnv*, jobject, jmethodID, ...);                     // 52
    jlong       (JNICALL *CallLongMethodV)(JNIEnv*, jobject, jmethodID, va_list);                // 53
    jlong       (JNICALL *CallLongMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);          // 54
    jfloat      (JNICALL *CallFloatMethod)(JNIEnv*, jobject, jmethodID, ...);                    // 55
    jfloat      (JNICALL *CallFloatMethodV)(JNIEnv*, jobject, jmethodID, va_list);               // 56
    jfloat      (JNICALL *CallFloatMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);         // 57
    jdouble     (JNICALL *CallDoubleMethod)(JNIEnv*, jobject, jmethodID, ...);                   // 58
    jdouble     (JNICALL *CallDoubleMethodV)(JNIEnv*, jobject, jmethodID, va_list);              // 59
    jdouble     (JNICALL *CallDoubleMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);        // 60
    void        (JNICALL *CallVoidMethod)(JNIEnv*, jobject, jmethodID, ...);                     // 61
    void        (JNICALL *CallVoidMethodV)(JNIEnv*, jobject, jmethodID, va_list);                // 62
    void        (JNICALL *CallVoidMethodA)(JNIEnv*, jobject, jmethodID, const jvalue*);          // 63
    // NonVirtual variants 64-93
    jobject     (JNICALL *CallNonvirtualObjectMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jobject     (JNICALL *CallNonvirtualObjectMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jobject     (JNICALL *CallNonvirtualObjectMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jboolean    (JNICALL *CallNonvirtualBooleanMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jboolean    (JNICALL *CallNonvirtualBooleanMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jboolean    (JNICALL *CallNonvirtualBooleanMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jbyte       (JNICALL *CallNonvirtualByteMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jbyte       (JNICALL *CallNonvirtualByteMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jbyte       (JNICALL *CallNonvirtualByteMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jchar       (JNICALL *CallNonvirtualCharMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jchar       (JNICALL *CallNonvirtualCharMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jchar       (JNICALL *CallNonvirtualCharMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jshort      (JNICALL *CallNonvirtualShortMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jshort      (JNICALL *CallNonvirtualShortMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jshort      (JNICALL *CallNonvirtualShortMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jint        (JNICALL *CallNonvirtualIntMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jint        (JNICALL *CallNonvirtualIntMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jint        (JNICALL *CallNonvirtualIntMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jlong       (JNICALL *CallNonvirtualLongMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jlong       (JNICALL *CallNonvirtualLongMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jlong       (JNICALL *CallNonvirtualLongMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jfloat      (JNICALL *CallNonvirtualFloatMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jfloat      (JNICALL *CallNonvirtualFloatMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jfloat      (JNICALL *CallNonvirtualFloatMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    jdouble     (JNICALL *CallNonvirtualDoubleMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    jdouble     (JNICALL *CallNonvirtualDoubleMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    jdouble     (JNICALL *CallNonvirtualDoubleMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    void        (JNICALL *CallNonvirtualVoidMethod)(JNIEnv*, jobject, jclass, jmethodID, ...);
    void        (JNICALL *CallNonvirtualVoidMethodV)(JNIEnv*, jobject, jclass, jmethodID, va_list);
    void        (JNICALL *CallNonvirtualVoidMethodA)(JNIEnv*, jobject, jclass, jmethodID, const jvalue*);
    // Field access 94-113
    jfieldID    (JNICALL *GetFieldID)(JNIEnv*, jclass, const char*, const char*);                 // 94
    jobject     (JNICALL *GetObjectField)(JNIEnv*, jobject, jfieldID);                            // 95
    jboolean    (JNICALL *GetBooleanField)(JNIEnv*, jobject, jfieldID);                           // 96
    jbyte       (JNICALL *GetByteField)(JNIEnv*, jobject, jfieldID);                              // 97
    jchar       (JNICALL *GetCharField)(JNIEnv*, jobject, jfieldID);                              // 98
    jshort      (JNICALL *GetShortField)(JNIEnv*, jobject, jfieldID);                             // 99
    jint        (JNICALL *GetIntField)(JNIEnv*, jobject, jfieldID);                               // 100
    jlong       (JNICALL *GetLongField)(JNIEnv*, jobject, jfieldID);                              // 101
    jfloat      (JNICALL *GetFloatField)(JNIEnv*, jobject, jfieldID);                             // 102
    jdouble     (JNICALL *GetDoubleField)(JNIEnv*, jobject, jfieldID);                            // 103
    void        (JNICALL *SetObjectField)(JNIEnv*, jobject, jfieldID, jobject);                   // 104
    void        (JNICALL *SetBooleanField)(JNIEnv*, jobject, jfieldID, jboolean);                 // 105
    void        (JNICALL *SetByteField)(JNIEnv*, jobject, jfieldID, jbyte);                       // 106
    void        (JNICALL *SetCharField)(JNIEnv*, jobject, jfieldID, jchar);                       // 107
    void        (JNICALL *SetShortField)(JNIEnv*, jobject, jfieldID, jshort);                     // 108
    void        (JNICALL *SetIntField)(JNIEnv*, jobject, jfieldID, jint);                         // 109
    void        (JNICALL *SetLongField)(JNIEnv*, jobject, jfieldID, jlong);                       // 110
    void        (JNICALL *SetFloatField)(JNIEnv*, jobject, jfieldID, jfloat);                     // 111
    void        (JNICALL *SetDoubleField)(JNIEnv*, jobject, jfieldID, jdouble);                   // 112
    // Static methods 113-145
    jmethodID   (JNICALL *GetStaticMethodID)(JNIEnv*, jclass, const char*, const char*);          // 113
    jobject     (JNICALL *CallStaticObjectMethod)(JNIEnv*, jclass, jmethodID, ...);               // 114
    jobject     (JNICALL *CallStaticObjectMethodV)(JNIEnv*, jclass, jmethodID, va_list);          // 115
    jobject     (JNICALL *CallStaticObjectMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);    // 116
    jboolean    (JNICALL *CallStaticBooleanMethod)(JNIEnv*, jclass, jmethodID, ...);              // 117
    jboolean    (JNICALL *CallStaticBooleanMethodV)(JNIEnv*, jclass, jmethodID, va_list);         // 118
    jboolean    (JNICALL *CallStaticBooleanMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);   // 119
    jbyte       (JNICALL *CallStaticByteMethod)(JNIEnv*, jclass, jmethodID, ...);                 // 120
    jbyte       (JNICALL *CallStaticByteMethodV)(JNIEnv*, jclass, jmethodID, va_list);            // 121
    jbyte       (JNICALL *CallStaticByteMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);      // 122
    jchar       (JNICALL *CallStaticCharMethod)(JNIEnv*, jclass, jmethodID, ...);                 // 123
    jchar       (JNICALL *CallStaticCharMethodV)(JNIEnv*, jclass, jmethodID, va_list);            // 124
    jchar       (JNICALL *CallStaticCharMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);      // 125
    jshort      (JNICALL *CallStaticShortMethod)(JNIEnv*, jclass, jmethodID, ...);                // 126
    jshort      (JNICALL *CallStaticShortMethodV)(JNIEnv*, jclass, jmethodID, va_list);           // 127
    jshort      (JNICALL *CallStaticShortMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);     // 128
    jint        (JNICALL *CallStaticIntMethod)(JNIEnv*, jclass, jmethodID, ...);                  // 129
    jint        (JNICALL *CallStaticIntMethodV)(JNIEnv*, jclass, jmethodID, va_list);             // 130
    jint        (JNICALL *CallStaticIntMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);       // 131
    jlong       (JNICALL *CallStaticLongMethod)(JNIEnv*, jclass, jmethodID, ...);                 // 132
    jlong       (JNICALL *CallStaticLongMethodV)(JNIEnv*, jclass, jmethodID, va_list);            // 133
    jlong       (JNICALL *CallStaticLongMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);      // 134
    jfloat      (JNICALL *CallStaticFloatMethod)(JNIEnv*, jclass, jmethodID, ...);                // 135
    jfloat      (JNICALL *CallStaticFloatMethodV)(JNIEnv*, jclass, jmethodID, va_list);           // 136
    jfloat      (JNICALL *CallStaticFloatMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);     // 137
    jdouble     (JNICALL *CallStaticDoubleMethod)(JNIEnv*, jclass, jmethodID, ...);               // 138
    jdouble     (JNICALL *CallStaticDoubleMethodV)(JNIEnv*, jclass, jmethodID, va_list);          // 139
    jdouble     (JNICALL *CallStaticDoubleMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);    // 140
    void        (JNICALL *CallStaticVoidMethod)(JNIEnv*, jclass, jmethodID, ...);                 // 141
    void        (JNICALL *CallStaticVoidMethodV)(JNIEnv*, jclass, jmethodID, va_list);            // 142
    void        (JNICALL *CallStaticVoidMethodA)(JNIEnv*, jclass, jmethodID, const jvalue*);      // 143
    // Static fields 144-153
    jfieldID    (JNICALL *GetStaticFieldID)(JNIEnv*, jclass, const char*, const char*);            // 144
    jobject     (JNICALL *GetStaticObjectField)(JNIEnv*, jclass, jfieldID);                        // 145
    jboolean    (JNICALL *GetStaticBooleanField)(JNIEnv*, jclass, jfieldID);                       // 146
    jbyte       (JNICALL *GetStaticByteField)(JNIEnv*, jclass, jfieldID);                          // 147
    jchar       (JNICALL *GetStaticCharField)(JNIEnv*, jclass, jfieldID);                          // 148
    jshort      (JNICALL *GetStaticShortField)(JNIEnv*, jclass, jfieldID);                         // 149
    jint        (JNICALL *GetStaticIntField)(JNIEnv*, jclass, jfieldID);                           // 150
    jlong       (JNICALL *GetStaticLongField)(JNIEnv*, jclass, jfieldID);                          // 151
    jfloat      (JNICALL *GetStaticFloatField)(JNIEnv*, jclass, jfieldID);                         // 152
    jdouble     (JNICALL *GetStaticDoubleField)(JNIEnv*, jclass, jfieldID);                        // 153
    void        (JNICALL *SetStaticObjectField)(JNIEnv*, jclass, jfieldID, jobject);
    void        (JNICALL *SetStaticBooleanField)(JNIEnv*, jclass, jfieldID, jboolean);
    void        (JNICALL *SetStaticByteField)(JNIEnv*, jclass, jfieldID, jbyte);
    void        (JNICALL *SetStaticCharField)(JNIEnv*, jclass, jfieldID, jchar);
    void        (JNICALL *SetStaticShortField)(JNIEnv*, jclass, jfieldID, jshort);
    void        (JNICALL *SetStaticIntField)(JNIEnv*, jclass, jfieldID, jint);
    void        (JNICALL *SetStaticLongField)(JNIEnv*, jclass, jfieldID, jlong);
    void        (JNICALL *SetStaticFloatField)(JNIEnv*, jclass, jfieldID, jfloat);
    void        (JNICALL *SetStaticDoubleField)(JNIEnv*, jclass, jfieldID, jdouble);
    // String operations
    jstring     (JNICALL *NewString)(JNIEnv*, const jchar*, jsize);
    jsize       (JNICALL *GetStringLength)(JNIEnv*, jstring);
    const jchar*(JNICALL *GetStringChars)(JNIEnv*, jstring, jboolean*);
    void        (JNICALL *ReleaseStringChars)(JNIEnv*, jstring, const jchar*);
    jstring     (JNICALL *NewStringUTF)(JNIEnv*, const char*);
    jsize       (JNICALL *GetStringUTFLength)(JNIEnv*, jstring);
    const char* (JNICALL *GetStringUTFChars)(JNIEnv*, jstring, jboolean*);
    void        (JNICALL *ReleaseStringUTFChars)(JNIEnv*, jstring, const char*);
    // Array operations
    jsize       (JNICALL *GetArrayLength)(JNIEnv*, jarray);
    jobjectArray(JNICALL *NewObjectArray)(JNIEnv*, jsize, jclass, jobject);
    jobject     (JNICALL *GetObjectArrayElement)(JNIEnv*, jobjectArray, jsize);
    void        (JNICALL *SetObjectArrayElement)(JNIEnv*, jobjectArray, jsize, jobject);
    jbooleanArray (JNICALL *NewBooleanArray)(JNIEnv*, jsize);
    jbyteArray    (JNICALL *NewByteArray)(JNIEnv*, jsize);
    jcharArray    (JNICALL *NewCharArray)(JNIEnv*, jsize);
    jshortArray   (JNICALL *NewShortArray)(JNIEnv*, jsize);
    jintArray     (JNICALL *NewIntArray)(JNIEnv*, jsize);
    jlongArray    (JNICALL *NewLongArray)(JNIEnv*, jsize);
    jfloatArray   (JNICALL *NewFloatArray)(JNIEnv*, jsize);
    jdoubleArray  (JNICALL *NewDoubleArray)(JNIEnv*, jsize);
    jboolean*   (JNICALL *GetBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*);
    jbyte*      (JNICALL *GetByteArrayElements)(JNIEnv*, jbyteArray, jboolean*);
    jchar*      (JNICALL *GetCharArrayElements)(JNIEnv*, jcharArray, jboolean*);
    jshort*     (JNICALL *GetShortArrayElements)(JNIEnv*, jshortArray, jboolean*);
    jint*       (JNICALL *GetIntArrayElements)(JNIEnv*, jintArray, jboolean*);
    jlong*      (JNICALL *GetLongArrayElements)(JNIEnv*, jlongArray, jboolean*);
    jfloat*     (JNICALL *GetFloatArrayElements)(JNIEnv*, jfloatArray, jboolean*);
    jdouble*    (JNICALL *GetDoubleArrayElements)(JNIEnv*, jdoubleArray, jboolean*);
    void        (JNICALL *ReleaseBooleanArrayElements)(JNIEnv*, jbooleanArray, jboolean*, jint);
    void        (JNICALL *ReleaseByteArrayElements)(JNIEnv*, jbyteArray, jbyte*, jint);
    void        (JNICALL *ReleaseCharArrayElements)(JNIEnv*, jcharArray, jchar*, jint);
    void        (JNICALL *ReleaseShortArrayElements)(JNIEnv*, jshortArray, jshort*, jint);
    void        (JNICALL *ReleaseIntArrayElements)(JNIEnv*, jintArray, jint*, jint);
    void        (JNICALL *ReleaseLongArrayElements)(JNIEnv*, jlongArray, jlong*, jint);
    void        (JNICALL *ReleaseFloatArrayElements)(JNIEnv*, jfloatArray, jfloat*, jint);
    void        (JNICALL *ReleaseDoubleArrayElements)(JNIEnv*, jdoubleArray, jdouble*, jint);
    void        (JNICALL *GetBooleanArrayRegion)(JNIEnv*, jbooleanArray, jsize, jsize, jboolean*);
    void        (JNICALL *GetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, jbyte*);
    void        (JNICALL *GetCharArrayRegion)(JNIEnv*, jcharArray, jsize, jsize, jchar*);
    void        (JNICALL *GetShortArrayRegion)(JNIEnv*, jshortArray, jsize, jsize, jshort*);
    void        (JNICALL *GetIntArrayRegion)(JNIEnv*, jintArray, jsize, jsize, jint*);
    void        (JNICALL *GetLongArrayRegion)(JNIEnv*, jlongArray, jsize, jsize, jlong*);
    void        (JNICALL *GetFloatArrayRegion)(JNIEnv*, jfloatArray, jsize, jsize, jfloat*);
    void        (JNICALL *GetDoubleArrayRegion)(JNIEnv*, jdoubleArray, jsize, jsize, jdouble*);
    void        (JNICALL *SetBooleanArrayRegion)(JNIEnv*, jbooleanArray, jsize, jsize, const jboolean*);
    void        (JNICALL *SetByteArrayRegion)(JNIEnv*, jbyteArray, jsize, jsize, const jbyte*);
    void        (JNICALL *SetCharArrayRegion)(JNIEnv*, jcharArray, jsize, jsize, const jchar*);
    void        (JNICALL *SetShortArrayRegion)(JNIEnv*, jshortArray, jsize, jsize, const jshort*);
    void        (JNICALL *SetIntArrayRegion)(JNIEnv*, jintArray, jsize, jsize, const jint*);
    void        (JNICALL *SetLongArrayRegion)(JNIEnv*, jlongArray, jsize, jsize, const jlong*);
    void        (JNICALL *SetFloatArrayRegion)(JNIEnv*, jfloatArray, jsize, jsize, const jfloat*);
    void        (JNICALL *SetDoubleArrayRegion)(JNIEnv*, jdoubleArray, jsize, jsize, const jdouble*);
    jint        (JNICALL *RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*, jint);
    jint        (JNICALL *UnregisterNatives)(JNIEnv*, jclass);
    jint        (JNICALL *MonitorEnter)(JNIEnv*, jobject);
    jint        (JNICALL *MonitorExit)(JNIEnv*, jobject);
    jint        (JNICALL *GetJavaVM)(JNIEnv*, JavaVM**);
    void        (JNICALL *GetStringRegion)(JNIEnv*, jstring, jsize, jsize, jchar*);
    void        (JNICALL *GetStringUTFRegion)(JNIEnv*, jstring, jsize, jsize, char*);
    void*       (JNICALL *GetPrimitiveArrayCritical)(JNIEnv*, jarray, jboolean*);
    void        (JNICALL *ReleasePrimitiveArrayCritical)(JNIEnv*, jarray, void*, jint);
    const jchar*(JNICALL *GetStringCritical)(JNIEnv*, jstring, jboolean*);
    void        (JNICALL *ReleaseStringCritical)(JNIEnv*, jstring, const jchar*);
    jweak       (JNICALL *NewWeakGlobalRef)(JNIEnv*, jobject);
    void        (JNICALL *DeleteWeakGlobalRef)(JNIEnv*, jweak);
    jboolean    (JNICALL *ExceptionCheck)(JNIEnv*);
    // Remaining NIO / module entries are unused — pad if needed
};

// ── JNIEnv wrapper ──────────────────────────────────────────────────────
struct JNIEnv_ {
    const JNINativeInterface_* functions;

    jint GetVersion() { return functions->GetVersion(this); }
    jclass FindClass(const char* n) { return functions->FindClass(this, n); }
    jthrowable ExceptionOccurred() { return functions->ExceptionOccurred(this); }
    void ExceptionDescribe() { functions->ExceptionDescribe(this); }
    void ExceptionClear() { functions->ExceptionClear(this); }
    jboolean ExceptionCheck() { return functions->ExceptionCheck(this); }
    jobject NewLocalRef(jobject o) { return functions->NewLocalRef(this, o); }
    void DeleteGlobalRef(jobject o) { functions->DeleteGlobalRef(this, o); }
    void DeleteLocalRef(jobject o) { functions->DeleteLocalRef(this, o); }
    jobject NewGlobalRef(jobject o) { return functions->NewGlobalRef(this, o); }
    jclass GetObjectClass(jobject o) { return functions->GetObjectClass(this, o); }
    jboolean IsInstanceOf(jobject o, jclass c) { return functions->IsInstanceOf(this, o, c); }

    jmethodID GetMethodID(jclass c, const char* n, const char* s) { return functions->GetMethodID(this, c, n, s); }
    jmethodID GetStaticMethodID(jclass c, const char* n, const char* s) { return functions->GetStaticMethodID(this, c, n, s); }
    jfieldID GetFieldID(jclass c, const char* n, const char* s) { return functions->GetFieldID(this, c, n, s); }
    jfieldID GetStaticFieldID(jclass c, const char* n, const char* s) { return functions->GetStaticFieldID(this, c, n, s); }

    jobject  GetObjectField(jobject o, jfieldID f) { return functions->GetObjectField(this, o, f); }
    jboolean GetBooleanField(jobject o, jfieldID f) { return functions->GetBooleanField(this, o, f); }
    jint     GetIntField(jobject o, jfieldID f) { return functions->GetIntField(this, o, f); }
    jfloat   GetFloatField(jobject o, jfieldID f) { return functions->GetFloatField(this, o, f); }
    jdouble  GetDoubleField(jobject o, jfieldID f) { return functions->GetDoubleField(this, o, f); }
    void     SetObjectField(jobject o, jfieldID f, jobject v) { functions->SetObjectField(this, o, f, v); }
    void     SetBooleanField(jobject o, jfieldID f, jboolean v) { functions->SetBooleanField(this, o, f, v); }
    void     SetIntField(jobject o, jfieldID f, jint v) { functions->SetIntField(this, o, f, v); }
    void     SetDoubleField(jobject o, jfieldID f, jdouble v) { functions->SetDoubleField(this, o, f, v); }
    void     SetFloatField(jobject o, jfieldID f, jfloat v) { functions->SetFloatField(this, o, f, v); }

    jobject GetStaticObjectField(jclass c, jfieldID f) { return functions->GetStaticObjectField(this, c, f); }

    jobject  CallObjectMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallObjectMethodV(this, o, m, a); va_end(a); return r; }
    jboolean CallBooleanMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallBooleanMethodV(this, o, m, a); va_end(a); return r; }
    jint     CallIntMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallIntMethodV(this, o, m, a); va_end(a); return r; }
    jfloat   CallFloatMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallFloatMethodV(this, o, m, a); va_end(a); return r; }
    jdouble  CallDoubleMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallDoubleMethodV(this, o, m, a); va_end(a); return r; }
    void     CallVoidMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); functions->CallVoidMethodV(this, o, m, a); va_end(a); }
    jlong    CallLongMethod(jobject o, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallLongMethodV(this, o, m, a); va_end(a); return r; }

    jobject  CallStaticObjectMethod(jclass c, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallStaticObjectMethodV(this, c, m, a); va_end(a); return r; }
    jboolean CallStaticBooleanMethod(jclass c, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallStaticBooleanMethodV(this, c, m, a); va_end(a); return r; }
    jint     CallStaticIntMethod(jclass c, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->CallStaticIntMethodV(this, c, m, a); va_end(a); return r; }
    void     CallStaticVoidMethod(jclass c, jmethodID m, ...) { va_list a; va_start(a, m); functions->CallStaticVoidMethodV(this, c, m, a); va_end(a); }

    jobject  NewObject(jclass c, jmethodID m, ...) { va_list a; va_start(a, m); auto r = functions->NewObjectV(this, c, m, a); va_end(a); return r; }

    jstring  NewStringUTF(const char* s) { return functions->NewStringUTF(this, s); }
    const char* GetStringUTFChars(jstring s, jboolean* c) { return functions->GetStringUTFChars(this, s, c); }
    void     ReleaseStringUTFChars(jstring s, const char* c) { functions->ReleaseStringUTFChars(this, s, c); }
    jsize    GetStringUTFLength(jstring s) { return functions->GetStringUTFLength(this, s); }

    jsize    GetArrayLength(jarray a) { return functions->GetArrayLength(this, a); }
    jobject  GetObjectArrayElement(jobjectArray a, jsize i) { return functions->GetObjectArrayElement(this, a, i); }

    jint PushLocalFrame(jint cap) { return functions->PushLocalFrame(this, cap); }
    jobject PopLocalFrame(jobject r) { return functions->PopLocalFrame(this, r); }

    jmethodID FromReflectedMethod(jobject method) { return functions->FromReflectedMethod(this, method); }
    jfieldID  FromReflectedField(jobject field) { return functions->FromReflectedField(this, field); }
    jclass    GetSuperclass(jclass c) { return functions->GetSuperclass(this, c); }
};

// ── JavaVM Invoke Interface ─────────────────────────────────────────────
struct JNIInvokeInterface_ {
    void* reserved0;
    void* reserved1;
    void* reserved2;
    jint (JNICALL *DestroyJavaVM)(JavaVM*);
    jint (JNICALL *AttachCurrentThread)(JavaVM*, void**, void*);
    jint (JNICALL *DetachCurrentThread)(JavaVM*);
    jint (JNICALL *GetEnv)(JavaVM*, void**, jint);
    jint (JNICALL *AttachCurrentThreadAsDaemon)(JavaVM*, void**, void*);
};

struct JavaVM_ {
    const JNIInvokeInterface_* functions;

    jint DestroyJavaVM() { return functions->DestroyJavaVM(this); }
    jint AttachCurrentThread(void** env, void* args) { return functions->AttachCurrentThread(this, env, args); }
    jint DetachCurrentThread() { return functions->DetachCurrentThread(this); }
    jint GetEnv(void** env, jint ver) { return functions->GetEnv(this, env, ver); }
    jint AttachCurrentThreadAsDaemon(void** env, void* args) { return functions->AttachCurrentThreadAsDaemon(this, env, args); }
};

// ── JNI_GetCreatedJavaVMs typedef ────────────────────────────────────────
typedef jint(JNICALL *JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);
