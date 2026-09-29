#include "pch.h"
#include "core/strcrypt.h"
#include "modules/xray.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "render/esp_renderer.h"
#include "config/mappings.h"

XRay::XRay() : Module("X-Ray", ModuleCategory::RENDER, 0, "Reveals ores through walls — configurable ores and radius") {
    defineBool("diamond",  "Diamond",   true);
    defineBool("iron",     "Iron",      true);
    defineBool("gold",     "Gold",      true);
    defineBool("coal",     "Coal",      false);
    defineBool("emerald",  "Emerald",   true);
    defineBool("lapis",    "Lapis",     false);
    defineBool("redstone", "Redstone",  false);
    defineBool("netherite","Netherite", true);
    defineBool("spawner",  "Spawner",   true);
    defineFloat("radius",    "Radius",    30.0f, 5.0f, 60.0f, "%.0f");
    defineFloat("thickness", "Thickness", 1.0f, 0.5f, 5.0f, "%.1f");
    addSearchTag("wallhack");
    addSearchTag("ores");
}

void XRay::onEnable()  { m_running = true; m_blocks.clear(); m_searchThread = std::thread(&XRay::searchLoop, this); }
void XRay::onDisable() { m_running = false; if (m_searchThread.joinable()) m_searchThread.join();
                          std::lock_guard<std::mutex> lk(m_mutex); m_blocks.clear(); }

void XRay::onRender() {
    if (!m_enabled) return;
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;
    auto cam = CMinecraft::getCameraData();
    if (!cam.valid) return;

    EspRenderer::setFOV(cam.fov);
    EspRenderer::setMatrices(cam.hasMatrices, cam.viewMatrix, cam.projMatrix);
    EspRenderer::updateCamera(cam.x, cam.y, cam.z, cam.yaw, cam.pitch,
                               EspRenderer::getScreenW(), EspRenderer::getScreenH());

    std::vector<BlockData> blocks;
    { std::lock_guard<std::mutex> lk(m_mutex); blocks = m_blocks; }

    float thick = m_floatSettings["thickness"];
    for (auto& b : blocks) {
        ImU32 c = IM_COL32(255,255,255,255);
        if (b.name.find("diamond")!=std::string::npos) c=IM_COL32(0,255,255,255);
        else if(b.name.find("gold")!=std::string::npos) c=IM_COL32(255,215,0,255);
        else if(b.name.find("iron")!=std::string::npos) c=IM_COL32(255,200,150,255);
        else if(b.name.find("emerald")!=std::string::npos) c=IM_COL32(0,255,0,255);
        else if(b.name.find("redstone")!=std::string::npos) c=IM_COL32(255,0,0,255);
        else if(b.name.find("lapis")!=std::string::npos) c=IM_COL32(0,0,255,255);
        else if(b.name.find("coal")!=std::string::npos) c=IM_COL32(50,50,50,255);
        else if(b.name.find("debris")!=std::string::npos) c=IM_COL32(100,0,0,255);
        else if(b.name.find("spawner")!=std::string::npos) c=IM_COL32(255,100,100,255);
        EspRenderer::draw3DBox(b.x, b.y, b.z, b.x+1.0, b.y+1.0, b.z+1.0, c, thick);
    }
}

void XRay::searchLoop() {
    JNIEnv* env = JvmWrapper::getEnv();
    if (!env) return;

    jclass mPosCls = JvmWrapper::findClass(Mappings::MutableBlockPos_Class);
    jmethodID initP = JvmWrapper::getMethodID(mPosCls, Mappings::MutableBlockPos_Init, Mappings::MutableBlockPos_Init_Sig);
    jmethodID setP  = JvmWrapper::getMethodID(mPosCls, Mappings::MutableBlockPos_set, Mappings::MutableBlockPos_set_Sig);
    jclass lvlCls   = JvmWrapper::findClass(Mappings::ClientLevel_Class);
    jmethodID getState = JvmWrapper::getMethodID(lvlCls, Mappings::Level_getBlockState, Mappings::Level_getBlockState_Sig);
    jclass stCls    = JvmWrapper::findClass(Mappings::BlockState_Class);
    jmethodID getBlock = JvmWrapper::getMethodID(stCls, Mappings::BlockState_getBlock, Mappings::BlockState_getBlock_Sig);
    jclass blkCls   = JvmWrapper::findClass(Mappings::Block_Class);
    jmethodID getName = JvmWrapper::getMethodID(blkCls, Mappings::Block_getName, Mappings::Block_getName_Sig);
    jclass compCls  = JvmWrapper::findClass(Mappings::Component_Class);
    jmethodID compStr = compCls ? JvmWrapper::getMethodID(compCls, Mappings::Component_getString, Mappings::Component_getString_Sig) : nullptr;

    if (!initP || !setP || !getState || !getBlock || !getName || !compStr) return;

    jobject pos = env->NewObject(mPosCls, initP, 0, 0, 0);
    jobject gRef = env->NewGlobalRef(pos); env->DeleteLocalRef(pos);

    while (m_running) {
        env->PushLocalFrame(256);
        auto cam = CMinecraft::getCameraData();
        // Snapshot settings under the mutex: the menu thread (hook thread)
        // writes m_floatSettings/m_boolSettings concurrently — reading an
        // unordered_map from another thread while it is modified is UB and
        // can crash (data race).
        int r;
        bool d_diamond, d_iron, d_gold, d_coal, d_emerald, d_lapis, d_redstone, d_netherite, d_spawner;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            r = (int)m_floatSettings["radius"];
            d_diamond   = m_boolSettings["diamond"];
            d_iron      = m_boolSettings["iron"];
            d_gold      = m_boolSettings["gold"];
            d_coal      = m_boolSettings["coal"];
            d_emerald   = m_boolSettings["emerald"];
            d_lapis     = m_boolSettings["lapis"];
            d_redstone  = m_boolSettings["redstone"];
            d_netherite = m_boolSettings["netherite"];
            d_spawner   = m_boolSettings["spawner"];
        }
        std::vector<BlockData> newBlocks;
        // Fetch world ONCE — doing getWorld() inside the triple loop
        // creates ~2000 leaked local refs per search pass.
        jobject world = CMinecraft::getWorld();
        int cx = (int)(cam.x), cy = (int)(cam.y), cz = (int)(cam.z);
        for (int x=cx-r; x<=cx+r && m_running; x++)
            for (int z=cz-r; z<=cz+r && m_running; z++)
                for (int y=cy-r; y<=cy+r && m_running; y++) {
                    if (y<-64||y>320) continue;
                    env->PushLocalFrame(16);
                    env->CallObjectMethod(gRef, setP, x, y, z);
                    jobject st = env->CallObjectMethod(world, getState, gRef);
                    if (st && !env->ExceptionCheck()) {
                        jobject blk = env->CallObjectMethod(st, getBlock);
                        if (blk && !env->ExceptionCheck()) {
                            jobject comp = env->CallObjectMethod(blk, getName);
                            if (comp && !env->ExceptionCheck()) {
                                jstring js = (jstring)env->CallObjectMethod(comp, compStr);
                                if (js && !env->ExceptionCheck()) {
                                    std::string n = JvmWrapper::jstringToString(js);
                                    if (n.find("ore")!=std::string::npos||n.find("debris")!=std::string::npos||n.find("spawner")!=std::string::npos) {
                                        bool add = false;
                                        if (d_diamond && n.find("diamond")!=std::string::npos) add=true;
                                        else if(d_iron && n.find("iron")!=std::string::npos) add=true;
                                        else if(d_gold && n.find("gold")!=std::string::npos) add=true;
                                        else if(d_coal && n.find("coal")!=std::string::npos) add=true;
                                        else if(d_emerald && n.find("emerald")!=std::string::npos) add=true;
                                        else if(d_lapis && n.find("lapis")!=std::string::npos) add=true;
                                        else if(d_redstone && n.find("redstone")!=std::string::npos) add=true;
                                        else if(d_netherite && n.find("debris")!=std::string::npos) add=true;
                                        else if(d_spawner && n.find("spawner")!=std::string::npos) add=true;
                                        if (add) { size_t dot=n.find_last_of('.'); if(dot!=std::string::npos)n=n.substr(dot+1); newBlocks.push_back({x,y,z,n}); }
                                    }
                                }
                            }
                        }
                    }
                    env->PopLocalFrame(nullptr);
                }
        if (world) env->DeleteLocalRef(world);
        if (m_running) { std::lock_guard<std::mutex> lk(m_mutex); m_blocks.swap(newBlocks); }
        env->PopLocalFrame(nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    env->DeleteGlobalRef(gRef);

    // Detach cleanly so repeated XRay toggles don't leave a pile of attached
    // JavaThreads behind (each one owns a JNI ref table).
    if (JavaVM* vm = JvmWrapper::getVM()) vm->DetachCurrentThread();
}
