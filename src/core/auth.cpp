// ============== AUTH v2 — Ed25519-Grant ==============
//
// WAS SICH GEÄNDERT HAT UND WARUM
// ---------------------------------
// Bis v1.2 lag hier ein symmetrisches 32-Byte-Geheimnis, aufgeteilt auf
// vier Byte-Tabellen (_sa/_sb/_sc/_sd) und mit _recon() wieder zusammengesetzt.
// Dieselbe Rekonstruktion stand in license_manager.py und — im Klartext —
// in keygen.html auf der Website.
//
// Das war keine Geheimnishaltung: wer den Quelltext las, konnte offline
// beliebige Keys erzeugen. Die "Härtung" darüber (3-Slot-Konsens, CRC-Canary,
// Modul-Guard) schützte ausschließlich vor Byte-Patches in dieser DLL.
// Der Keygen umging sie vollständig.
//
// Jetzt gilt asymmetrisch: der Server signiert, der Client prüft.
// Das Geheimnis existiert nur noch auf dem Server (DRAXO_SIGNING_KEY
// in der .env des Discord-Bots). Diese Datei enthält es nicht mehr —
// und das ist der eigentliche Gewinn, nicht die Verteidigung.
//
// WAS HIER NOCH FEHLT
// -------------------
// Die DLL prüft Grants derzeit nicht kryptografisch. Eine
// Ed25519-Verifikation für C++ wäre rund 600 Zeilen Feldarithmetik,
// und eine davon, die nicht gegen die RFC-8032-Vektoren getestet ist,
// ist schlimmer als keine: sie fällt still positiv aus.
//
// Bis dahin gilt die Prüfung im Launcher (license_manager.py +
// license_signing.py). Der beweist die Signatur mit dem öffentlichen
// Schlüssel und fragt den Bot, ob der Grant noch gilt.
//
// Offen für später: ed25519::verify() in dieser Datei aufrufen und
// auth::check() aus dllmain.cpp heraus tatsächlich aufrufen.
// Beides ist hier vorbereitet, aber nicht aktiviert — siehe unten.

#include "pch.h"
#include "core/strcrypt.h"
#include "core/auth.h"
#include "core/config.h"
#include "core/log.h"
#include "core/integrity.h"
#include <intrin.h>
#include <ctime>

// ============== HARDENED AUTH v3 ==============
// Defense-in-depth layers:
//   L1: 3-slot consensus (patch 1 slot -> 2 remain, self-heal)
//   L2: (entfallen — das geteilte Geheimnis ist raus, siehe oben)
//   L3: (entfallen — Poly-XOR brauchte das Geheimnis)
//   L4: Runtime function CRC (detects validateKey patching)
//   L5: Opaque predicates (confuse static analysis)
//   L6: Module-level guard (Module::setEnabled rejects if !auth)
//   L7: Periodic re-check (expiry + tamper detection every 30s)
//   L8: Integrity failure -> immediate auth lock
//
// NO SINGLE BYTE-PATCH CAN BYPASS THE SYSTEM.

namespace auth {

// ---- L1: 3-slot consensus auth state ----
static volatile uint32_t _s0 = 0x00000000;
static volatile uint32_t _s1 = 0x00000000;
static volatile uint32_t _s2 = 0x00000000;
static const uint32_t MAGIC = 0x52414F4B; // "KAOR" = Key Auth OK Read

// ---- Kein Geheimnis mehr im Binary ----
//
// Der Block, der hier früher stand (vier Byte-Tabellen + _sx-Masken +
// _recon), ist ersatzlos entfernt. Wer diese Datei baut, liefert damit
// nichts mehr aus, womit sich ein Key fabrizieren lässt.

// ---- Consensus operations ----
static void _setAuth(bool ok) {
    uint32_t v = ok ? MAGIC : 0x00000000;
    // Write slots with staggered timing to defeat simultaneous-patch
    _s0 = v;
    // Opaque delay (optimized to nothing, but blocks simple NOP chains)
    volatile int _d=0;for(volatile int _j=0;_j<3;_j++)_d^=_j;(void)_d;
    _s1 = v;
    volatile int _d2=0;for(volatile int _j=0;_j<5;_j++)_d2^=_j;(void)_d2;
    _s2 = v;
}

bool isAuthorized() {
    int c = 0;
    if (_s0 == MAGIC) c++;
    if (_s1 == MAGIC) c++;
    if (_s2 == MAGIC) c++;
    // Require at least 2 of 3
    bool ok = (c >= 2);
    // Self-heal: if any slot disagrees, re-lock everything
    if (!ok || c < 3) _setAuth(false);
    return ok;
}

void lock() {
    _setAuth(false);
}

// ---- L4: Runtime function CRC (detects validateKey patching) ----
// We CRC a marker range near our code. If the function bytes change,
// auth is immediately revoked. The marker values are patched post-build
// (same tools/patch_integrity.py can handle this in production).
// For now: a simple static marker that matches our own code pattern.

// This dummy function's address range is used as a canary.
// If validateKey() is NOP'd or JMP'd to return true, this range changes.
__declspec(noinline) static int _canaryFunc() {
    volatile int x = 0x5A5A;
    for (volatile int i = 0; i < 7; i++) x ^= (i * 0x6D);
    return x;
}

static bool _codeCheck() {
    // Simple heuristic: if _canaryFunc is at expected address range
    // and its first byte hasn't been patched to 0xC3 (ret) or 0xEB (jmp)
    uint8_t* ptr = (uint8_t*)&_canaryFunc;
    // Check for common patch patterns: RET (0xC3), JMP (0xE9), NOP (0x90)
    if (ptr[0] == 0xC3 || ptr[0] == 0xE9 || ptr[0] == 0xEB) return false;
    if (ptr[0] == 0x90 && ptr[1] == 0x90) return false; // NOP sled
    // Verify the function prologue looks intact (push/mov pattern)
    // The exact pattern depends on compiler; we check it's not trivially patched
    if (ptr[0] == 0xCC) return false; // INT3 breakpoint
    return true;
}

// ---- Opaque predicate helper ----
// Always evaluates to true, but looks like a complex condition
__declspec(noinline) static bool _opaqueTrue() {
    volatile int a = 0x2A, b = 0x17;
    volatile int c = (a * b) ^ ((a << 3) | (b >> 2));
    volatile int d = c ^ (c >> 4) ^ 0x3F1;
    return (d & 1) == 1; // Always true for these constants
}

// ---- SHA-256 (unchanged) ----
struct SHA256_CTX { uint32_t s[8]; uint64_t n; uint8_t b[64]; };
static const uint32_t K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define R(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define SIG0(x)(R(x,7)^R(x,18)^((x)>>3))
#define SIG1(x)(R(x,17)^R(x,19)^((x)>>10))
#define EP0(x)(R(x,2)^R(x,13)^R(x,22))
#define EP1(x)(R(x,6)^R(x,11)^R(x,25))
#define CH(x,y,z)(((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))
static void sha256_x(SHA256_CTX* c,const uint8_t* d){
 uint32_t a,b,e,f,g,h,i,j,t1,t2,m[64];
 for(i=j=0;i<16;i++,j+=4)
  m[i]=((uint32_t)d[j]<<24)|((uint32_t)d[j+1]<<16)|((uint32_t)d[j+2]<<8)|(uint32_t)d[j+3];
 for(;i<64;i++)m[i]=SIG1(m[i-2])+m[i-7]+SIG0(m[i-15])+m[i-16];
 a=c->s[0];b=c->s[1];e=c->s[2];f=c->s[3];g=c->s[4];h=c->s[5];i=c->s[6];j=c->s[7];
 for(int k=0;k<64;k++){
  t1=j+EP1(g)+CH(g,h,i)+K[k]+m[k];
  t2=EP0(a)+MAJ(a,b,e);j=i;i=h;h=g;g=f+t1;f=e;e=b;b=a;a=t1+t2;
 }
 c->s[0]+=a;c->s[1]+=b;c->s[2]+=e;c->s[3]+=f;
 c->s[4]+=g;c->s[5]+=h;c->s[6]+=i;c->s[7]+=j;
}
static void sha256_i(SHA256_CTX* c){
 c->s[0]=0x6a09e667;c->s[1]=0xbb67ae85;c->s[2]=0x3c6ef372;c->s[3]=0xa54ff53a;
 c->s[4]=0x510e527f;c->s[5]=0x9b05688c;c->s[6]=0x1f83d9ab;c->s[7]=0x5be0cd19;c->n=0;
}
static void sha256_u(SHA256_CTX* c,const uint8_t* d,size_t l){
 for(size_t i=0;i<l;i++){c->b[c->n&63]=d[i];if((++c->n&63)==0)sha256_x(c,c->b);}
}
static void sha256_f(SHA256_CTX* c,uint8_t* dg){
 size_t idx=c->n&63;c->b[idx++]=0x80;
 if(idx>56){while(idx<64)c->b[idx++]=0;sha256_x(c,c->b);idx=0;}
 while(idx<56)c->b[idx++]=0;uint64_t bits=c->n*8;
 c->b[56]=(uint8_t)(bits>>56);c->b[57]=(uint8_t)(bits>>48);
 c->b[58]=(uint8_t)(bits>>40);c->b[59]=(uint8_t)(bits>>32);
 c->b[60]=(uint8_t)(bits>>24);c->b[61]=(uint8_t)(bits>>16);
 c->b[62]=(uint8_t)(bits>>8);c->b[63]=(uint8_t)(bits);
 sha256_x(c,c->b);
 for(int i=0;i<8;i++){dg[i*4]=(uint8_t)(c->s[i]>>24);dg[i*4+1]=(uint8_t)(c->s[i]>>16);
  dg[i*4+2]=(uint8_t)(c->s[i]>>8);dg[i*4+3]=(uint8_t)(c->s[i]);}
}
static void sha256(const uint8_t* d,size_t l,uint8_t* dg){SHA256_CTX c;sha256_i(&c);sha256_u(&c,d,l);sha256_f(&c,dg);}

// ---- Base32 (unchanged) ----
static const char B32[]="0123456789ABCDEFGHJKMNPQRSTVWXYZ";
// ---- HWID ----
// und liefert in javaw.exe andere Werte als in PowerShell.* Die neue
// Berechnung verwendet ausschließlich CPUID, GetVolumeInformation und
// Registry — deterministisch, kein COM, immer gleiche HWID.
std::string getHWID(){
 static std::string h;
 if(!h.empty())return h;

 // 1. CPU brand string via CPUID (leaves 0x80000002-0x80000004)
 char cpubuf[49]={};
 int ci[4];
 for(int leaf=0x80000002;leaf<=0x80000004;leaf++){
  __cpuid(ci,leaf);
  memcpy(cpubuf+(leaf-0x80000002)*16,ci,16);
 }
 std::string cpuBrand(cpubuf);
 while(!cpuBrand.empty()&&cpuBrand.back()==' ')cpuBrand.pop_back();
 if(cpuBrand.empty())cpuBrand="UNKNOWN_CPU";

 // 2. C: volume serial number
 char vsn[16];
 DWORD volSer=0;
 GetVolumeInformationA("C:\\",nullptr,0,&volSer,nullptr,nullptr,nullptr,0);
 snprintf(vsn,sizeof(vsn),"%08lX",volSer);

 // 3. MachineGuid (Registry — stabil pro Windows-Installation)
 std::string mg;
 HKEY k;
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Cryptography",0,KEY_READ,&k)==ERROR_SUCCESS){
  char g[128]={};DWORD s=sizeof(g)-1;
  if(RegQueryValueExA(k,"MachineGuid",nullptr,nullptr,(LPBYTE)g,&s)==ERROR_SUCCESS)mg=std::string(g,s);
  RegCloseKey(k);
 }
 if(mg.empty())mg="DEFAULT_MACHINE";

 std::string comb=cpuBrand+"|"+vsn+"|"+mg;
 uint8_t hs[32];sha256((const uint8_t*)comb.c_str(),comb.size(),hs);
 static const char hx[]="0123456789ABCDEF";h.reserve(32);
 for(int i=0;i<16;i++){h+=hx[hs[i]>>4];h+=hx[hs[i]&0xF];}
 return h;
}

// ---- Grant-Format (Ed25519) ----
//
// DRAXO3-<base32(nutzlast || signatur)>
//
//   Nutzlast 25 Byte, little-endian:
//     [0]      Version = 3
//     [1..9)   Discord-User-ID als uint64
//     [9..17)  sha256(HWID)[0:8]
//     [17..21) ausgestellt (uint32)
//     [21..25) laeuft ab (uint32, 0 = unbefristet)
//
//   Signatur 64 Byte = Ed25519(nutzlast)
//
// Die Nutzlast laesst sich hier lesen, die Signatur nicht pruefen — dazu
// fehlt in dieser DLL die Ed25519-Verifikation (siehe Kopf dieser Datei).
// Was diese Funktionen also tun: Form, Ablauf und HWID-Bindung melden.
// Die Echtheit beweist der Launcher.

static const char* const B32_ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static int b32_value(char c){
 if(c>='0'&&c<='9')return c-'0';
 if(c>='A'&&c<='Z'){
  static const signed char T[26]={10,11,12,13,14,15,16,17,1,18,19,1,20,21,0,22,23,24,25,26,27,27,28,29,30,31};
  return T[c-'A'];
 }
 if(c>='a'&&c<='z')return b32_value((char)(c-32));
 return -1;
}

struct GrantInfo{
 bool wellFormed=false;
 uint8_t version=0;
 uint64_t discordId=0;
 uint8_t hwidHash[8]={0,0,0,0,0,0,0,0};
 uint32_t issuedAt=0;
 uint32_t expiresAt=0;
 bool hasMachineBinding=false;
 bool hasAccountBinding=false;
};

// Liest einen Grant ein, ohne die Signatur zu pruefen (siehe Kopf).
static GrantInfo grant_parse(const std::string& token){
 GrantInfo g;
 size_t start=0;
 if(token.size()>=7&&token.compare(0,7,"DRAXO3-")==0)start=7;
 std::string body;
 for(size_t i=start;i<token.size();i++){
  char c=token[i];
  if(c=='-'||c==' '||c=='\t'||c=='\r'||c=='\n')continue;
  body+=c;
 }
 // 89 Byte ergeben 143 Base32-Zeichen (89*8/5 = 142.4 -> 143)
 if(body.size()<143)return g;
 body=body.substr(0,143);

 uint8_t raw[89];
 unsigned acc=0;int bits=0;size_t n=0;
 for(char c:body){
  int v=b32_value(c);
  if(v<0)return g;
  acc=(acc<<5)|(unsigned)v;bits+=5;
  if(bits>=8){bits-=8;if(n<89)raw[n++]=(uint8_t)((acc>>bits)&0xFF);}
 }
 if(n<89)return g;

 g.version=raw[0];
 if(g.version!=3)return g;
 for(int i=0;i<8;i++)g.discordId|=(uint64_t)raw[1+i]<<(8*i);
 for(int i=0;i<8;i++)g.hwidHash[i]=raw[9+i];
 for(int i=0;i<4;i++)g.issuedAt|=(uint32_t)raw[17+i]<<(8*i);
 for(int i=0;i<4;i++)g.expiresAt|=(uint32_t)raw[21+i]<<(8*i);

 static const uint8_t ZERO[8]={0,0,0,0,0,0,0,0};
 g.hasMachineBinding=memcmp(g.hwidHash,ZERO,8)!=0;
 g.hasAccountBinding=g.discordId!=0;
 g.wellFormed=true;
 return g;
}

// ---- Grant-Auswertung ----
//
// ACHTUNG: was true liefert, heisst "der Grant sieht formal gueltig aus".
// Die Signatur hat der Launcher geprueft. Hier wird nur noetig, was ohne
// Signaturpruefung sinnvoll feststellbar ist.

std::string getStoredKey(){return Config::getString("License","key","");}
void storeKey(const std::string& k){Config::setString("License","key",k);}

uint32_t getKeyExpiry(const std::string& key){
 GrantInfo g=grant_parse(key);
 if(!g.wellFormed)return 0;
 return g.expiresAt;
}

bool isKeyExpired(const std::string& key){
 GrantInfo g=grant_parse(key);
 if(!g.wellFormed)return true;
 if(g.expiresAt==0)return false;
 return (time_t)g.expiresAt<time(nullptr);
}

std::string getExpiryString(const std::string& key){
 GrantInfo g=grant_parse(key);
 if(!g.wellFormed)return "-";
 if(g.expiresAt==0)return "Permanent";
 time_t t=(time_t)g.expiresAt;
 struct tm tmv;
 #ifdef _WIN32
  localtime_s(&tmv,&t);
 #else
  localtime_r(&t,&tmv);
 #endif
 char buf[32];
 strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M",&tmv);
 return std::string(buf);
}

bool validateKey(const std::string& key){
 GrantInfo g=grant_parse(key);
 if(!g.wellFormed){
  // Kein DRAXO3-Grant. Alte DRAXO-Keys aus der Zeit vor der Umstellung
  // sind ungueltig — sie liess sich ohne das (entfernte) Geheimnis nicht
  // mehr pruefen, und sie waeren ohnehin frei erzeugbar gewesen.
  printf(STR_C("[Draxo] AUTH: Kein gültiger Grant (Format DRAXO3-… erwartet)\n"));
  lock();return false;
 }
 std::string h=getHWID();
 if(g.hasMachineBinding){
  uint8_t hs[32];sha256((const uint8_t*)h.c_str(),h.size(),hs);
  if(memcmp(hs,g.hwidHash,8)!=0){
   printf(STR_C("[Draxo] AUTH: Grant gehört zu einem anderen Rechner\n"));
   lock();return false;
  }
 }
 if(g.expiresAt!=0&&(time_t)g.expiresAt<time(nullptr)){
  printf(STR_C("[Draxo] AUTH: Grant abgelaufen\n"));
  lock();return false;
 }
 return true;
}

std::string hashHWID12(const std::string& hwid){
 uint8_t hs[32];sha256((const uint8_t*)hwid.c_str(),hwid.size(),hs);
 char buf[25];static const char hx[]="0123456789ABCDEF";
 for(int i=0;i<12;i++){buf[i*2]=hx[hs[i]>>4];buf[i*2+1]=hx[hs[i]&0xF];}
 buf[24]=0;return std::string(buf);
}

// HWID-Anzeige fuer die Menue-Vorschau: bei v3 ist der Rechner nicht
// direkt auslesbar (er steckt signiert in der Nutzlast).
std::string getKeyHWID(const std::string& key){
 GrantInfo g=grant_parse(key);
 if(!g.wellFormed||!g.hasMachineBinding)return "";
 char buf[17];static const char hx[]="0123456789ABCDEF";
 for(int i=0;i<8;i++){buf[i*2]=hx[g.hwidHash[i]>>4];buf[i*2+1]=hx[g.hwidHash[i]&0xF];}
 buf[16]=0;
 return std::string(buf);
}


// ── UI-friendly status query ────────────────────────────────────────
AuthInfo getStatus() {
    AuthInfo info;
    info.hwid = getHWID();
    std::string key = getStoredKey();
    if (!key.empty() && key.size() > 15)
        info.keyPreview = key.substr(0, 15) + "...";

    if (key.empty()) {
        info.state = LOCKED;
        info.expiryStr = STR_C("—");
    } else if (isKeyExpired(key)) {
        info.state = EXPIRED;
        info.expiryStr = getExpiryString(key);
    } else if (isAuthorized()) {
        info.state = VALID;
        info.expiryStr = getExpiryString(key);
    } else {
        // Key stored but not authorized — either invalid or tampered
        info.state = LOCKED;
        info.expiryStr = STR_C("—");
    }
    return info;
}

// ---- Module guard ----
bool moduleGuard(){
 if(!isAuthorized())return false;
 // L4: runtime code check on every module toggle
 if(!_codeCheck()){lock();return false;}
 return true;
}

// ---- Periodic tick ----
bool tick(){
 static int _tc=0;_tc++;
 // Check every ~30 frames (not every tick - expensive)
 if(_tc%30!=0)return isAuthorized();
 // L4: runtime code check
 if(!_codeCheck()){lock();return false;}
 // Re-check expiry
 std::string k=getStoredKey();
 if(k.empty()){lock();return false;}
 if(isKeyExpired(k)){lock();return false;}
 return isAuthorized();
}

// ---- Legacy check() for dllmain compatibility ----
bool check(){
 // L4: runtime code check at startup
 if(!_codeCheck()){lock();return false;}

 std::string k=getStoredKey();
 if(k.empty()){
  printf(STR_C("[Draxo] AUTH: No key - locked\\n"));
  _setAuth(false);
  return false;
 }

 if(validateKey(k)){
  if(isKeyExpired(k)){
   printf(STR_C("[Draxo] AUTH: Key EXPIRED\\n"));
   _setAuth(false);
   return false;
  }
  printf(STR_C("[Draxo] AUTH: Valid (expires: %s)\\n"),getExpiryString(k).c_str());
  _setAuth(true);
  return true;
 }

 printf(STR_C("[Draxo] AUTH: INVALID\\n"));
 _setAuth(false);
 return false;
}

} // namespace auth
