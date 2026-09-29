#include "pch.h"
#include "core/strcrypt.h"
#include "modules/spammer.h"
#include "core/jvm_wrapper.h"
#include <string>

Spammer::Spammer() : Module("Spammer", ModuleCategory::MISC, 0, "Sends chat messages on a loop") { setTickInterval(1);
    m_floatSettings["delay"]=3000.0f; m_displayNames["delay"]="Delay (ms)";
}
void Spammer::onUpdate(JNIEnv* env) {
    if(!m_enabled)return;
    auto now=std::chrono::steady_clock::now();
    long long ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    // Guard — only start a new message after the full delay
    if(m_spamState==0 && ms-m_last < m_floatSettings["delay"]) return;

    const char* msgs[]={"gg","Draxo on top","lol","nice try","eZ"};
    static std::string s_msg;
    static int s_msgIdx=0;

    switch(m_spamState) {
    case 0: // Open chat
        m_last=ms;
        s_msg=msgs[rand()%5]; s_msgIdx=0;
        keybd_event('T',0,0,0); keybd_event('T',0,KEYEVENTF_KEYUP,0);
        m_spamState=1; break;
    case 1: // Type one char per tick (no Sleep!)
        if(s_msgIdx<(int)s_msg.size()){
            SHORT vk=VkKeyScanA(s_msg[s_msgIdx]);
            keybd_event((BYTE)vk,0,0,0);
            keybd_event((BYTE)vk,0,KEYEVENTF_KEYUP,0);
            s_msgIdx++;
        }else{keybd_event(VK_RETURN,0,0,0);keybd_event(VK_RETURN,0,KEYEVENTF_KEYUP,0);m_spamState=0;}
        break;
    }
}
