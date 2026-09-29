#include "pch.h"
#include "core/strcrypt.h"
#include "sdk/minecraft.h"

bool CMinecraft::initIDs() {
    s_mcClass = JvmWrapper::findClass(Mappings::Minecraft_Class);
    if (!s_mcClass) {
        printf(STR_C("[Draxo] ERROR: Minecraft class not found!\n"));
        return false;
    }

    // ── Try to find getInstance method ──────────────────────────────
    s_getInstance = JvmWrapper::getStaticMethodID(s_mcClass, Mappings::MC_getInstance, Mappings::MC_getInstance_Sig);
    
    // ── Try to find the static 'instance' field directly ────────────
    // In Forge 1.21.1, the singleton may be stored in a field called 'instance'
    JNIEnv* env = JvmWrapper::getEnv();
    if (env) {
        // Try common field names for the singleton
        const char* fieldNames[] = { Mappings::MC_instance, "INSTANCE" };
        for (auto fname : fieldNames) {
            if (s_instanceField) break;
            s_instanceField = env->GetStaticFieldID(s_mcClass, fname, Mappings::MC_getInstance_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_instanceField = nullptr; }
            if (s_instanceField) {
                printf(STR_C("[Draxo] Found Minecraft singleton field: '%s'\n"), fname);
            }
        }
        
        // If standard field lookup fails, try JVMTI scan for static fields of Minecraft type
        if (!s_instanceField) {
            s_instanceField = JvmWrapper::getStaticFieldID(s_mcClass, Mappings::MC_instance, Mappings::MC_getInstance_Sig);
            if (s_instanceField) {
                printf(STR_C("[Draxo] Found Minecraft singleton field via JVMTI: 'instance'\n"));
            }
        }
    }
    
    s_player = JvmWrapper::getFieldID(s_mcClass, Mappings::MC_player, Mappings::MC_player_Sig);
    s_level  = JvmWrapper::getFieldID(s_mcClass, Mappings::MC_level, Mappings::MC_level_Sig);
    s_gameRenderer = JvmWrapper::getFieldID(s_mcClass, Mappings::MC_gameRenderer, Mappings::MC_gameRenderer_Sig);
    s_options = JvmWrapper::getFieldID(s_mcClass, Mappings::MC_options, Mappings::MC_options_Sig);

    // ── GameRenderer ───────────────────────────────────────────────────
    s_grClass = JvmWrapper::findClass(Mappings::GameRenderer_Class);
    if (s_grClass) {
        // DISABLED: JvmWrapper::dumpClassInfo(s_grClass, "GameRenderer", 200, 50);
        
        s_getMainCamera = JvmWrapper::getMethodID(s_grClass, Mappings::GR_getMainCamera, Mappings::GR_getMainCamera_Sig);
        // getProjectionMatrix: 1.21.1 = (D)double, 1.21.2+ = (F)float partialTicks
        if (env->ExceptionCheck()) env->ExceptionClear();  // defensiv: keine Alt-Exception
        s_getGRProjMatrix = env->GetMethodID(s_grClass, Mappings::GR_getProjectionMatrix, Mappings::GR_getProjectionMatrix_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_getGRProjMatrix = nullptr; }
        if (!s_getGRProjMatrix) {
            s_getGRProjMatrix = env->GetMethodID(s_grClass, Mappings::GR_getProjectionMatrix, Mappings::GR_getProjectionMatrix_Sig_Legacy);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getGRProjMatrix = nullptr; }
            if (s_getGRProjMatrix) s_grProjTakesFloat = false;
        }
        s_getFov = env->GetMethodID(s_grClass, Mappings::GR_getFov, Mappings::GR_getFov_Sig);
        
        if (s_getMainCamera) printf(STR_C("[Draxo] GameRenderer::getMainCamera resolved OK\n"));
        if (s_getGRProjMatrix) printf(STR_C("[Draxo] GameRenderer::getProjectionMatrix resolved OK (%s)\n"),
                                      s_grProjTakesFloat ? "float" : "double");
        if (s_getFov) printf(STR_C("[Draxo] GameRenderer::getFov resolved OK\n"));
        
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Camera (fields, not methods — Camera is NOT an Entity subclass) ──
    s_camClass = JvmWrapper::findClass(Mappings::Camera_Class);
    if (s_camClass) {
        // DISABLED: JvmWrapper::dumpClassInfo(s_camClass, "Camera", 30, 20);
        
        // Camera stores position/rotation as FIELDS
        s_camPosition = env->GetFieldID(s_camClass, Mappings::Camera_position, Mappings::Camera_position_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_camPosition = nullptr; }
        
        s_camYRot = env->GetFieldID(s_camClass, Mappings::Camera_yRot, Mappings::Camera_yRot_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_camYRot = nullptr; }
        
        s_camXRot = env->GetFieldID(s_camClass, Mappings::Camera_xRot, Mappings::Camera_xRot_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_camXRot = nullptr; }
        
        bool camOK = s_camPosition && s_camYRot && s_camXRot;
        printf(STR_C("[Draxo] Camera fields resolved: %s (pos=%p yRot=%p xRot=%p)\n"),
               camOK ? "OK" : "PARTIAL", s_camPosition, s_camYRot, s_camXRot);
    } else {
        printf(STR_C("[Draxo] WARN: Camera class not found\n"));
    }

    // ── Options FOV ────────────────────────────────────────────────────
    s_optionsClass = JvmWrapper::findClass(Mappings::Options_Class);
    if (s_optionsClass && s_options) {
        // fov is an OptionInstance<Integer>
        s_optFov = env->GetFieldID(s_optionsClass, Mappings::Options_fov, Mappings::Options_fov_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_optFov = nullptr; }
        
        if (s_optFov) {
            // OptionInstance.get() returns Object
            jclass optInstClass = JvmWrapper::findClass(Mappings::OptionInstance_Class);
            if (optInstClass) {
                s_optInstGet = env->GetMethodID(optInstClass, Mappings::OptionInstance_get, Mappings::OptionInstance_get_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_optInstGet = nullptr; }
                printf(STR_C("[Draxo] Options FOV resolved: %s\n"), s_optInstGet ? "OK" : "FAILED");
            }
        } else {
            printf(STR_C("[Draxo] WARN: Options.fov field not found\n"));
        }
    }

    // ── Window (framebuffer dimensions for aspect ratio) ─────────────
    s_window = env->GetFieldID(s_mcClass, Mappings::MC_window, Mappings::MC_window_Sig);
    if (env->ExceptionCheck()) { env->ExceptionClear(); s_window = nullptr; }

    if (s_window) {
        s_windowClass = JvmWrapper::findClass(Mappings::Window_Class);
        if (s_windowClass) {
            s_getWidth  = env->GetMethodID(s_windowClass, Mappings::Window_getWidth, Mappings::Window_getWidth_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getWidth = nullptr; }
            s_getHeight = env->GetMethodID(s_windowClass, Mappings::Window_getHeight, Mappings::Window_getHeight_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_getHeight = nullptr; }
            printf(STR_C("[Draxo] Window resolved: %s (w=%p h=%p)\n"),
                   (s_getWidth && s_getHeight) ? "OK" : "PARTIAL", s_getWidth, s_getHeight);
        } else {
            printf(STR_C("[Draxo] WARN: Window class not found\n"));
        }
    } else {
        printf(STR_C("[Draxo] WARN: Minecraft.window field not found\n"));
    }

    // ── RenderSystem and Matrix4f ─────────────────────────────────────
    s_rsClass = JvmWrapper::findClass(Mappings::RenderSystem_Class);
    if (s_rsClass) {
        s_rsGetProj = env->GetStaticMethodID(s_rsClass,
            Mappings::RS_getProjectionMatrix, Mappings::RS_getProjectionMatrix_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_rsGetProj = nullptr; }

        s_rsGetModelView = env->GetStaticMethodID(s_rsClass,
            Mappings::RS_getModelViewMatrix, Mappings::RS_getModelViewMatrix_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_rsGetModelView = nullptr; }

        s_mat4Class = JvmWrapper::findClass(Mappings::Matrix4f_Class);
        if (s_mat4Class) {
            #define GET_MAT_FIELD(row, col) \
                s_mat4_m##row##col = env->GetFieldID(s_mat4Class, Mappings::Matrix4f_m##row##col, "F"); \
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_mat4_m##row##col = nullptr; }

            GET_MAT_FIELD(0, 0); GET_MAT_FIELD(0, 1); GET_MAT_FIELD(0, 2); GET_MAT_FIELD(0, 3);
            GET_MAT_FIELD(1, 0); GET_MAT_FIELD(1, 1); GET_MAT_FIELD(1, 2); GET_MAT_FIELD(1, 3);
            GET_MAT_FIELD(2, 0); GET_MAT_FIELD(2, 1); GET_MAT_FIELD(2, 2); GET_MAT_FIELD(2, 3);
            GET_MAT_FIELD(3, 0); GET_MAT_FIELD(3, 1); GET_MAT_FIELD(3, 2); GET_MAT_FIELD(3, 3);
            
            #undef GET_MAT_FIELD
        }
        printf(STR_C("[Draxo] RenderSystem matrices: %s (getProj=%p getModelView=%p)\n"),
               (s_rsGetProj && s_rsGetModelView && s_mat4_m00) ? "OK" : "PARTIAL",
               s_rsGetProj, s_rsGetModelView);
    } else {
        printf(STR_C("[Draxo] WARN: RenderSystem class not found\n"));
    }

    printf(STR_C("[Draxo] Minecraft IDs: getInstance=%p instanceField=%p player=%p level=%p\n"),
           s_getInstance, s_instanceField, s_player, s_level);

    bool ok = (s_getInstance || s_instanceField) && s_player && s_level;
    printf(STR_C("[Draxo] Minecraft IDs initialized: %s\n"), ok ? "OK" : "PARTIAL");
    
    if (!ok) {
        if (!s_getInstance && !s_instanceField) printf(STR_C("[Draxo]   FAILED: no way to get Minecraft instance\n"));
        if (!s_player) printf(STR_C("[Draxo]   FAILED: player field\n"));
        if (!s_level)  printf(STR_C("[Draxo]   FAILED: level field\n"));
        // DISABLED: JvmWrapper::dumpClassInfo(s_mcClass, "Minecraft Diagnostics", 10, 10);
    }

    return ok;
}

jobject CMinecraft::getInstance() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return nullptr;

    // ── Strategy 1: Read static field directly (most reliable) ──────
    if (s_instanceField) {
        jobject mc = env->GetStaticObjectField(s_mcClass, s_instanceField);
        if (env->ExceptionCheck()) {
            static int excCount = 0;
            if (++excCount <= 3) {
                printf(STR_C("[Draxo] getInstance: exception reading instance field!\n"));
                env->ExceptionDescribe();
            }
            env->ExceptionClear();
        }
        if (mc) return mc;
        
        static int fieldNullCount = 0;
        if (++fieldNullCount <= 3) {
            printf(STR_C("[Draxo] getInstance: static field returned null\n"));
        }
    }

    // ── Strategy 2: Call getInstance() method ───────────────────────
    if (s_getInstance && s_mcClass) {
        jobject mc = env->CallStaticObjectMethod(s_mcClass, s_getInstance);
        if (env->ExceptionCheck()) {
            static int excCount = 0;
            if (++excCount <= 3) {
                printf(STR_C("[Draxo] getInstance: exception calling getInstance()!\n"));
                env->ExceptionDescribe();
            }
            env->ExceptionClear();
            return nullptr;
        }
        if (mc) return mc;
        
        static int methodNullCount = 0;
        if (++methodNullCount <= 3) {
            printf(STR_C("[Draxo] getInstance: method returned null (no exception)\n"));
        }
    }

    // ── Strategy 3: Last resort — scan all static fields via reflection ──
    static bool triedScan = false;
    if (!triedScan && s_mcClass && env) {
        triedScan = true;
        printf(STR_C("[Draxo] getInstance: trying brute-force field scan...\n"));
        
        jclass clsClass = env->FindClass("java/lang/Class");
        if (clsClass) {
            jmethodID getDeclaredFields = env->GetMethodID(clsClass, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
            if (getDeclaredFields) {
                jobjectArray fields = (jobjectArray)env->CallObjectMethod(s_mcClass, getDeclaredFields);
                if (fields && !env->ExceptionCheck()) {
                    jsize fieldCount = env->GetArrayLength(fields);
                    printf(STR_C("[Draxo] getInstance: scanning %d declared fields...\n"), fieldCount);
                    
                    jclass fieldClass = env->FindClass("java/lang/reflect/Field");
                    jmethodID getNameM = env->GetMethodID(fieldClass, "getName", "()Ljava/lang/String;");
                    jmethodID getModifiers = env->GetMethodID(fieldClass, "getModifiers", "()I");
                    jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
                    jmethodID isStaticM = env->GetStaticMethodID(modifierClass, "isStatic", "(I)Z");
                    
                    for (jsize i = 0; i < fieldCount; i++) {
                        jobject field = env->GetObjectArrayElement(fields, i);
                        jstring fname = (jstring)env->CallObjectMethod(field, getNameM);
                        jint mods = env->CallIntMethod(field, getModifiers);
                        bool isStatic = env->CallStaticBooleanMethod(modifierClass, isStaticM, mods);
                        
                        std::string name = JvmWrapper::jstringToString(fname);
                        
                        if (isStatic) {
                            // Try to read it as a Minecraft-typed field
                            jfieldID fid = env->GetStaticFieldID(s_mcClass, name.c_str(), 
                                Mappings::Minecraft_Class_Sig);
                            if (env->ExceptionCheck()) { env->ExceptionClear(); fid = nullptr; }
                            
                            if (fid) {
                                printf(STR_C("[Draxo]   Static field '%s' matches Minecraft type!\n"), name.c_str());
                                jobject mc = env->GetStaticObjectField(s_mcClass, fid);
                                if (mc) {
                                    printf(STR_C("[Draxo]   SUCCESS: Got Minecraft instance via field '%s'!\n"), name.c_str());
                                    s_instanceField = fid;
                                    env->DeleteLocalRef(field);
                                    if (fname) env->DeleteLocalRef(fname);
                                    env->DeleteLocalRef(fields);
                                    env->DeleteLocalRef(fieldClass);
                                    env->DeleteLocalRef(modifierClass);
                                    env->DeleteLocalRef(clsClass);
                                    return mc;
                                } else {
                                    printf(STR_C("[Draxo]   Field '%s' has Minecraft type but value is null\n"), name.c_str());
                                }
                            }
                        }
                        
                        env->DeleteLocalRef(field);
                        if (fname) env->DeleteLocalRef(fname);
                    }
                    env->DeleteLocalRef(fields);
                    env->DeleteLocalRef(fieldClass);
                    env->DeleteLocalRef(modifierClass);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(clsClass);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    return nullptr;
}

jobject CMinecraft::getPlayer() {
    if (!s_player) return nullptr;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return nullptr;

    jobject mc = getInstance();
    if (!mc) {
        static int warnCount = 0;
        warnCount++;
        if (warnCount <= 3 || warnCount % 1800 == 0) {
            printf(STR_C("[Draxo] getPlayer: getInstance() returned null (count=%d)\n"), warnCount);
        }
        return nullptr;
    }

    jobject player = env->GetObjectField(mc, s_player);
    if (JvmWrapper::checkException()) {
        env->DeleteLocalRef(mc);
        return nullptr;
    }
    
    if (!player) {
        static int nullCount = 0;
        nullCount++;
        if (nullCount <= 3 || nullCount % 1800 == 0) {
            printf(STR_C("[Draxo] getPlayer: player field is null (not in world?) count=%d\n"), nullCount);
        }
    }
    
    env->DeleteLocalRef(mc);
    return player;
}

jobject CMinecraft::getWorld() {
    if (!s_level) return nullptr;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return nullptr;

    jobject mc = getInstance();
    if (!mc) return nullptr;

    jobject world = env->GetObjectField(mc, s_level);
    // Never let a pending exception bleed into the caller's next JNI call —
    // calling JNI functions with a pending exception is undefined behaviour.
    if (env->ExceptionCheck()) { env->ExceptionClear(); world = nullptr; }
    env->DeleteLocalRef(mc);
    return world;
}

CMinecraft::CameraData CMinecraft::getCameraData() {
    // Per-frame cache: HUD coords/direction, ESP, Tracers, Waypoints and
    // TargetHUD call this 4-6x per frame. Every call does ~30 JNI calls and
    // allocates fresh Matrix4f/Vec3 objects via JNI (young-gen churn that
    // forces frequent GCs). The camera is constant within one frame, so
    // caching per ImGui frame is safe and cuts our JNI+allocation load ~5x.
    // thread_local: the XRay background thread also calls this — a shared
    // static would race. The render thread gets the per-frame cache; the
    // XRay thread (once/sec) simply never hits it.
    static thread_local int s_cachedFrame = -1;
    static thread_local CameraData s_cached{};
    int frame = ImGui::GetFrameCount();
    if (frame == s_cachedFrame) return s_cached;

    CameraData data{};
    data.valid = false;
    data.fov = 70.0f;  // default

    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return data;

    jobject mc = getInstance();
    if (!mc) return data;

    // ── Read FOV from options ────────────────────────────────────────
    if (s_options && s_optFov && s_optInstGet) {
        jobject opts = env->GetObjectField(mc, s_options);
        if (opts && !env->ExceptionCheck()) {
            jobject fovOpt = env->GetObjectField(opts, s_optFov);
            if (fovOpt && !env->ExceptionCheck()) {
                jobject fovBoxed = env->CallObjectMethod(fovOpt, s_optInstGet);
                if (fovBoxed && !env->ExceptionCheck()) {
                    // Unbox Integer → int
                    jclass intClass = env->FindClass("java/lang/Integer");
                    if (intClass) {
                        jmethodID intValue = env->GetMethodID(intClass, "intValue", "()I");
                        if (intValue) {
                            data.fov = (float)env->CallIntMethod(fovBoxed, intValue);
                        }
                        env->DeleteLocalRef(intClass);
                    }
                    env->DeleteLocalRef(fovBoxed);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(fovOpt);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(opts);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Read camera position from GameRenderer.getMainCamera() ──────
    jobject gr = nullptr;
    if (s_gameRenderer) {
        gr = env->GetObjectField(mc, s_gameRenderer);
        if (env->ExceptionCheck()) { env->ExceptionClear(); gr = nullptr; }
    }

    if (gr && s_getMainCamera && s_camPosition && s_camYRot && s_camXRot) {
        jobject camera = env->CallObjectMethod(gr, s_getMainCamera);
        if (camera && !env->ExceptionCheck()) {
            // Read Camera.position (Vec3 field)
            jobject pos = env->GetObjectField(camera, s_camPosition);
            if (pos && !env->ExceptionCheck()) {
                jclass v3c = env->GetObjectClass(pos);
                jfieldID fx = env->GetFieldID(v3c, Mappings::Vec3_x, Mappings::Vec3_D_Sig);
                jfieldID fy = env->GetFieldID(v3c, Mappings::Vec3_y, Mappings::Vec3_D_Sig);
                jfieldID fz = env->GetFieldID(v3c, Mappings::Vec3_z, Mappings::Vec3_D_Sig);
                if (fx && fy && fz) {
                    data.x = env->GetDoubleField(pos, fx);
                    data.y = env->GetDoubleField(pos, fy);
                    data.z = env->GetDoubleField(pos, fz);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(v3c);
                env->DeleteLocalRef(pos);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();

            // Read Camera.yRot, Camera.xRot (float fields)
            data.yaw   = env->GetFloatField(camera, s_camYRot);
            if (env->ExceptionCheck()) env->ExceptionClear();
            data.pitch = env->GetFloatField(camera, s_camXRot);
            if (env->ExceptionCheck()) env->ExceptionClear();

            data.valid = true;
            
            // ── Read exact dynamic FOV ────────────────────────────────────────
            if (s_getFov) {
                // getFov(Camera camera, float partialTicks, boolean useFovSetting)
                float fov = env->CallFloatMethod(gr, s_getFov, camera, 1.0f, JNI_TRUE);
                if (!env->ExceptionCheck()) {
                    data.fov = fov;
                } else {
                    env->ExceptionClear();
                }
            }
            // ------------------------------------------------------------------
            
            static int camLogCount = 0;
            if (++camLogCount <= 5 || camLogCount % 1800 == 0) {
                printf(STR_C("[Draxo] CameraData: pos=(%.1f,%.1f,%.1f) yaw=%.1f pitch=%.1f fov=%.1f\n"),
                       data.x, data.y, data.z, data.yaw, data.pitch, data.fov);
            }
            
            env->DeleteLocalRef(camera);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Read Window framebuffer dimensions ─────────────────────────────
    if (s_window && s_getWidth && s_getHeight) {
        jobject window = env->GetObjectField(mc, s_window);
        if (window && !env->ExceptionCheck()) {
            data.screenW = env->CallIntMethod(window, s_getWidth);
            if (env->ExceptionCheck()) env->ExceptionClear();
            data.screenH = env->CallIntMethod(window, s_getHeight);
            if (env->ExceptionCheck()) env->ExceptionClear();
            
            static int winLogCount = 0;
            if (++winLogCount <= 5 || winLogCount % 1800 == 0) {
                printf(STR_C("[Draxo] Window framebuffer: %dx%d (aspect=%.3f)\n"),
                       data.screenW, data.screenH,
                       data.screenH > 0 ? (double)data.screenW / data.screenH : 0.0);
            }
            env->DeleteLocalRef(window);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ── Read Matrices ────────────────────────────────────────────────
    // 1.21.1: RenderSystem.getProjectionMatrix()/getModelViewMatrix() statisch
    // 1.21.2+: RenderSystem-Projektion entfernt -> GameRenderer.getProjectionMatrix(partialTicks)
    data.hasMatrices = false;
    jobject projMat = nullptr;
    if (s_rsClass && s_rsGetProj) {
        projMat = env->CallStaticObjectMethod(s_rsClass, s_rsGetProj);
        if (env->ExceptionCheck()) { env->ExceptionClear(); projMat = nullptr; }
    }
    if (!projMat && gr && s_getGRProjMatrix) {
        // 1.21.2+ Pfad: GameRenderer.getProjectionMatrix(partialTicks)
        projMat = s_grProjTakesFloat
            ? env->CallObjectMethod(gr, s_getGRProjMatrix, 1.0f)
            : env->CallObjectMethod(gr, s_getGRProjMatrix, 1.0);
        if (env->ExceptionCheck()) { env->ExceptionClear(); projMat = nullptr; }
    }
    jobject viewMat = nullptr;
    if (s_rsClass && s_rsGetModelView) {
        viewMat = env->CallStaticObjectMethod(s_rsClass, s_rsGetModelView);
        if (env->ExceptionCheck()) { env->ExceptionClear(); viewMat = nullptr; }
    }
    if (projMat && viewMat && s_mat4_m00) {
        #define READ_MAT(obj, arr, r, c, idx) \
            arr[idx] = env->GetFloatField(obj, s_mat4_m##r##c)
        
        READ_MAT(projMat, data.projMatrix, 0, 0, 0); READ_MAT(projMat, data.projMatrix, 1, 0, 1); READ_MAT(projMat, data.projMatrix, 2, 0, 2); READ_MAT(projMat, data.projMatrix, 3, 0, 3);
        READ_MAT(projMat, data.projMatrix, 0, 1, 4); READ_MAT(projMat, data.projMatrix, 1, 1, 5); READ_MAT(projMat, data.projMatrix, 2, 1, 6); READ_MAT(projMat, data.projMatrix, 3, 1, 7);
        READ_MAT(projMat, data.projMatrix, 0, 2, 8); READ_MAT(projMat, data.projMatrix, 1, 2, 9); READ_MAT(projMat, data.projMatrix, 2, 2, 10); READ_MAT(projMat, data.projMatrix, 3, 2, 11);
        READ_MAT(projMat, data.projMatrix, 0, 3, 12); READ_MAT(projMat, data.projMatrix, 1, 3, 13); READ_MAT(projMat, data.projMatrix, 2, 3, 14); READ_MAT(projMat, data.projMatrix, 3, 3, 15);

        READ_MAT(viewMat, data.viewMatrix, 0, 0, 0); READ_MAT(viewMat, data.viewMatrix, 1, 0, 1); READ_MAT(viewMat, data.viewMatrix, 2, 0, 2); READ_MAT(viewMat, data.viewMatrix, 3, 0, 3);
        READ_MAT(viewMat, data.viewMatrix, 0, 1, 4); READ_MAT(viewMat, data.viewMatrix, 1, 1, 5); READ_MAT(viewMat, data.viewMatrix, 2, 1, 6); READ_MAT(viewMat, data.viewMatrix, 3, 1, 7);
        READ_MAT(viewMat, data.viewMatrix, 0, 2, 8); READ_MAT(viewMat, data.viewMatrix, 1, 2, 9); READ_MAT(viewMat, data.viewMatrix, 2, 2, 10); READ_MAT(viewMat, data.viewMatrix, 3, 2, 11);
        READ_MAT(viewMat, data.viewMatrix, 0, 3, 12); READ_MAT(viewMat, data.viewMatrix, 1, 3, 13); READ_MAT(viewMat, data.viewMatrix, 2, 3, 14); READ_MAT(viewMat, data.viewMatrix, 3, 3, 15);
        
        #undef READ_MAT
        data.hasMatrices = true;
        // Einmaliges Sanity-Log: bestätigt, dass der neue 1.21.2+-Matrix-Pfad greift
        static bool s_loggedMats = false;
        if (!s_loggedMats) {
            s_loggedMats = true;
            printf(STR_C("[Draxo] Matrices OK: proj[0]=%.3f proj[5]=%.3f proj[10]=%.3f proj[15]=%.3f view[0]=%.3f view[12]=%.3f\n"),
                   data.projMatrix[0], data.projMatrix[5], data.projMatrix[10], data.projMatrix[15],
                   data.viewMatrix[0], data.viewMatrix[12]);
        }
    }
    if (projMat) env->DeleteLocalRef(projMat);
    if (viewMat) env->DeleteLocalRef(viewMat);
    if (env->ExceptionCheck()) env->ExceptionClear();


    if (gr) env->DeleteLocalRef(gr);

    env->DeleteLocalRef(mc);

    // Cache for the remainder of this frame (see top of function).
    s_cached = data;
    s_cachedFrame = frame;
    return data;
}

std::string CMinecraft::getServerIp() {
    std::string result = "singleplayer";
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return result;
    jobject mc = getInstance();
    if (!mc) return result;

    static jmethodID s_getServer = nullptr;
    if (!s_getServer) {
        jclass mcC = env->GetObjectClass(mc);
        s_getServer = env->GetMethodID(mcC, Mappings::MC_getCurrentServer, Mappings::MC_getCurrentServer_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_getServer = nullptr; }
        env->DeleteLocalRef(mcC);
    }

    if (s_getServer) {
        jobject sd = env->CallObjectMethod(mc, s_getServer);
        if (sd && !env->ExceptionCheck()) {
            jclass sdC = env->GetObjectClass(sd);
            static jfieldID s_ip = nullptr;
            if (!s_ip) {
                s_ip = env->GetFieldID(sdC, Mappings::ServerData_ip, Mappings::ServerData_ip_Sig);
                if (env->ExceptionCheck()) { env->ExceptionClear(); s_ip = nullptr; }
            }
            if (s_ip) {
                jstring jip = (jstring)env->GetObjectField(sd, s_ip);
                if (jip) {
                    result = JvmWrapper::jstringToString(jip);
                    env->DeleteLocalRef(jip);
                }
            }
            env->DeleteLocalRef(sdC);
            env->DeleteLocalRef(sd);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    env->DeleteLocalRef(mc);
    return result;
}

long long CMinecraft::getWorldSeed() {
    // Struktur-Positionen werden aus dem World-Seed berechnet (Structure ESP).
    // Level.getSeed() ist auf WorldGenLevel deklariert (von Level/ClientLevel geerbt).
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return 0;

    static jclass s_levelClass = nullptr;
    static jmethodID s_getSeed = nullptr;
    if (!s_levelClass) {
        s_levelClass = JvmWrapper::findClass(Mappings::Level_Class);
        if (!s_levelClass) return 0;
        s_getSeed = env->GetMethodID(s_levelClass, Mappings::Level_getSeed, Mappings::Level_getSeed_Sig);
        if (env->ExceptionCheck()) { env->ExceptionClear(); s_getSeed = nullptr; }
    }
    if (!s_getSeed) return 0;

    jobject world = getWorld();
    if (!world) return 0;
    jlong seed = env->CallLongMethod(world, s_getSeed);
    if (env->ExceptionCheck()) { env->ExceptionClear(); seed = 0; }
    env->DeleteLocalRef(world);
    return (long long)seed;
}

std::string CMinecraft::getDimension() {
    // Reads Level.dimension (ResourceKey<Level> field) → .location() → .getPath()
    // Returns "overworld" / "the_nether" / "the_end" or empty on error.
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return {};

    static jclass    s_levelCls = nullptr;
    static jfieldID  s_dimField = nullptr;
    static jclass    s_rkCls    = nullptr;
    static jmethodID s_locMeth  = nullptr;
    static jclass    s_idCls    = nullptr;
    static jmethodID s_pathMeth = nullptr;
    static bool s_tried = false;

    if (!s_tried) {
        s_tried = true;
        s_levelCls = JvmWrapper::findClass(Mappings::Level_Class);
        if (s_levelCls) {
            s_dimField = env->GetFieldID(s_levelCls, Mappings::Level_dimension, Mappings::Level_dimension_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_dimField = nullptr; }
        }
        s_rkCls = JvmWrapper::findClass(Mappings::ResourceKey_Class);
        if (s_rkCls) {
            s_locMeth = env->GetMethodID(s_rkCls, Mappings::ResourceKey_location, Mappings::ResourceKey_location_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_locMeth = nullptr; }
        }
        s_idCls = JvmWrapper::findClass(Mappings::Identifier_Class);
        if (s_idCls) {
            s_pathMeth = env->GetMethodID(s_idCls, Mappings::Identifier_getPath, Mappings::Identifier_getPath_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); s_pathMeth = nullptr; }
        }
    }
    if (!s_levelCls || !s_dimField || !s_rkCls || !s_locMeth || !s_idCls || !s_pathMeth)
        return {};

    jobject world = getWorld();
    if (!world) return {};
    jobject dimKey = env->GetObjectField(world, s_dimField);
    env->DeleteLocalRef(world);
    if (!dimKey || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); return {}; }
    jobject ident = env->CallObjectMethod(dimKey, s_locMeth);
    env->DeleteLocalRef(dimKey);
    if (!ident || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); return {}; }
    jstring path = (jstring)env->CallObjectMethod(ident, s_pathMeth);
    env->DeleteLocalRef(ident);
    if (!path || env->ExceptionCheck()) { if (env->ExceptionCheck()) env->ExceptionClear(); return {}; }
    std::string result = JvmWrapper::jstringToString(path);
    env->DeleteLocalRef(path);
    return result;
}
