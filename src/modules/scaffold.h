#pragma once
#include "modules/module.h"
#include <chrono>
#include <random>
#include <string>

/*
 * Scaffold v2 — Modern auto-bridging.
 *
 * Six modes (setting "mode"):
 *   Legit:     classic sneak-release bridging. The camera smoothly aims at
 *              the block being placed (backward bridging look) while a
 *              sneak/release cycle keeps the player on the edge.
 *   Semi:      same 180° rotation, but NO sneaking. Blocks are placed
 *              perfectly under/behind the player so you never fall.
 *   Rage:      no rotation lock. Blocks are placed continuously directly
 *              under the player while you sprint forward.
 *   GodBridge: forward bridging while looking almost straight down at the
 *              feet. A fast crouch-timing cycle (hold/release) glides the
 *              player forward over the freshly placed blocks.
 *   Breezily:  speed bridging — auto-sprint + auto-jump while blocks are
 *              placed directly under the player (you never stop moving).
 *   Eagle:     safe auto-sneak bridging. Looks ahead at a relaxed angle and
 *              holds sneak ONLY at block edges (SafeWalk style), so you
 *              never fall while walking on flat ground stays normal.
 *
 * Extra features:
 *   - Auto block switch: when the current stack runs out, the next block
 *     stack in the hotbar is selected automatically.
 *   - InventoryMove: if the hotbar is empty, block stacks are pulled from
 *     the main inventory into the hotbar via a real container click
 *     (ClickType.SWAP through MultiPlayerGameMode.handleInventoryMouseClick).
 *   - Tower: while jumping, a block is placed directly under the player and
 *     the player jumps — you can climb straight up.
 *   - No blocks: if no block item is available anywhere, the player is held
 *     in place (no falling) until new blocks appear or Scaffold is disabled.
 *   - HUD preview: the currently selected block + stack count is rendered
 *     above the hotbar.
 *
 * All JNI names go through Mappings:: constants so vanilla_builder.py
 * obfuscates them for the target MC version.
 */
class Scaffold : public Module {
public:
    Scaffold();
    void onEnable() override;
    void onDisable() override;
    void onUpdate(JNIEnv* env) override;
    void onRender() override;

private:
    enum class Mode : int { Legit = 0, Semi = 1, Rage = 2, GodBridge = 3, Breezily = 4, Eagle = 5 };

    void smoothRotate(float tYaw, float tPitch, float speed);
    void initJNI(JNIEnv* env);

    // ── Inventory helpers (JNI) ────────────────────────────────────
    jobject getInventory(JNIEnv* env, jobject playerObj);   // Player.inventory
    bool    stackIsBlock(JNIEnv* env, jobject stack);       // instanceof BlockItem && count > 0
    bool    stackIsEmpty(JNIEnv* env, jobject stack);
    int     stackCount(JNIEnv* env, jobject stack);
    std::string stackName(JNIEnv* env, jobject stack);      // ItemStack.getHoverName().getString()
    int     findBlockSlot(JNIEnv* env, jobject inv);        // hotbar 0..8, -1 = none
    int     findInventoryBlockSlot(JNIEnv* env, jobject inv); // main inv 9..35, -1 = none
    bool    selectSlot(JNIEnv* env, jobject inv, int slot);
    bool    blockIsAir(JNIEnv* env, int bx, int by, int bz);

    // ── InventoryMove: real container click (ClickType.SWAP) ─────────
    // Moves inventory index `srcIdx` (9..35) into hotbar slot `dstHotbar` (0..8).
    bool    moveStackToHotbar(JNIEnv* env, jobject playerObj, int srcIdx, int dstHotbar);

    // ── Placement ──────────────────────────────────────────────────
    bool placeBlock();          // true = click DOWN was sent
    bool sendRightDown();       // false when game window is not focused
    void sendRightUp();
    // Echte Block-Platzierung per JNI (useItemOn) — kein Mausklick nötig.
    // Liefert true, wenn das Packet versendet wurde (Rage-Modus).
    bool placeBlockAtJNI(JNIEnv* env, jobject playerObj, int bx, int by, int bz);

    // ── State ──────────────────────────────────────────────────────
    float m_curYaw = 0, m_curPitch = 0;
    bool  m_hasTgt = false;

    // ── Rotation anchor ─────────────────────────────────────────────
    // The movement direction for target selection is derived from
    // m_baseYaw (a FROZEN reference), never from the live camera yaw.
    // Deriving it from the camera yaw creates a feedback loop — the
    // camera rotates, the target block moves, so the camera rotates
    // more (spinning in circles while holding W). The anchor only
    // re-anchors when the player actually changes direction.
    float m_baseYaw = 0;
    bool  m_baseInit = false;
    int   m_prevKeyState = 0;   // last WASD bitmask (W=1, S=2, A=4, D=8)
    std::chrono::steady_clock::time_point m_lastPlace;
    int   m_placeDelay = 100;

    // click phase (down pending → up next tick) — avoids Sleep() in the tick thread
    bool  m_clickDown = false;
    std::chrono::steady_clock::time_point m_clickDownAt;

    // sneak/release cycle (Legit / GodBridge)
    bool  m_sneakDown = false;
    std::chrono::steady_clock::time_point m_lastSneakFlip;

    // sprint hold (Breezily)
    bool  m_sprintDown = false;

    // tower state
    std::chrono::steady_clock::time_point m_lastJump;

    // inventory move cooldown (wait for server sync after a click)
    std::chrono::steady_clock::time_point m_lastInvMove;

    // preview data (filled in onUpdate, drawn in onRender)
    std::string m_previewName;
    int   m_previewCount = 0;
    bool  m_hasBlock = false;
    ImU32 m_previewColor = IM_COL32(120, 120, 120, 255);

    std::mt19937 m_rng{std::random_device{}()};

    // ── Cached JNI IDs ─────────────────────────────────────────────
    static inline bool sjni = false;
    static inline jclass    s_playerCls = nullptr;          // Player (inventory field)
    static inline jfieldID  s_invField  = nullptr;          // Player.inventory
    static inline jclass    s_invCls    = nullptr;          // Inventory
    static inline jmethodID s_getSelected   = nullptr;      // Inventory.getSelectedSlot
    static inline jmethodID s_setSelected   = nullptr;      // Inventory.setSelectedSlot
    static inline jmethodID s_getItem       = nullptr;      // Inventory.getItem(int)
    static inline jclass    s_stackCls      = nullptr;      // ItemStack
    static inline jmethodID s_stackIsEmpty  = nullptr;      // ItemStack.isEmpty
    static inline jmethodID s_stackGetCount = nullptr;      // ItemStack.getCount
    static inline jmethodID s_stackGetItem  = nullptr;      // ItemStack.getItem
    static inline jmethodID s_stackGetHoverName = nullptr;  // ItemStack.getHoverName
    static inline jclass    s_compCls       = nullptr;      // Component
    static inline jmethodID s_compGetString = nullptr;      // Component.getString
    static inline jclass    s_itemCls       = nullptr;      // Item
    static inline jclass    s_blockItemCls  = nullptr;      // BlockItem
    // Block air-check
    static inline jclass    s_bpCls     = nullptr;          // BlockPos
    static inline jmethodID s_bpInit    = nullptr;          // BlockPos.<init>(III)
    static inline jclass    s_lvlCls    = nullptr;          // ClientLevel / Level
    static inline jmethodID s_getState  = nullptr;          // Level.getBlockState
    static inline jclass    s_stateCls  = nullptr;          // BlockState
    static inline jmethodID s_isAir     = nullptr;          // BlockStateBase.isAir
    // Container click (InventoryMove)
    static inline jclass    s_gmCls     = nullptr;          // MultiPlayerGameMode
    static inline jfieldID  s_gmField   = nullptr;          // Minecraft.gameMode
    static inline jclass    s_mcCls     = nullptr;          // Minecraft
    static inline jmethodID s_handleClick = nullptr;        // handleInventoryMouseClick
    static inline jclass    s_clickTypeCls = nullptr;       // ClickType
    static inline jfieldID  s_swapField = nullptr;          // ClickType.SWAP
    static inline jfieldID  s_containerMenuField = nullptr; // Player.containerMenu
    static inline jfieldID  s_containerIdField  = nullptr;  // AbstractContainerMenu.containerId

    // Echte Block-Platzierung (Rage) — useItemOn via gameMode
    static inline jmethodID s_useItemOn    = nullptr;       // MultiPlayerGameMode.useItemOn
    static inline jclass    s_bhrCls       = nullptr;       // BlockHitResult
    static inline jmethodID s_bhrInit      = nullptr;       // BlockHitResult.<init>
    static inline jclass    s_vec3Cls      = nullptr;       // Vec3
    static inline jmethodID s_vec3Init     = nullptr;       // Vec3.<init>(DDD)
    static inline jclass    s_dirCls       = nullptr;       // Direction
    static inline jfieldID  s_dirDownField = nullptr;       // Direction.DOWN
    static inline jclass    s_handCls      = nullptr;       // InteractionHand
    static inline jfieldID  s_mainHandField = nullptr;      // InteractionHand.MAIN_HAND
    static inline jclass    s_irCls        = nullptr;       // InteractionResult
    static inline jmethodID s_irConsumes   = nullptr;       // InteractionResult.consumesAction
};
