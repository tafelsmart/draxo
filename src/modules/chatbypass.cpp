#include "pch.h"
#include "core/strcrypt.h"
#include "modules/chatbypass.h"
#include "sdk/minecraft.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

ChatBypass::ChatBypass() : Module("ChatBypass", ModuleCategory::MISC, 0,
    "Built-in command sender — type commands and send them directly") {
    addSearchTag("chat");
    addSearchTag("command");
    addSearchTag("send");
    addSearchTag("lobby");
}

/* ──────────────────────────────────────────────────────────────────
 *  Render: draw the text input + Send button at bottom-center
 * ────────────────────────────────────────────────────────────────── */
void ChatBypass::onRender() {
    if (!m_enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    float ww = io.DisplaySize.x;
    float wh = io.DisplaySize.y;

    // Position: bottom center, above the crosshair area
    float pw = 380.0f;  // panel width
    float ph = 52.0f;   // panel height
    float px = (ww - pw) * 0.5f;
    float py = wh - ph - 20.0f;  // 20px from bottom

    ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.02f, 0.10f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.55f, 0.40f, 0.90f, 0.40f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.88f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.16f, 0.48f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.18f, 0.50f, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.44f, 0.28f, 0.70f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.54f, 0.38f, 0.86f, 0.60f));

    ImGui::Begin("##cmdSender", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing);

    // Label
    ImGui::TextUnformatted("Command:");

    // Text input (takes most of the width)
    ImGui::SetNextItemWidth(pw - 120.0f);
    bool enterPressed = ImGui::InputText("##cmdInput", s_inputBuf, sizeof(s_inputBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    // Send button
    ImGui::SameLine();
    if (ImGui::Button("Send", ImVec2(70, 24)) || enterPressed) {
        if (s_inputBuf[0]) {
            s_pending = true;  // will be sent in next onUpdate
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(3);
}

/* ──────────────────────────────────────────────────────────────────
 *  Update: send the pending command via JNI (game thread — safe)
 * ────────────────────────────────────────────────────────────────── */
void ChatBypass::onUpdate(JNIEnv* env) {
    if (!m_enabled || !s_pending) return;
    s_pending = false;

    std::string msg(s_inputBuf);
    if (msg.empty()) return;

    send(env, msg);

    // Clear input after sending
    memset(s_inputBuf, 0, sizeof(s_inputBuf));
}

void ChatBypass::send(JNIEnv* env, const std::string& msg) {
    // ── Get Minecraft instance ─────────────────────────────────────
    jobject mc = CMinecraft::getInstance();
    if (!mc) return;

    // ── Get the connection (ClientPacketListener) ──────────────────
    static jmethodID s_getConn = nullptr;
    if (!s_getConn) {
        jclass mcCls = env->GetObjectClass(mc);
        s_getConn = env->GetMethodID(mcCls,
            Mappings::MC_getConnection,
            Mappings::MC_getConnection_Sig);
        env->DeleteLocalRef(mcCls);
    }
    if (!s_getConn) { env->DeleteLocalRef(mc); return; }

    jobject conn = env->CallObjectMethod(mc, s_getConn);
    if (!conn || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mc);
        return;
    }

    // ── Choose sendChat or sendCommand based on '/' prefix ─────────
    // ClientCommonPacketListenerImpl has:
    //   void sendChat(String)     → sends a chat message
    //   void sendCommand(String)  → sends a command (with / prefix)
    bool isCommand = (!msg.empty() && msg[0] == '/');

    jclass connCls = env->GetObjectClass(conn);
    jmethodID sendMethod = nullptr;

    if (isCommand) {
        sendMethod = env->GetMethodID(connCls,
            Mappings::CCPLI_sendCommand, Mappings::CCPLI_sendCommand_Sig);
    } else {
        sendMethod = env->GetMethodID(connCls,
            Mappings::CCPLI_sendChat, Mappings::CCPLI_sendChat_Sig);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        sendMethod = nullptr;
    }

    if (sendMethod) {
        jstring jMsg = env->NewStringUTF(msg.c_str());
        env->CallVoidMethod(conn, sendMethod, jMsg);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(jMsg);

        printf(STR_C("[Draxo] ChatBypass: sent '%s'\n"), msg.c_str());
    }

    env->DeleteLocalRef(connCls);
    env->DeleteLocalRef(conn);
    env->DeleteLocalRef(mc);
}
