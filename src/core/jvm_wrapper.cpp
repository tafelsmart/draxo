#include "pch.h"
#include "core/strcrypt.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"
#include "jni_headers/jvmti_min.h"
#include <vector>
#include <string>
#include <algorithm>

bool JvmWrapper::init() {
    // ── Find jvm.dll ───────────────────────────────────────────────
    HMODULE jvmDll = GetModuleHandleA("jvm.dll");
    if (!jvmDll) {
        printf(STR_C("[Draxo] ERROR: jvm.dll not found\n"));
        return false;
    }

    auto fnGet = reinterpret_cast<JNI_GetCreatedJavaVMs_t>(GetProcAddress(jvmDll, "JNI_GetCreatedJavaVMs"));
    jsize vmCount = 0;
    if (!fnGet || fnGet(&s_vm, 1, &vmCount) != JNI_OK || vmCount == 0) {
        printf(STR_C("[Draxo] ERROR: No JVM found\n"));
        return false;
    }

    JNIEnv* env = getEnv();
    if (!env) return false;

    printf(STR_C("[Draxo] JVM attached (JNI: 0x%08X)\n"), env->GetVersion());

    // ── Get JVMTI (Primary Strategy for ClassLoader discovery) ─────
    s_vm->GetEnv(reinterpret_cast<void**>(&s_jvmti), JVMTI_VERSION_1_0);
    if (s_jvmti) {
        printf(STR_C("[Draxo] JVMTI 1.0 environment acquired\n"));
    }

    // ── Bootstrap ClassLoader ──────────────────────────────────────
    bootstrapClassLoader(env);

    return true;
}

bool JvmWrapper::bootstrapClassLoader(JNIEnv* env) {
    if (s_classLoader) return true;

    jclass mcClass = nullptr;

    // ── Strategy A: JVMTI lookup (MUST be first for Forge!) ────────
    // Forge uses TransformingClassLoader. FindClass from a native thread
    // returns a class from the SYSTEM classloader, which is a DIFFERENT
    // class object with empty static fields. JVMTI finds the ACTUAL
    // game-loaded class.
    if (s_jvmti) {
        jint count = 0;
        jclass* classes = nullptr;
        if (s_jvmti->GetLoadedClasses(&count, &classes) == JVMTI_ERROR_NONE && classes) {
            const char* targetSig = Mappings::Minecraft_Class_Sig;
            for (jint i = 0; i < count; i++) {
                char* sig = nullptr;
                if (!mcClass && s_jvmti->GetClassSignature(classes[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
                    if (strcmp(sig, targetSig) == 0) {
                        mcClass = (jclass)env->NewLocalRef(classes[i]);
                    }
                    s_jvmti->Deallocate((unsigned char*)sig);
                }
            }
            s_jvmti->Deallocate((unsigned char*)classes);
        }
        if (mcClass) {
            printf(STR_C("[Draxo] bootstrapClassLoader: Found Minecraft via JVMTI (correct classloader)\n"));
        }
    }

    // ── Strategy B: Direct FindClass (fallback, may use wrong classloader) ──
    if (!mcClass) {
        mcClass = env->FindClass(Mappings::Minecraft_Class);
        if (env->ExceptionCheck()) { env->ExceptionClear(); mcClass = nullptr; }
        if (mcClass) {
            printf(STR_C("[Draxo] bootstrapClassLoader: Found Minecraft via FindClass (may be wrong classloader!)\n"));
        }
    }

    if (!mcClass) {
        printf(STR_C("[Draxo] bootstrapClassLoader: Could not find Minecraft class reference\n"));
        return false;
    }

    // Get the ClassLoader from the Minecraft class
    jclass clsClass = env->FindClass("java/lang/Class");
    jmethodID getClassLoader = env->GetMethodID(clsClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(mcClass, getClassLoader);
    
    if (loader && !env->ExceptionCheck()) {
        s_classLoader = env->NewGlobalRef(loader);
        jclass loaderCls = env->GetObjectClass(loader);
        s_loadClass = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        
        // Log the classloader type for diagnostics
        jmethodID toStringM = env->GetMethodID(loaderCls, "toString", "()Ljava/lang/String;");
        if (toStringM) {
            jstring loaderStr = (jstring)env->CallObjectMethod(loader, toStringM);
            if (loaderStr) {
                std::string loaderName = jstringToString(loaderStr);
                printf(STR_C("[Draxo] Game ClassLoader: %s\n"), loaderName.c_str());
                env->DeleteLocalRef(loaderStr);
            }
        }
        
        env->DeleteLocalRef(loaderCls);
        env->DeleteLocalRef(loader);
        printf(STR_C("[Draxo] Game ClassLoader bootstrapped successfully\n"));
    } else {
        if (env->ExceptionCheck()) env->ExceptionClear();
        printf(STR_C("[Draxo] WARNING: Could not get ClassLoader from Minecraft class\n"));
    }

    env->DeleteLocalRef(clsClass);
    env->DeleteLocalRef(mcClass);
    return s_classLoader != nullptr;
}

void JvmWrapper::shutdown() {
    JNIEnv* env = getEnv();
    if (env) {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        for (auto& [name, cls] : s_classCache) {
            if (cls) env->DeleteGlobalRef(cls);
        }
        if (s_classLoader) env->DeleteGlobalRef(s_classLoader);
    }
    s_classCache.clear();
    s_classLoader = nullptr;
    s_loadClass = nullptr;

    if (s_vm) s_vm->DetachCurrentThread();
    s_vm = nullptr;
    s_jvmti = nullptr;
}

JNIEnv* JvmWrapper::getEnv() {
    if (!s_vm) return nullptr;
    JNIEnv* env = nullptr;
    jint result = s_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (result == JNI_OK) return env;
    
    if (result == JNI_EDETACHED) {
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_8;
        args.name = (char*)"DraxoClient_RenderThread";
        args.group = nullptr;
        // MUST use Daemon attach for rendering threads so the JVM doesn't block on exit
        if (s_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), &args) == JNI_OK) {
            return env;
        }
    }
    return nullptr;
}

JavaVM* JvmWrapper::getVM() { return s_vm; }

jclass JvmWrapper::findClass(const char* name) {
    {
        // Thread-safe cache read: the XRay search thread resolves classes
        // while the render thread does the same — an unlocked unordered_map
        // read/write race is UB and can feed garbage jclass values into JNI.
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_classCache.find(name);
        if (it != s_classCache.end()) return it->second;
    }

    JNIEnv* env = getEnv();
    if (!env) return nullptr;

    jclass localRef = nullptr;

    // ── Strategy 1: Bootstrapped ClassLoader (Fast & Reliable) ─────
    if (s_classLoader && s_loadClass) {
        std::string dotName(name);
        for (auto& c : dotName) if (c == '/') c = '.';
        jstring jName = env->NewStringUTF(dotName.c_str());
        localRef = (jclass)env->CallObjectMethod(s_classLoader, s_loadClass, jName);
        env->DeleteLocalRef(jName);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Strategy 2: standard FindClass ─────────────────────────────
    if (!localRef) {
        localRef = env->FindClass(name);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Strategy 3: JVMTI fallback (Slow, only used if others fail) ──
    if (!localRef && s_jvmti) {
        jint count = 0;
        jclass* classes = nullptr;
        if (s_jvmti->GetLoadedClasses(&count, &classes) == JVMTI_ERROR_NONE && classes) {
            std::string target = "L"; target += name; target += ";";
            for (jint i = 0; i < count; i++) {
                char* sig = nullptr;
                if (!localRef && s_jvmti->GetClassSignature(classes[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
                    if (target == sig) {
                        localRef = (jclass)env->NewLocalRef(classes[i]);
                    }
                    s_jvmti->Deallocate((unsigned char*)sig);
                }
            }
            s_jvmti->Deallocate((unsigned char*)classes);
        }
    }

    if (!localRef) {
        printf(STR_C("[Draxo] WARNING: Class not found: %s\n"), name);
        return nullptr;
    }

    jclass globalRef = static_cast<jclass>(env->NewGlobalRef(localRef));
    env->DeleteLocalRef(localRef);
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        // Re-check under the lock: another thread may have resolved the same
        // class while we were doing the lookup. If so, prefer the existing
        // entry and free our duplicate global ref (avoids a global-ref leak).
        auto it = s_classCache.find(name);
        if (it != s_classCache.end()) {
            env->DeleteGlobalRef(globalRef);
            return it->second;
        }
        s_classCache[name] = globalRef;
    }
    printf(STR_C("[Draxo] Cached class: %s\n"), name);
    return globalRef;
}

// ... rest of the method IDs helpers (standard GetMethodID calls) ...
jmethodID JvmWrapper::getMethodID(jclass cls, const char* name, const char* sig) {
    JNIEnv* env = getEnv();
    if (!env || !cls) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (!env->ExceptionCheck() && id) return id;
    if (env->ExceptionCheck()) env->ExceptionClear();

    return findMethodViaJVMTI(env, cls, name, false);
}

jmethodID JvmWrapper::getStaticMethodID(jclass cls, const char* name, const char* sig) {
    JNIEnv* env = getEnv();
    if (!env || !cls) return nullptr;
    jmethodID id = env->GetStaticMethodID(cls, name, sig);
    if (!env->ExceptionCheck() && id) return id;
    if (env->ExceptionCheck()) env->ExceptionClear();

    return findMethodViaJVMTI(env, cls, name, true);
}

jfieldID JvmWrapper::getFieldID(jclass cls, const char* name, const char* sig) {
    JNIEnv* env = getEnv();
    if (!env || !cls) return nullptr;
    jfieldID id = env->GetFieldID(cls, name, sig);
    if (!env->ExceptionCheck() && id) return id;
    if (env->ExceptionCheck()) env->ExceptionClear();

    return findFieldViaJVMTI(env, cls, name, false);
}

jfieldID JvmWrapper::getStaticFieldID(jclass cls, const char* name, const char* sig) {
    JNIEnv* env = getEnv();
    if (!env || !cls) return nullptr;
    jfieldID id = env->GetStaticFieldID(cls, name, sig);
    if (!env->ExceptionCheck() && id) return id;
    if (env->ExceptionCheck()) env->ExceptionClear();

    return findFieldViaJVMTI(env, cls, name, true);
}

jmethodID JvmWrapper::findMethodViaJVMTI(JNIEnv* env, jclass cls, const char* name, bool isStatic) {
    if (!s_jvmti) return nullptr;

    jmethodID result = nullptr;
    jclass currentCls = cls; // cls is a global ref from findClass or SDK

    printf(STR_C("[Draxo] JVMTI Scan: method '%s' (static: %d)\n"), name, isStatic);

    while (currentCls && !result) {
        jint methodCount = 0;
        jmethodID* methods = nullptr;
        jvmtiError err = s_jvmti->GetClassMethods(currentCls, &methodCount, &methods);
        if (err == JVMTI_ERROR_NONE) {
            for (jint i = 0; i < methodCount; i++) {
                char* mName = nullptr; char* mSig = nullptr;
                if (s_jvmti->GetMethodName(methods[i], &mName, &mSig, nullptr) == JVMTI_ERROR_NONE) {
                    if (strcmp(mName, name) == 0) {
                        jint mods = 0;
                        s_jvmti->GetMethodModifiers(methods[i], &mods);
                        bool isStat = (mods & 0x0008) != 0;
                        if (isStat == isStatic) {
                            result = methods[i];
                        }
                    }
                    if (mName) s_jvmti->Deallocate((unsigned char*)mName);
                    if (mSig) s_jvmti->Deallocate((unsigned char*)mSig);
                }
                if (result) break;
            }
            s_jvmti->Deallocate((unsigned char*)methods);
        } else {
            printf(STR_C("[Draxo]   GetClassMethods err: %d\n"), (int)err);
        }

        if (!result) {
            jclass super = env->GetSuperclass(currentCls);
            if (!super) break;
            // env->GetSuperclass returns a local ref
            if (currentCls != cls) env->DeleteLocalRef(currentCls);
            currentCls = super;
        }
    }

    if (currentCls && currentCls != cls) env->DeleteLocalRef(currentCls);
    
    if (result) printf(STR_C("[Draxo]   JVMTI Found '%s' OK\n"), name);
    else printf(STR_C("[Draxo]   JVMTI FAILED to find '%s'\n"), name);
    return result;
}

jfieldID JvmWrapper::findFieldViaJVMTI(JNIEnv* env, jclass cls, const char* name, bool isStatic) {
    if (!s_jvmti) return nullptr;

    jfieldID result = nullptr;
    jclass currentCls = cls;

    printf(STR_C("[Draxo] JVMTI Scan: field '%s' (static: %d)\n"), name, isStatic);

    while (currentCls && !result) {
        jint fieldCount = 0;
        jfieldID* fields = nullptr;
        if (s_jvmti->GetClassFields(currentCls, &fieldCount, &fields) == JVMTI_ERROR_NONE) {
            for (jint i = 0; i < fieldCount; i++) {
                char* fName = nullptr; char* fSig = nullptr;
                if (s_jvmti->GetFieldName(currentCls, fields[i], &fName, &fSig, nullptr) == JVMTI_ERROR_NONE) {
                    if (strcmp(fName, name) == 0) {
                        jint mods = 0;
                        s_jvmti->GetFieldModifiers(currentCls, fields[i], &mods);
                        bool isStat = (mods & 0x0008) != 0;
                        if (isStat == isStatic) {
                            result = fields[i];
                        }
                    }
                    if (fName) s_jvmti->Deallocate((unsigned char*)fName);
                    if (fSig) s_jvmti->Deallocate((unsigned char*)fSig);
                }
                if (result) break;
            }
            s_jvmti->Deallocate((unsigned char*)fields);
        }

        if (!result) {
            jclass super = env->GetSuperclass(currentCls);
            if (!super) break;
            if (currentCls != cls) env->DeleteLocalRef(currentCls);
            currentCls = super;
        }
    }

    if (currentCls && currentCls != cls) env->DeleteLocalRef(currentCls);

    if (result) printf(STR_C("[Draxo]   JVMTI Found field '%s' OK\n"), name);
    else printf(STR_C("[Draxo]   JVMTI FAILED to find field '%s'\n"), name);
    return result;
}

std::string JvmWrapper::jstringToString(jstring str) {
    JNIEnv* env = getEnv();
    if (!env || !str) return "";
    const char* utf = env->GetStringUTFChars(str, nullptr);
    if (!utf) return "";
    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    // Cleanup any hidden whitespace or descriptors if they leaked
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

bool JvmWrapper::checkException() {
    JNIEnv* env = getEnv();
    if (!env) return false;
    if (env->ExceptionCheck()) {
        // Rate-limited ExceptionDescribe: printing a full stack trace on hot
        // paths (e.g. per-entity per-frame lookups) allocates heavily and spams
        // stderr. Describe only the first few, then just clear silently.
        // Atomic: both the render thread and the XRay thread call this.
        static std::atomic<int> s_described{0};
        if (s_described.fetch_add(1) < 5) env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

void JvmWrapper::dumpAllClasses(const char* filter) {
    if (!s_jvmti) return;
    jint count = 0; jclass* classes = nullptr;
    if (s_jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE) return;
    
    printf(STR_C("[Draxo] --- JVMTI Class Dump (filter: %s, count: %d) ---\n"), filter ? filter : "none", count);
    for (jint i = 0; i < count; i++) {
        char* sig = nullptr;
        if (s_jvmti->GetClassSignature(classes[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
            if (!filter || strstr(sig, filter)) printf(STR_C("[Draxo]   %s\n"), sig);
            s_jvmti->Deallocate((unsigned char*)sig);
        }
    }
    s_jvmti->Deallocate((unsigned char*)classes);
}

void JvmWrapper::dumpClassInfo(jclass cls, const char* label, int maxMethods, int maxFields) {
    if (!s_jvmti) return;
    printf(STR_C("\n[Draxo] ====== JVMTI CLASS DUMP: %s ======\n"), label);

    jint methodCount = 0;
    jmethodID* methods = nullptr;
    jvmtiError err = s_jvmti->GetClassMethods(cls, &methodCount, &methods);
    if (err == JVMTI_ERROR_NONE) {
        printf(STR_C("[Draxo]   Methods (%d total):\n"), (int)methodCount);
        for (jint i = 0; i < methodCount; i++) {
            char* mName = nullptr; char* mSig = nullptr;
            if (s_jvmti->GetMethodName(methods[i], &mName, &mSig, nullptr) == JVMTI_ERROR_NONE) {
                jint mods = 0;
                s_jvmti->GetMethodModifiers(methods[i], &mods);
                printf(STR_C("[Draxo]     %s %s %s\n"), (mods & 0x0008) ? "static" : "", mName, mSig);
                s_jvmti->Deallocate((unsigned char*)mName);
                s_jvmti->Deallocate((unsigned char*)mSig);
            }
        }
        s_jvmti->Deallocate((unsigned char*)methods);
    } else {
        printf(STR_C("[Draxo]   GetClassMethods err: %d\n"), (int)err);
    }

    jint fieldCount = 0;
    jfieldID* fields = nullptr;
    err = s_jvmti->GetClassFields(cls, &fieldCount, &fields);
    if (err == JVMTI_ERROR_NONE) {
        printf(STR_C("[Draxo]   Fields (%d total):\n"), (int)fieldCount);
        for (jint i = 0; i < fieldCount; i++) {
            char* fName = nullptr; char* fSig = nullptr;
            if (s_jvmti->GetFieldName(cls, fields[i], &fName, &fSig, nullptr) == JVMTI_ERROR_NONE) {
                jint mods = 0;
                s_jvmti->GetFieldModifiers(cls, fields[i], &mods);
                printf(STR_C("[Draxo]     %s %s %s\n"), (mods & 0x0008) ? "static" : "", fName, fSig);
                s_jvmti->Deallocate((unsigned char*)fName);
                s_jvmti->Deallocate((unsigned char*)fSig);
            }
        }
        s_jvmti->Deallocate((unsigned char*)fields);
    } else {
        printf(STR_C("[Draxo]   GetClassFields err: %d\n"), (int)err);
    }

    printf(STR_C("[Draxo] ====== END DUMP ====== \n\n"));
}
