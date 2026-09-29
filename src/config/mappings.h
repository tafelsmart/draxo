#pragma once

/*
 * Mapping Table — Minecraft 1.21.1 (Forge / NeoForge)
 *
 * Forge 1.21.1 uses Mojang (deobfuscated) names at runtime.
 * All class paths use JNI '/' separator format.
 * Method and field names are NOT obfuscated in the Forge runtime.
 */

namespace Mappings {

    // ─── Classes (JNI path format — Mojang deobfuscated) ────────────────
    inline constexpr const char* Minecraft_Class    = "net/minecraft/client/Minecraft";
    inline constexpr const char* Entity_Class       = "net/minecraft/world/entity/Entity";
    inline constexpr const char* LivingEntity_Class = "net/minecraft/world/entity/LivingEntity";
    inline constexpr const char* LocalPlayer_Class  = "net/minecraft/client/player/LocalPlayer";
    inline constexpr const char* ClientLevel_Class  = "net/minecraft/client/multiplayer/ClientLevel";
    inline constexpr const char* Vec3_Class         = "net/minecraft/world/phys/Vec3";
    inline constexpr const char* AABB_Class         = "net/minecraft/world/phys/AABB";
    inline constexpr const char* Player_Class       = "net/minecraft/world/entity/player/Player";
    inline constexpr const char* List_Class         = "java/util/List";
    inline constexpr const char* GameRenderer_Class = "net/minecraft/client/renderer/GameRenderer";
    inline constexpr const char* Connection_Class   = "net/minecraft/network/Connection";
    inline constexpr const char* RenderSystem_Class = "com/mojang/blaze3d/systems/RenderSystem";
    inline constexpr const char* Matrix4f_Class     = "org/joml/Matrix4f";
    inline constexpr const char* GameProfile_Class  = "com/mojang/authlib/GameProfile";
    inline constexpr const char* Component_Class    = "net/minecraft/network/chat/Component";
    inline constexpr const char* AbstractClientPlayer_Class = "net/minecraft/client/player/AbstractClientPlayer";
    inline constexpr const char* ItemEntity_Class   = "net/minecraft/world/entity/item/ItemEntity";
    inline constexpr const char* ArmorStand_Class   = "net/minecraft/world/entity/decoration/ArmorStand";
    inline constexpr const char* MinecartChest_Class= "net/minecraft/world/entity/vehicle/MinecartChest";
    inline constexpr const char* Camera_Class       = "net/minecraft/client/Camera";
    inline constexpr const char* Options_Class      = "net/minecraft/client/Options";
    inline constexpr const char* Level_Class        = "net/minecraft/world/level/Level";

    // ─── Minecraft Methods ─────────────────────────────────────────────
    inline constexpr const char* MC_getInstance     = "getInstance";
    inline constexpr const char* MC_getInstance_Sig = "()Lnet/minecraft/client/Minecraft;";

    // ─── Minecraft Fields ──────────────────────────────────────────────
    inline constexpr const char* MC_profileKeyPairManager = "profileKeyPairManager";
    inline constexpr const char* MC_profileKeyPairManager_Sig = "Lnet/minecraft/client/multiplayer/ProfileKeyPairManager;";
    inline constexpr const char* MC_player          = "player";
    inline constexpr const char* MC_player_Sig      = "Lnet/minecraft/client/player/LocalPlayer;";
    inline constexpr const char* MC_level           = "level";
    inline constexpr const char* MC_level_Sig       = "Lnet/minecraft/client/multiplayer/ClientLevel;";
    inline constexpr const char* MC_gameRenderer    = "gameRenderer";
    inline constexpr const char* MC_gameRenderer_Sig = "Lnet/minecraft/client/renderer/GameRenderer;";
    inline constexpr const char* MC_options          = "options";
    inline constexpr const char* MC_options_Sig      = "Lnet/minecraft/client/Options;";

    // ─── GameRenderer Methods ────────────────────────────────────────
    inline constexpr const char* GR_getMainCamera     = "getMainCamera";
    inline constexpr const char* GR_getMainCamera_Sig = "()Lnet/minecraft/client/Camera;";
    inline constexpr const char* GR_getFov            = "getFov";
    inline constexpr const char* GR_getFov_Sig        = "(Lnet/minecraft/client/Camera;FZ)F";


    // Camera: Wir lesen die FELDER (position/yRot/xRot) direkt — die
    // Methoden (getPosition/getXRot/getYRot) existieren ab 1.21.2 nicht mehr.

    // ─── ClientCommonPacketListenerImpl Methods ─────────────────────
    // sendChat(String) / sendCommand(String) — used by ChatBypass
    inline constexpr const char* CCPLI_sendChat         = "sendChat";
    inline constexpr const char* CCPLI_sendChat_Sig     = "(Ljava/lang/String;)V";
    inline constexpr const char* CCPLI_sendCommand      = "sendCommand";
    inline constexpr const char* CCPLI_sendCommand_Sig  = "(Ljava/lang/String;)V";

    // ─── Options Fields ──────────────────────────────────────────────
    inline constexpr const char* Options_fov            = "fov";
    inline constexpr const char* Options_gamma          = "gamma";
    inline constexpr const char* Options_gamma_Sig      = "Lnet/minecraft/client/OptionInstance;";
    // In 1.21.1, fov is an OptionInstance<Integer> — we access via fov().get()

    // ─── Entity Methods ────────────────────────────────────────────────
    inline constexpr const char* Entity_getX        = "getX";
    inline constexpr const char* Entity_getX_Sig    = "()D";
    inline constexpr const char* Entity_getY        = "getY";
    inline constexpr const char* Entity_getY_Sig    = "()D";
    inline constexpr const char* Entity_getZ        = "getZ";
    inline constexpr const char* Entity_getZ_Sig    = "()D";

    inline constexpr const char* Entity_isAlive     = "isAlive";
    inline constexpr const char* Entity_isAlive_Sig = "()Z";
    inline constexpr const char* Entity_getId       = "getId";
    inline constexpr const char* Entity_getId_Sig   = "()I";

    inline constexpr const char* Entity_getBoundingBox     = "getBoundingBox";
    inline constexpr const char* Entity_getBoundingBox_Sig = "()Lnet/minecraft/world/phys/AABB;";

    inline constexpr const char* Entity_getDeltaMovement     = "getDeltaMovement";
    inline constexpr const char* Entity_getDeltaMovement_Sig = "()Lnet/minecraft/world/phys/Vec3;";

    inline constexpr const char* Entity_setDeltaMovement     = "setDeltaMovement";
    inline constexpr const char* Entity_setDeltaMovement_Sig = "(Lnet/minecraft/world/phys/Vec3;)V";

    inline constexpr const char* Entity_getYRot     = "getYRot";
    inline constexpr const char* Entity_getYRot_Sig = "()F";
    inline constexpr const char* Entity_getXRot     = "getXRot";
    inline constexpr const char* Entity_getXRot_Sig = "()F";

    inline constexpr const char* Entity_setYRot     = "setYRot";
    inline constexpr const char* Entity_setYRot_Sig = "(F)V";
    inline constexpr const char* Entity_setXRot     = "setXRot";
    inline constexpr const char* Entity_setXRot_Sig = "(F)V";

    inline constexpr const char* Entity_getName     = "getName";
    inline constexpr const char* Entity_getName_Sig = "()Lnet/minecraft/network/chat/Component;";
    inline constexpr const char* Entity_getScoreboardName     = "getScoreboardName";
    inline constexpr const char* Entity_getScoreboardName_Sig = "()Ljava/lang/String;";
    inline constexpr const char* Entity_setSharedFlag     = "setSharedFlag";
    inline constexpr const char* Entity_setSharedFlag_Sig = "(IZ)V";
    inline constexpr const char* Entity_getSharedFlag     = "getSharedFlag";
    inline constexpr const char* Entity_getSharedFlag_Sig = "(I)Z";

    // ─── LivingEntity Methods ──────────────────────────────────────────
    inline constexpr const char* LivingEntity_getHealth     = "getHealth";
    inline constexpr const char* LivingEntity_getHealth_Sig = "()F";
    inline constexpr const char* LivingEntity_getMaxHealth     = "getMaxHealth";
    inline constexpr const char* LivingEntity_getMaxHealth_Sig = "()F";

    // ─── Player Methods ────────────────────────────────────────────────
    inline constexpr const char* Player_getGameProfile     = "getGameProfile";
    inline constexpr const char* Player_getGameProfile_Sig = "()Lcom/mojang/authlib/GameProfile;";

    // GameProfile.getName() — external library, never obfuscated
    inline constexpr const char* GameProfile_getName       = "getName";
    inline constexpr const char* GameProfile_getName_Sig   = "()Ljava/lang/String;";

    // ─── ClientLevel / Level ──────────────────────────────────────────
    inline constexpr const char* Level_players      = "players";
    inline constexpr const char* Level_players_Sig  = "Ljava/util/List;";

    // Level.getSeed() — deklariert auf WorldGenLevel (geerbt von Level/ClientLevel)
    inline constexpr const char* Level_getSeed     = "getSeed";
    inline constexpr const char* Level_getSeed_Sig = "()J";
    
    inline constexpr const char* Level_getBlockState = "getBlockState";
    inline constexpr const char* Level_getBlockState_Sig = "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;";

    // Level.isLoaded(BlockPos) — true wenn der Chunk des Punktes geladen ist
    // (StructureESP-Verifikation: nur geladene Chunks wirklich pruefen)
    inline constexpr const char* Level_isLoaded     = "isLoaded";
    inline constexpr const char* Level_isLoaded_Sig = "(Lnet/minecraft/core/BlockPos;)Z";

    // Registry-Zugriff fuer sprachunabhaengige Block-Namen (StructureESP):
    // BuiltInRegistries.BLOCK.getKey(block).getPath() -> "oak_planks"
    inline constexpr const char* BuiltInRegistries_Class    = "net/minecraft/core/registries/BuiltInRegistries";
    inline constexpr const char* BuiltInRegistries_BLOCK     = "BLOCK";
    inline constexpr const char* BuiltInRegistries_BLOCK_Sig = "Lnet/minecraft/core/DefaultedRegistry;";
    inline constexpr const char* Registry_Class    = "net/minecraft/core/Registry";
    inline constexpr const char* Registry_getKey   = "getKey";
    inline constexpr const char* Registry_getKey_Sig = "(Ljava/lang/Object;)Lnet/minecraft/resources/Identifier;";
    // Level.dimension() -> ResourceKey<Level> (FIELD, not method!)
    inline constexpr const char* Level_dimension = "dimension";
    inline constexpr const char* Level_dimension_Sig = "Lnet/minecraft/resources/ResourceKey;";
    // ResourceKey<Level>.location() -> Identifier
    inline constexpr const char* ResourceKey_Class = "net/minecraft/resources/ResourceKey";
    inline constexpr const char* ResourceKey_location = "location";
    inline constexpr const char* ResourceKey_location_Sig = "()Lnet/minecraft/resources/ResourceLocation;";
    // Identifier (= ResourceLocation in Mojang deobf)
    inline constexpr const char* Identifier_Class    = "net/minecraft/resources/Identifier";
    inline constexpr const char* Identifier_getPath  = "getPath";
    inline constexpr const char* Identifier_getPath_Sig = "()Ljava/lang/String;";

    // ─── Block & BlockState ──────────────────────────────────────────
    inline constexpr const char* MutableBlockPos_Class = "net/minecraft/core/BlockPos$MutableBlockPos";
    inline constexpr const char* MutableBlockPos_Init = "<init>";
    inline constexpr const char* MutableBlockPos_Init_Sig = "(III)V";
    inline constexpr const char* MutableBlockPos_set = "set";
    inline constexpr const char* MutableBlockPos_set_Sig = "(III)Lnet/minecraft/core/BlockPos$MutableBlockPos;";
    
    inline constexpr const char* BlockState_Class = "net/minecraft/world/level/block/state/BlockState";
    inline constexpr const char* BlockState_getBlock = "getBlock";
    inline constexpr const char* BlockState_getBlock_Sig = "()Lnet/minecraft/world/level/block/Block;";

    // ─── Entity Physics ────────────────────────────────────────────────
    inline constexpr const char* Entity_isInWater     = "isInWater";
    inline constexpr const char* Entity_isInWater_Sig = "()Z";

    inline constexpr const char* Entity_onGround     = "onGround";
    inline constexpr const char* Entity_onGround_Sig = "Z";
    inline constexpr const char* Entity_horizontalCollision = "horizontalCollision";
    inline constexpr const char* Entity_horizontalCollision_Sig = "Z";
    inline constexpr const char* Entity_wasTouchingWater = "wasTouchingWater";
    inline constexpr const char* Entity_wasTouchingWater_Sig = "Z";

    inline constexpr const char* Entity_isShiftKeyDown     = "isShiftKeyDown";
    inline constexpr const char* Entity_isShiftKeyDown_Sig = "()Z";

    inline constexpr const char* LivingEntity_discardFriction = "discardFriction";
    inline constexpr const char* LivingEntity_discardFriction_Sig = "Z";
    inline constexpr const char* LivingEntity_jumping = "jumping";
    inline constexpr const char* LivingEntity_jumping_Sig = "Z";

    inline constexpr const char* Block_Class = "net/minecraft/world/level/block/Block";
    inline constexpr const char* Block_getName = "getName";
    inline constexpr const char* Block_getName_Sig = "()Lnet/minecraft/network/chat/Component;";
    inline constexpr const char* Block_getDescriptionId = "getDescriptionId";
    inline constexpr const char* Block_getDescriptionId_Sig = "()Ljava/lang/String;";

    // ─── java.util.List Methods (JDK — never obfuscated) ───────────────
    inline constexpr const char* List_size          = "size";
    inline constexpr const char* List_size_Sig      = "()I";
    inline constexpr const char* List_get           = "get";
    inline constexpr const char* List_get_Sig       = "(I)Ljava/lang/Object;";

    // ─── Vec3 Fields ───────────────────────────────────────────────────
    inline constexpr const char* Vec3_x     = "x";
    inline constexpr const char* Vec3_y     = "y";
    inline constexpr const char* Vec3_z     = "z";
    inline constexpr const char* Vec3_D_Sig = "D";

    // ─── AABB Fields ───────────────────────────────────────────────────
    inline constexpr const char* AABB_minX  = "minX";
    inline constexpr const char* AABB_minY  = "minY";
    inline constexpr const char* AABB_minZ  = "minZ";
    inline constexpr const char* AABB_maxX  = "maxX";
    inline constexpr const char* AABB_maxY  = "maxY";
    inline constexpr const char* AABB_maxZ  = "maxZ";
    inline constexpr const char* AABB_D_Sig = "D";

    // ─── AABB Methods ──────────────────────────────────────────────────
    inline constexpr const char* AABB_inflate     = "inflate";
    inline constexpr const char* AABB_inflate_Sig = "(DDD)Lnet/minecraft/world/phys/AABB;";

    // ─── RenderSystem Methods ──────────────────────────────────────────
    inline constexpr const char* RS_getModelViewMatrix     = "getModelViewMatrix";
    inline constexpr const char* RS_getModelViewMatrix_Sig = "()Lorg/joml/Matrix4f;";
    inline constexpr const char* RS_getProjectionMatrix     = "getProjectionMatrix";
    inline constexpr const char* RS_getProjectionMatrix_Sig = "()Lorg/joml/Matrix4f;";
    // RS_getProjectionMatrix existiert nur bis 1.21.1; ab 1.21.2 wird die
    // Projektionsmatrix über GameRenderer.getProjectionMatrix(float) bezogen.

    // ─── GameRenderer Methods ─────────────────────────────────────────
    // getProjectionMatrix: 1.21.1 nimmt (double), ab 1.21.2 (float partialTicks)
    inline constexpr const char* GR_getProjectionMatrix           = "getProjectionMatrix";
    inline constexpr const char* GR_getProjectionMatrix_Sig       = "(F)Lorg/joml/Matrix4f;";
    inline constexpr const char* GR_getProjectionMatrix_Sig_Legacy = "(D)Lorg/joml/Matrix4f;";

    // ─── Matrix4f Fields (JOML — never obfuscated) ─────────────────────
    inline constexpr const char* Matrix4f_m00 = "m00";
    inline constexpr const char* Matrix4f_m01 = "m01";
    inline constexpr const char* Matrix4f_m02 = "m02";
    inline constexpr const char* Matrix4f_m03 = "m03";
    inline constexpr const char* Matrix4f_m10 = "m10";
    inline constexpr const char* Matrix4f_m11 = "m11";
    inline constexpr const char* Matrix4f_m12 = "m12";
    inline constexpr const char* Matrix4f_m13 = "m13";
    inline constexpr const char* Matrix4f_m20 = "m20";
    inline constexpr const char* Matrix4f_m21 = "m21";
    inline constexpr const char* Matrix4f_m22 = "m22";
    inline constexpr const char* Matrix4f_m23 = "m23";
    inline constexpr const char* Matrix4f_m30 = "m30";
    inline constexpr const char* Matrix4f_m31 = "m31";
    inline constexpr const char* Matrix4f_m32 = "m32";
    inline constexpr const char* Matrix4f_m33 = "m33";

    // ─── Network / Packet Classes ────────────────────────────────────
    inline constexpr const char* ClientPacketListener_Class = "net/minecraft/client/multiplayer/ClientPacketListener";
    inline constexpr const char* ServerboundMovePlayerPacket_Pos_Class = "net/minecraft/network/protocol/game/ServerboundMovePlayerPacket$Pos";
    inline constexpr const char* ServerboundInteractPacket_Class = "net/minecraft/network/protocol/game/ServerboundInteractPacket";

    // Minecraft.getConnection() → ClientPacketListener
    inline constexpr const char* MC_getConnection = "getConnection";
    inline constexpr const char* MC_getConnection_Sig = "()Lnet/minecraft/client/multiplayer/ClientPacketListener;";

    // Connection.send(Packet) — Connection is a field of ClientCommonPacketListenerImpl
    inline constexpr const char* Connection_send = "send";
    inline constexpr const char* Connection_send_Sig = "(Lnet/minecraft/network/protocol/Packet;)V";
    inline constexpr const char* ClientCommonPacketListenerImpl_Class = "net/minecraft/client/multiplayer/ClientCommonPacketListenerImpl";
    inline constexpr const char* CCPLI_connection = "connection";
    inline constexpr const char* CCPLI_connection_Sig = "Lnet/minecraft/network/Connection;";

    // ServerboundMovePlayerPacket.Pos constructor: (double x, double y, double z, float yaw, float pitch, boolean onGround)
    inline constexpr const char* MovePacketPos_Init = "<init>";

    // ServerboundInteractPacket.createAttackPacket(Entity, boolean sneaking)
    inline constexpr const char* InteractPacket_createAttackPacket = "createAttackPacket";
    inline constexpr const char* InteractPacket_createAttackPacket_Sig = "(Lnet/minecraft/world/entity/Entity;Z)Lnet/minecraft/network/protocol/game/ServerboundInteractPacket;";

    // Minecraft.hitResult field (HitResult) — for detecting what entity the player is targeting
    inline constexpr const char* MC_hitResult = "hitResult";
    inline constexpr const char* MC_hitResult_Sig = "Lnet/minecraft/world/phys/HitResult;";

    // HitResult.getType() — returns HitResult.Type enum
    inline constexpr const char* HitResult_Class = "net/minecraft/world/phys/HitResult";
    inline constexpr const char* HitResult_getType = "getType";
    inline constexpr const char* HitResult_getType_Sig = "()Lnet/minecraft/world/phys/HitResult$Type;";

    // EntityHitResult.getEntity()
    inline constexpr const char* EntityHitResult_Class = "net/minecraft/world/phys/EntityHitResult";
    inline constexpr const char* EntityHitResult_getEntity = "getEntity";
    inline constexpr const char* EntityHitResult_getEntity_Sig = "()Lnet/minecraft/world/entity/Entity;";

    // Player.attackStrengthTicker (int field) — legacy alias, siehe
    // LivingEntity_attackStrengthTicker unten (in 1.21.x auf LivingEntity).
    inline constexpr const char* Player_attackStrengthTicker = "attackStrengthTicker";
    inline constexpr const char* Player_attackStrengthTicker_Sig = "I";

    // LivingEntity.attackStrengthTicker (int) — zählt 0..20 hoch, 20 = volle
    // Schlagkraft. Fallback, wenn getAttackStrengthScale fehlt.
    inline constexpr const char* LivingEntity_isDeadOrDying = "isDeadOrDying";
    inline constexpr const char* LivingEntity_isDeadOrDying_Sig = "()Z";
    inline constexpr const char* LivingEntity_attackStrengthTicker = "attackStrengthTicker";
    inline constexpr const char* LivingEntity_attackStrengthTicker_Sig = "I";

    // Player.getAttackStrengthScale(float) — 1.0 = Cooldown fertig (voller Damage).
    // Eindeutig gegenueber dem Ticker-Feld (Zaehlrichtung versionsabhaengig).
    inline constexpr const char* Player_getOffhandItem = "getOffhandItem";
    inline constexpr const char* Player_getOffhandItem_Sig = "()Lnet/minecraft/world/item/ItemStack;";
    inline constexpr const char* Player_getAttackStrengthScale = "getAttackStrengthScale";
    inline constexpr const char* Player_getAttackStrengthScale_Sig = "(F)F";

    // LivingEntity.hurtTime (int) — >0 solange der Spieler gerade getroffen
    // wurde (Knockback-Fenster, zaehlt 10->0 runter).
    inline constexpr const char* LivingEntity_hurtTime = "hurtTime";
    inline constexpr const char* LivingEntity_hurtTime_Sig = "I";

    // Player.swing(InteractionHand)
    inline constexpr const char* Player_swing = "swing";
    inline constexpr const char* Player_swing_Sig = "(Lnet/minecraft/world/InteractionHand;)V";

    // InteractionHand.MAIN_HAND enum constant
    inline constexpr const char* InteractionHand_Class = "net/minecraft/world/InteractionHand";
    inline constexpr const char* InteractionHand_MAIN_HAND = "MAIN_HAND";
    inline constexpr const char* InteractionHand_MAIN_HAND_Sig = "Lnet/minecraft/world/InteractionHand;";

    // ─── Minecraft Class Signature (for JVMTI bootstrap) ──────────────
    inline constexpr const char* Minecraft_Class_Sig = "Lnet/minecraft/client/Minecraft;";

    // ─── Minecraft Singleton static field ─────────────────────────────
    inline constexpr const char* MC_instance     = "instance";
    inline constexpr const char* MC_instance_Sig = "Lnet/minecraft/client/Minecraft;";

    // ─── Camera Fields (NOT methods — read directly) ──────────────────
    inline constexpr const char* Camera_position      = "position";
    inline constexpr const char* Camera_position_Sig  = "Lnet/minecraft/world/phys/Vec3;";
    inline constexpr const char* Camera_yRot          = "yRot";
    inline constexpr const char* Camera_yRot_Sig      = "F";
    inline constexpr const char* Camera_xRot          = "xRot";
    inline constexpr const char* Camera_xRot_Sig      = "F";

    // ─── Options FOV (OptionInstance) ─────────────────────────────────
    inline constexpr const char* Options_fov_Sig        = "Lnet/minecraft/client/OptionInstance;";
    inline constexpr const char* OptionInstance_Class   = "net/minecraft/client/OptionInstance";
    inline constexpr const char* OptionInstance_get     = "get";
    inline constexpr const char* OptionInstance_get_Sig = "()Ljava/lang/Object;";

    // ─── Window (framebuffer) ─────────────────────────────────────────
    inline constexpr const char* MC_window           = "window";
    inline constexpr const char* MC_window_Sig       = "Lcom/mojang/blaze3d/platform/Window;";
    inline constexpr const char* Window_Class        = "com/mojang/blaze3d/platform/Window";
    inline constexpr const char* Window_getWidth     = "getWidth";
    inline constexpr const char* Window_getWidth_Sig = "()I";
    inline constexpr const char* Window_getHeight    = "getHeight";
    inline constexpr const char* Window_getHeight_Sig = "()I";

    // ─── Component.getString ──────────────────────────────────────────
    inline constexpr const char* Component_getString     = "getString";
    inline constexpr const char* Component_getString_Sig = "()Ljava/lang/String;";

    // ─── Vec3 constructor ─────────────────────────────────────────────
    inline constexpr const char* Vec3_Init     = "<init>";
    inline constexpr const char* Vec3_Init_Sig = "(DDD)V";

    // ─── ClientLevel.entitiesForRendering ─────────────────────────────
    inline constexpr const char* ClientLevel_entitiesForRendering     = "entitiesForRendering";
    inline constexpr const char* ClientLevel_entitiesForRendering_Sig = "()Ljava/lang/Iterable;";

    // ─── Player.attack(Entity) ────────────────────────────────────────
    inline constexpr const char* Player_attack     = "attack";
    inline constexpr const char* Player_attack_Sig = "(Lnet/minecraft/world/entity/Entity;)V";

    // ─── Attributes (Reach) ───────────────────────────────────────────
    inline constexpr const char* Attributes_Class = "net/minecraft/world/entity/ai/attributes/Attributes";
    inline constexpr const char* Attributes_ENTITY_INTERACTION_RANGE     = "ENTITY_INTERACTION_RANGE";
    inline constexpr const char* Attributes_ENTITY_INTERACTION_RANGE_Sig = "Lnet/minecraft/core/Holder;";
    inline constexpr const char* Attributes_BLOCK_INTERACTION_RANGE      = "BLOCK_INTERACTION_RANGE";
    inline constexpr const char* Attributes_BLOCK_INTERACTION_RANGE_Sig  = "Lnet/minecraft/core/Holder;";
    inline constexpr const char* LivingEntity_getAttribute     = "getAttribute";
    inline constexpr const char* LivingEntity_getAttribute_Sig = "(Lnet/minecraft/core/Holder;)Lnet/minecraft/world/entity/ai/attributes/AttributeInstance;";
    inline constexpr const char* AttributeInstance_Class       = "net/minecraft/world/entity/ai/attributes/AttributeInstance";
    inline constexpr const char* AttributeInstance_setBaseValue     = "setBaseValue";
    inline constexpr const char* AttributeInstance_setBaseValue_Sig = "(D)V";

    // ─── MinecartChest — package moved in 1.21.2+ ─────────────────────
    inline constexpr const char* MinecartChest_Class_New = "net/minecraft/world/entity/vehicle/minecart/MinecartChest";

    // ─── ServerboundMovePlayerPacket$Pos ctor — 1.21.1: (DDDZ)V, 1.21.2+: (DDDZZ)V ──
    inline constexpr const char* MovePacketPos_Init_Sig      = "(DDDZZ)V";
    inline constexpr const char* MovePacketPos_Init_Sig_Legacy = "(DDDZ)V";

    // ─── MultiPlayerGameMode (Nuker + KillAura) ─────────────────────
    inline constexpr const char* MC_gameMode = "gameMode";
    inline constexpr const char* MC_gameMode_Sig = "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;";

    inline constexpr const char* GameMode_Class = "net/minecraft/client/multiplayer/MultiPlayerGameMode";
    inline constexpr const char* GameMode_startDestroyBlock = "startDestroyBlock";
    inline constexpr const char* GameMode_startDestroyBlock_Sig = "(Lnet/minecraft/core/BlockPos;Lnet/minecraft/core/Direction;)Z";
    inline constexpr const char* GameMode_continueDestroyBlock = "continueDestroyBlock";
    inline constexpr const char* GameMode_continueDestroyBlock_Sig = "(Lnet/minecraft/core/BlockPos;Lnet/minecraft/core/Direction;)Z";
    inline constexpr const char* GameMode_stopDestroyBlock = "stopDestroyBlock";
    inline constexpr const char* GameMode_stopDestroyBlock_Sig = "()V";

    // MultiPlayerGameMode.attack(Player, Entity) — sendet das ECHTE
    // ServerboundInteractPacket (Attack). Seit 1.21.x überschreibt
    // LocalPlayer.attack(Entity) die Basis-Methode nicht mehr; der
    // Packet-Versand läuft über die gameMode-Methode. Ohne diesen Aufruf
    // schwingt der Arm zwar (Swing-Packet), aber es geht KEIN Attack-Packet
    // raus → kein Damage.
    inline constexpr const char* GameMode_attack = "attack";
    inline constexpr const char* GameMode_attack_Sig = "(Lnet/minecraft/world/entity/player/Player;Lnet/minecraft/world/entity/Entity;)V";
    // Bis 1.21.1 hatte MultiPlayerGameMode.attack die Signatur (Entity)V
    inline constexpr const char* GameMode_attack_Sig_Legacy = "(Lnet/minecraft/world/entity/Entity;)V";

    // MultiPlayerGameMode.useItemOn(Player, InteractionHand, BlockHitResult)
    // — ECHTE Block-Platzierung per JNI (Scaffold). Sendet das
    // ServerboundUseItemOnPacket direkt an die Ziel-Position, ohne dass der
    // Spieler hinschauen oder ein Mausklick nötig ist.
    inline constexpr const char* GameMode_useItemOn = "useItemOn";
    inline constexpr const char* GameMode_useItemOn_Sig = "(Lnet/minecraft/client/player/LocalPlayer;Lnet/minecraft/world/InteractionHand;Lnet/minecraft/world/phys/BlockHitResult;)Lnet/minecraft/world/InteractionResult;";

    // BlockHitResult(Vec3 location, Direction direction, BlockPos blockPos, boolean inside)
    inline constexpr const char* BlockHitResult_Class = "net/minecraft/world/phys/BlockHitResult";
    inline constexpr const char* BlockHitResult_Init = "<init>";
    inline constexpr const char* BlockHitResult_Init_Sig = "(Lnet/minecraft/world/phys/Vec3;Lnet/minecraft/core/Direction;Lnet/minecraft/core/BlockPos;Z)V";

    // InteractionResult.consumesAction() — true wenn useItemOn wirklich
    // platziert hat (Scaffold: nur dann Delay fortschreiten)
    inline constexpr const char* InteractionResult_Class = "net/minecraft/world/InteractionResult";
    inline constexpr const char* InteractionResult_consumesAction = "consumesAction";
    inline constexpr const char* InteractionResult_consumesAction_Sig = "()Z";

    // ─── BlockPos (Nuker) ────────────────────────────────────────────
    inline constexpr const char* BlockPos_Class = "net/minecraft/core/BlockPos";
    inline constexpr const char* BlockPos_Init = "<init>";
    inline constexpr const char* BlockPos_Init_Sig = "(III)V";

    // ─── Direction (Nuker) ───────────────────────────────────────────
    inline constexpr const char* Direction_Class = "net/minecraft/core/Direction";
    inline constexpr const char* Direction_Class_Sig = "Lnet/minecraft/core/Direction;";
    inline constexpr const char* Direction_DOWN = "DOWN";
    inline constexpr const char* Direction_DOWN_Sig = "Lnet/minecraft/core/Direction;";
    inline constexpr const char* Direction_UP   = "UP";
    inline constexpr const char* Direction_UP_Sig = "Lnet/minecraft/core/Direction;";

    // ─── Inventory / ItemStack (Scaffold v2) ─────────────────────────
    inline constexpr const char* Player_inventory = "inventory";
    inline constexpr const char* Player_inventory_Sig = "Lnet/minecraft/world/entity/player/Inventory;";

    inline constexpr const char* Inventory_Class = "net/minecraft/world/entity/player/Inventory";
    inline constexpr const char* Inventory_getSelectedSlot = "getSelectedSlot";
    inline constexpr const char* Inventory_getSelectedSlot_Sig = "()I";
    inline constexpr const char* Inventory_setSelectedSlot = "setSelectedSlot";
    inline constexpr const char* Inventory_setSelectedSlot_Sig = "(I)V";
    inline constexpr const char* Inventory_getItem = "getItem";
    inline constexpr const char* Inventory_getItem_Sig = "(I)Lnet/minecraft/world/item/ItemStack;";
    inline constexpr const char* Inventory_getContainerSize = "getContainerSize";
    inline constexpr const char* Inventory_getContainerSize_Sig = "()I";

    inline constexpr const char* ItemStack_Class = "net/minecraft/world/item/ItemStack";
    inline constexpr const char* ItemStack_isEmpty = "isEmpty";
    inline constexpr const char* ItemStack_isEmpty_Sig = "()Z";
    inline constexpr const char* ItemStack_getCount = "getCount";
    inline constexpr const char* ItemStack_getCount_Sig = "()I";
    inline constexpr const char* ItemStack_getItem = "getItem";
    inline constexpr const char* ItemStack_getItem_Sig = "()Lnet/minecraft/world/item/Item;";
    inline constexpr const char* ItemStack_getHoverName = "getHoverName";
    inline constexpr const char* ItemStack_getHoverName_Sig = "()Lnet/minecraft/network/chat/Component;";

    inline constexpr const char* Item_Class = "net/minecraft/world/item/Item";
    inline constexpr const char* BlockItem_Class = "net/minecraft/world/item/BlockItem";

    // isAir liegt auf BlockBehaviour$BlockStateBase (Superklasse von BlockState)
    inline constexpr const char* BlockStateBase_isAir = "isAir";
    inline constexpr const char* BlockStateBase_isAir_Sig = "()Z";

    // ─── InventoryMove / Container-Click (Scaffold) ───────────────────
    inline constexpr const char* Player_containerMenu = "containerMenu";
    inline constexpr const char* Player_containerMenu_Sig = "Lnet/minecraft/world/inventory/AbstractContainerMenu;";
    inline constexpr const char* Player_inventoryMenu = "inventoryMenu";
    inline constexpr const char* Player_inventoryMenu_Sig = "Lnet/minecraft/world/inventory/InventoryMenu;";

    inline constexpr const char* AbstractContainerMenu_Class = "net/minecraft/world/inventory/AbstractContainerMenu";
    inline constexpr const char* AbstractContainerMenu_containerId = "containerId";
    inline constexpr const char* AbstractContainerMenu_containerId_Sig = "I";

    inline constexpr const char* ClickType_Class = "net/minecraft/world/inventory/ClickType";
    inline constexpr const char* ClickType_Class_Sig = "Lnet/minecraft/world/inventory/ClickType;";
    inline constexpr const char* ClickType_QUICK_MOVE = "QUICK_MOVE";
    inline constexpr const char* ClickType_QUICK_MOVE_Sig = "Lnet/minecraft/world/inventory/ClickType;";
    inline constexpr const char* ClickType_SWAP = "SWAP";
    inline constexpr const char* ClickType_SWAP_Sig = "Lnet/minecraft/world/inventory/ClickType;";

    // MultiPlayerGameMode.handleInventoryMouseClick(containerId, slotId, button, ClickType, player)
    inline constexpr const char* GameMode_handleInventoryMouseClick = "handleInventoryMouseClick";
    inline constexpr const char* GameMode_handleInventoryMouseClick_Sig = "(IIILnet/minecraft/world/inventory/ClickType;Lnet/minecraft/world/entity/player/Player;)V";

    // ─── Timer / DeltaTracker (Timer module) ──────────────────────────
    // 1.21.1: Minecraft.timer (Timer). 1.21.2+: getDeltaTracker() -> DeltaTracker
    inline constexpr const char* MC_getDeltaTracker = "getDeltaTracker";
    inline constexpr const char* MC_getDeltaTracker_Sig = "()Lnet/minecraft/client/DeltaTracker;";
    inline constexpr const char* Timer_Class = "net/minecraft/client/DeltaTracker$Timer";
    inline constexpr const char* Timer_msPerTick = "msPerTick";
    inline constexpr const char* Timer_msPerTick_Sig = "F";
    // MC_timer: 1.21.1 = 'timer', 1.21.2+ = 'deltaTracker' (Builder ersetzt
    // den Feldnamen versionabhängig). Die Sig referenziert die moderne
    // DeltaTracker$Timer-Klasse; der Builder schreibt sie für < 1.21.2 zurück.
    inline constexpr const char* MC_timer = "timer";
    inline constexpr const char* MC_timer_Sig = "Lnet/minecraft/client/DeltaTracker$Timer;";

    // ─── Minecraft.rightClickDelay / crosshairPickEntity ──────────────
    inline constexpr const char* MC_rightClickDelay = "rightClickDelay";
    inline constexpr const char* MC_rightClickDelay_Sig = "I";
    inline constexpr const char* MC_crosshairPickEntity = "crosshairPickEntity";
    inline constexpr const char* MC_crosshairPickEntity_Sig = "Lnet/minecraft/world/entity/Entity;";

    // MultiPlayerGameMode.destroyDelay (int) — moderner FastBreak-Hebel
    // (0 = sofort neuer Abbau beginnbar). Ersetzt das alte
    // Minecraft.destroySpeed, das ab 1.21.2 nicht mehr existiert.
    inline constexpr const char* GameMode_destroyDelay = "destroyDelay";
    inline constexpr const char* GameMode_destroyDelay_Sig = "I";

    // ─── Minecraft.playerList (tab list — connected players) ─────────
    inline constexpr const char* MC_playerList = "playerList";
    inline constexpr const char* MC_playerList_Sig = "Ljava/util/List;";

    // ─── ServerData / getCurrentServer (Server IP HUD) ────────────────
    // Minecraft.getCurrentServer() -> ServerData (1.21.11: "X")
    inline constexpr const char* MC_getCurrentServer = "getCurrentServer";
    inline constexpr const char* MC_getCurrentServer_Sig = "()Lnet/minecraft/client/multiplayer/ServerData;";
    inline constexpr const char* ServerData_Class = "net/minecraft/client/multiplayer/ServerData";
    // ServerData.ip (1.21.11: "b")
    inline constexpr const char* ServerData_ip = "ip";
    inline constexpr const char* ServerData_ip_Sig = "Ljava/lang/String;";
    // ServerData.playerList (1.21.11: "i") — Tab-Liste, ab 1.21.2 hier statt auf Minecraft
    inline constexpr const char* ServerData_playerList = "playerList";
    inline constexpr const char* ServerData_playerList_Sig = "Ljava/util/List;";
}
