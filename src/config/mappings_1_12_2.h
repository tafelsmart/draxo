#pragma once
/*
 * Mapping Table â€” Minecraft 1.12.2 (MCP stable_39 / Forge)
 *
 * Forge 1.12.2 deobfuscates to MCP names at runtime. All class paths,
 * method names, and field names below use the MCP naming convention.
 */

namespace Mappings {

    // â”€â”€â”€ Classes (JNI path format â€” MCP names) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Minecraft_Class    = "net/minecraft/client/Minecraft";
    inline constexpr const char* Entity_Class       = "net/minecraft/entity/Entity";
    inline constexpr const char* LivingEntity_Class = "net/minecraft/entity/EntityLivingBase";
    inline constexpr const char* LocalPlayer_Class  = "net/minecraft/client/entity/EntityPlayerSP";
    inline constexpr const char* ClientLevel_Class  = "net/minecraft/client/multiplayer/WorldClient";
    inline constexpr const char* Vec3_Class         = "net/minecraft/util/math/Vec3d";
    inline constexpr const char* AABB_Class         = "net/minecraft/util/math/AxisAlignedBB";
    inline constexpr const char* Player_Class       = "net/minecraft/entity/player/EntityPlayer";
    inline constexpr const char* List_Class         = "java/util/List";
    inline constexpr const char* GameRenderer_Class = "net/minecraft/client/renderer/EntityRenderer";
    inline constexpr const char* Connection_Class   = "net/minecraft/network/NetworkManager";
    inline constexpr const char* RenderSystem_Class = "net/minecraft/client/renderer/GlStateManager";
    inline constexpr const char* Matrix4f_Class     = "net/minecraft/client/renderer/culling/ClippingHelper";
    inline constexpr const char* GameProfile_Class  = "com/mojang/authlib/GameProfile";
    inline constexpr const char* Component_Class    = "net/minecraft/util/text/ITextComponent";
    inline constexpr const char* AbstractClientPlayer_Class = "net/minecraft/client/entity/AbstractClientPlayer";
    inline constexpr const char* ItemEntity_Class   = "net/minecraft/entity/item/EntityItem";
    inline constexpr const char* ArmorStand_Class   = "net/minecraft/entity/item/EntityArmorStand";
    inline constexpr const char* MinecartChest_Class= "net/minecraft/entity/item/EntityMinecartChest";
    inline constexpr const char* Options_Class      = "net/minecraft/client/settings/GameSettings";
    inline constexpr const char* Level_Class        = "net/minecraft/world/World";
    inline constexpr const char* Camera_Class       = "net/minecraft/client/renderer/EntityRenderer";

    // â”€â”€â”€ Minecraft â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* MC_getInstance     = "getMinecraft";
    inline constexpr const char* MC_getInstance_Sig = "()Lnet/minecraft/client/Minecraft;";
    inline constexpr const char* MC_instance         = "instance";
    inline constexpr const char* MC_player          = "player";
    inline constexpr const char* MC_player_Sig      = "Lnet/minecraft/client/entity/EntityPlayerSP;";
    inline constexpr const char* MC_level           = "world";
    inline constexpr const char* MC_level_Sig       = "Lnet/minecraft/client/multiplayer/WorldClient;";
    inline constexpr const char* MC_gameRenderer    = "entityRenderer";
    inline constexpr const char* MC_gameRenderer_Sig = "Lnet/minecraft/client/renderer/EntityRenderer;";
    inline constexpr const char* MC_options          = "gameSettings";
    inline constexpr const char* MC_options_Sig      = "Lnet/minecraft/client/settings/GameSettings;";
    inline constexpr const char* MC_rightClickDelay  = "rightClickDelayTimer";
    inline constexpr const char* MC_rightClickDelay_Sig = "I";
    inline constexpr const char* MC_profileKeyPairManager = "profileKeyPairManager";
    inline constexpr const char* MC_profileKeyPairManager_Sig = "Ljava/lang/Object;";
    inline constexpr const char* MC_timer            = "timer";
    inline constexpr const char* MC_timer_Sig        = "Lnet/minecraft/util/Timer;";
    inline constexpr const char* MC_gameMode         = "playerController";
    inline constexpr const char* MC_gameMode_Sig     = "Lnet/minecraft/client/multiplayer/PlayerControllerMP;";
    inline constexpr const char* MC_screen           = "currentScreen";
    inline constexpr const char* MC_screen_Sig       = "Lnet/minecraft/client/gui/GuiScreen;";
    inline constexpr const char* MC_crosshairPickEntity = "objectMouseOver";
    inline constexpr const char* MC_crosshairPickEntity_Sig = "Lnet/minecraft/util/math/RayTraceResult;";
    inline constexpr const char* MC_hitResult        = "objectMouseOver";
    inline constexpr const char* MC_hitResult_Sig    = "Lnet/minecraft/util/math/RayTraceResult;";
    inline constexpr const char* MC_getConnection    = "getConnection";
    inline constexpr const char* MC_getConnection_Sig = "()Lnet/minecraft/network/NetworkManager;";
    inline constexpr const char* MC_getCurrentServer = "getCurrentServerData";
    inline constexpr const char* MC_getCurrentServer_Sig = "()Lnet/minecraft/client/multiplayer/ServerData;";
    inline constexpr const char* MC_playerList       = "playerList";
    inline constexpr const char* MC_playerList_Sig   = "Lnet/minecraft/client/network/NetHandlerPlayClient;";

    // â”€â”€â”€ EntityRenderer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* GR_getMainCamera     = "getCamera";
    inline constexpr const char* GR_getMainCamera_Sig = "()Lnet/minecraft/client/renderer/EntityRenderer;";
    inline constexpr const char* GR_getFov            = "getFOVModifier";
    inline constexpr const char* GR_getFov_Sig        = "(FZ)F";
    inline constexpr const char* GR_getProjectionMatrix= "getProjectionMatrix";
    inline constexpr const char* GR_getProjectionMatrix_Sig = "(D)Lorg/joml/Matrix4f;";
    inline constexpr const char* GR_getProjectionMatrix_Sig_Legacy = "(D)Lorg/joml/Matrix4f;";

    // â”€â”€â”€ Camera (not a separate class in 1.12.2) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Camera_position       = "cameraPosition";
    inline constexpr const char* Camera_position_Sig   = "Lnet/minecraft/util/math/Vec3d;";
    inline constexpr const char* Camera_yRot           = "cameraYaw";
    inline constexpr const char* Camera_yRot_Sig       = "F";
    inline constexpr const char* Camera_xRot           = "cameraPitch";
    inline constexpr const char* Camera_xRot_Sig       = "F";

    // â”€â”€â”€ GameSettings (Options equivalent) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Options_fov            = "fovSetting";
    inline constexpr const char* Options_gamma          = "gammaSetting";
    inline constexpr const char* Options_gamma_Sig      = "F";  // raw float

    // â”€â”€â”€ Entity â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Entity_getX           = "posX";
    inline constexpr const char* Entity_getX_Sig       = "D";
    inline constexpr const char* Entity_getY           = "posY";
    inline constexpr const char* Entity_getY_Sig       = "D";
    inline constexpr const char* Entity_getZ           = "posZ";
    inline constexpr const char* Entity_getZ_Sig       = "D";
    inline constexpr const char* Entity_getYaw         = "rotationYaw";
    inline constexpr const char* Entity_getYaw_Sig     = "F";
    inline constexpr const char* Entity_getPitch       = "rotationPitch";
    inline constexpr const char* Entity_getPitch_Sig   = "F";
    inline constexpr const char* Entity_motionX        = "motionX";
    inline constexpr const char* Entity_motionX_Sig    = "D";
    inline constexpr const char* Entity_motionY        = "motionY";
    inline constexpr const char* Entity_motionY_Sig    = "D";
    inline constexpr const char* Entity_motionZ        = "motionZ";
    inline constexpr const char* Entity_motionZ_Sig    = "D";
    inline constexpr const char* Entity_onGround       = "onGround";
    inline constexpr const char* Entity_onGround_Sig   = "Z";
    inline constexpr const char* Entity_isDead         = "isDead";
    inline constexpr const char* Entity_isDead_Sig     = "Z";
    inline constexpr const char* Entity_getId           = "entityId";
    inline constexpr const char* Entity_getId_Sig       = "I";
    inline constexpr const char* Entity_getBoundingBox  = "getEntityBoundingBox";
    inline constexpr const char* Entity_getBoundingBox_Sig = "()Lnet/minecraft/util/math/AxisAlignedBB;";
    inline constexpr const char* Entity_isSneaking      = "isSneaking";
    inline constexpr const char* Entity_isSneaking_Sig  = "()Z";
    inline constexpr const char* Entity_width           = "width";
    inline constexpr const char* Entity_width_Sig       = "F";
    inline constexpr const char* Entity_height          = "height";
    inline constexpr const char* Entity_height_Sig      = "F";

    // â”€â”€â”€ EntityLivingBase â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* LivingEntity_getHealth        = "getHealth";
    inline constexpr const char* LivingEntity_getHealth_Sig    = "()F";
    inline constexpr const char* LivingEntity_getMaxHealth     = "getMaxHealth";
    inline constexpr const char* LivingEntity_getMaxHealth_Sig = "()F";
    inline constexpr const char* LivingEntity_isItemInUse       = "isHandActive";
    inline constexpr const char* LivingEntity_isItemInUse_Sig   = "()Z";
    inline constexpr const char* LivingEntity_getAttribute      = "getEntityAttribute";
    inline constexpr const char* LivingEntity_getAttribute_Sig  = "(Lnet/minecraft/entity/ai/attributes/IAttribute;)Lnet/minecraft/entity/ai/attributes/IAttributeInstance;";
    inline constexpr const char* LivingEntity_moveForward       = "moveForward";
    inline constexpr const char* LivingEntity_moveForward_Sig   = "F";
    inline constexpr const char* LivingEntity_moveStrafing      = "moveStrafing";
    inline constexpr const char* LivingEntity_moveStrafing_Sig  = "F";

    // â”€â”€â”€ Player â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Player_attack        = "attackTargetEntityWithCurrentItem";
    inline constexpr const char* Player_attack_Sig    = "(Lnet/minecraft/entity/Entity;)V";
    inline constexpr const char* Player_swing         = "swingArm";
    inline constexpr const char* Player_swing_Sig     = "(Lnet/minecraft/util/EnumHand;)V";
    inline constexpr const char* Player_getAttackStrengthScale = "getCooledAttackStrength";
    inline constexpr const char* Player_getAttackStrengthScale_Sig = "(F)F";

    // â”€â”€â”€ World â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* World_getAllEntities      = "loadedEntityList";
    inline constexpr const char* World_getAllEntities_Sig  = "Ljava/util/List;";
    inline constexpr const char* World_getPlayers          = "playerEntities";
    inline constexpr const char* World_getPlayers_Sig      = "Ljava/util/List;";

    // â”€â”€â”€ Attributes (SharedMonsterAttributes) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Attributes_Class                    = "net/minecraft/entity/SharedMonsterAttributes";
    inline constexpr const char* Attributes_ENTITY_INTERACTION_RANGE = "ATTACK_DAMAGE";
    inline constexpr const char* Attributes_ENTITY_INTERACTION_RANGE_Sig = "Lnet/minecraft/entity/ai/attributes/IAttribute;";
    inline constexpr const char* Attributes_BLOCK_INTERACTION_RANGE  = "ATTACK_SPEED";
    inline constexpr const char* Attributes_BLOCK_INTERACTION_RANGE_Sig = "Lnet/minecraft/entity/ai/attributes/IAttribute;";
    inline constexpr const char* AttributeInstance_Class           = "net/minecraft/entity/ai/attributes/IAttributeInstance";
    inline constexpr const char* AttributeInstance_setBaseValue     = "setBaseValue";
    inline constexpr const char* AttributeInstance_setBaseValue_Sig = "(D)V";

    // â”€â”€â”€ PlayerControllerMP / GameMode â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* GameMode_Class     = "net/minecraft/client/multiplayer/PlayerControllerMP";
    inline constexpr const char* GameMode_attack    = "attackEntity";
    inline constexpr const char* GameMode_attack_Sig= "(Lnet/minecraft/entity/player/EntityPlayer;Lnet/minecraft/entity/Entity;)V";

    // â”€â”€â”€ EnumHand â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* InteractionHand_Class      = "net/minecraft/util/EnumHand";
    inline constexpr const char* InteractionHand_MAIN_HAND  = "MAIN_HAND";
    inline constexpr const char* InteractionHand_MAIN_HAND_Sig = "Lnet/minecraft/util/EnumHand;";

    // â”€â”€â”€ RayTraceResult â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* EntityHitResult_Class       = "net/minecraft/util/math/RayTraceResult";
    inline constexpr const char* EntityHitResult_getEntity   = "entityHit";
    inline constexpr const char* EntityHitResult_getEntity_Sig = "Lnet/minecraft/entity/Entity;";

    // â”€â”€â”€ Network â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* ClientCommonPacketListenerImpl_Class = "net/minecraft/client/network/NetHandlerPlayClient";
    inline constexpr const char* CCPLI_connection       = "netManager";
    inline constexpr const char* CCPLI_connection_Sig   = "Lnet/minecraft/network/NetworkManager;";
    inline constexpr const char* CCPLI_sendChat         = "sendChatMessage";
    inline constexpr const char* CCPLI_sendChat_Sig     = "(Ljava/lang/String;)V";
    inline constexpr const char* CCPLI_sendCommand      = "sendChatMessage";
    inline constexpr const char* CCPLI_sendCommand_Sig  = "(Ljava/lang/String;)V";
    inline constexpr const char* Connection_send        = "sendPacket";
    inline constexpr const char* Connection_send_Sig    = "(Lnet/minecraft/network/Packet;)V";
    inline constexpr const char* MovePacketPos_Init_Sig       = "(DDDZZ)V";
    inline constexpr const char* MovePacketPos_Init_Sig_Legacy = "(DDDZ)V";
    inline constexpr const char* ServerboundMovePlayerPacket_Pos_Class = "net/minecraft/network/play/client/CPacketPlayer$Position";

    // â”€â”€â”€ ServerData â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* ServerData_Class        = "net/minecraft/client/multiplayer/ServerData";
    inline constexpr const char* ServerData_playerList    = "playerList";
    inline constexpr const char* ServerData_playerList_Sig = "Ljava/util/List;";
    inline constexpr const char* ServerData_serverIP      = "serverIP";
    inline constexpr const char* ServerData_serverIP_Sig  = "Ljava/lang/String;";

    // â”€â”€â”€ Timer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* Timer_msPerTick     = "timerSpeed";
    inline constexpr const char* Timer_msPerTick_Sig = "F";

    // â”€â”€â”€ ItemStack â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    inline constexpr const char* ItemStack_Class      = "net/minecraft/item/ItemStack";
    inline constexpr const char* ItemStack_getItem    = "getItem";
    inline constexpr const char* ItemStack_getItem_Sig = "()Lnet/minecraft/item/Item;";
    inline constexpr const char* ItemStack_getCount   = "getCount";
    inline constexpr const char* ItemStack_getCount_Sig = "()I";


    // ═══════════════════════════════════════════════════════════════
    // AUTO-GENERATED: 1.12.2 MCP equivalents (verify before using!)
    // ═══════════════════════════════════════════════════════════════

    inline constexpr const char* Entity_isAlive = "isEntityAlive";
    inline constexpr const char* Entity_isAlive_Sig = "()Z";
    inline constexpr const char* Entity_getDeltaMovement = "getPositionVector";
    inline constexpr const char* Entity_getDeltaMovement_Sig = "()Lnet/minecraft/util/math/Vec3d;";
    inline constexpr const char* Entity_setDeltaMovement = "setVelocity";
    inline constexpr const char* Entity_setDeltaMovement_Sig = "(DDD)V";
    inline constexpr const char* Entity_getYRot = "rotationYaw";
    inline constexpr const char* Entity_getYRot_Sig = "F";
    inline constexpr const char* Entity_getXRot = "rotationPitch";
    inline constexpr const char* Entity_getXRot_Sig = "F";
    inline constexpr const char* Entity_setYRot = "rotationYaw";
    inline constexpr const char* Entity_setYRot_Sig = "F";
    inline constexpr const char* Entity_setXRot = "rotationPitch";
    inline constexpr const char* Entity_setXRot_Sig = "F";
    inline constexpr const char* Entity_getName = "getName";
    inline constexpr const char* Entity_getName_Sig = "()Ljava/lang/String;";
    inline constexpr const char* Entity_getScoreboardName = "getName";
    inline constexpr const char* Entity_getScoreboardName_Sig = "()Ljava/lang/String;";
    inline constexpr const char* Entity_setSharedFlag = "setFlag";
    inline constexpr const char* Entity_setSharedFlag_Sig = "(IZ)V";
    inline constexpr const char* Entity_getSharedFlag = "getFlag";
    inline constexpr const char* Entity_getSharedFlag_Sig = "(I)Z";
    inline constexpr const char* Player_getGameProfile = "getGameProfile";
    inline constexpr const char* Player_getGameProfile_Sig = "()Lcom/mojang/authlib/GameProfile;";
    inline constexpr const char* GameProfile_getName = "getName";
    inline constexpr const char* GameProfile_getName_Sig = "()Ljava/lang/String;";
    inline constexpr const char* Level_players = "players";
    inline constexpr const char* Level_players_Sig = "Ljava/util/List;";
    inline constexpr const char* Level_getSeed = "getSeed";
    inline constexpr const char* Level_getSeed_Sig = "()J";
    inline constexpr const char* Level_getBlockState = "getBlockState";
    inline constexpr const char* Level_getBlockState_Sig = "(Lnet/minecraft/util/math/BlockPos;)Lnet/minecraft/block/state/IBlockState;";
    inline constexpr const char* Level_isLoaded = "isLoaded";
    inline constexpr const char* Level_isLoaded_Sig = "(Lnet/minecraft/util/math/BlockPos;)Z";
    inline constexpr const char* BuiltInRegistries_Class = "net/minecraft/util/registry/RegistryNamespaced";
    inline constexpr const char* BuiltInRegistries_BLOCK = "BLOCK";
    inline constexpr const char* BuiltInRegistries_BLOCK_Sig = "Lnet/minecraft/util/registry/RegistryNamespaced;";
    inline constexpr const char* Registry_Class = "net/minecraft/util/registry/RegistryNamespaced";
    inline constexpr const char* Registry_getKey = "getKey";
    inline constexpr const char* Registry_getKey_Sig = "(Ljava/lang/Object;)Lnet/minecraft/util/ResourceLocation;";
    inline constexpr const char* Level_dimension = "dimension";
    inline constexpr const char* Level_dimension_Sig = "Lnet/minecraft/util/ResourceLocation;";
    inline constexpr const char* ResourceKey_Class = "net/minecraft/util/ResourceLocation";
    inline constexpr const char* ResourceKey_location = "location";
    inline constexpr const char* ResourceKey_location_Sig = "()Lnet/minecraft/util/ResourceLocation;";
    inline constexpr const char* Identifier_Class = "net/minecraft/util/ResourceLocation";
    inline constexpr const char* Identifier_getPath = "getPath";
    inline constexpr const char* Identifier_getPath_Sig = "()Ljava/lang/String;";
    inline constexpr const char* MutableBlockPos_Class = "net/minecraft/util/math/BlockPos$MutableBlockPos";
    inline constexpr const char* MutableBlockPos_Init = "<init>";
    inline constexpr const char* MutableBlockPos_Init_Sig = "(III)V";
    inline constexpr const char* MutableBlockPos_set = "set";
    inline constexpr const char* MutableBlockPos_set_Sig = "(III)Lnet/minecraft/util/math/BlockPos$MutableBlockPos;";
    inline constexpr const char* BlockState_Class = "net/minecraft/block/state/IBlockState";
    inline constexpr const char* BlockState_getBlock = "getBlock";
    inline constexpr const char* BlockState_getBlock_Sig = "()Lnet/minecraft/block/Block;";
    inline constexpr const char* Entity_isInWater = "isInWater";
    inline constexpr const char* Entity_isInWater_Sig = "()Z";
    inline constexpr const char* Entity_horizontalCollision = "horizontalCollision";
    inline constexpr const char* Entity_horizontalCollision_Sig = "Z";
    inline constexpr const char* Entity_wasTouchingWater = "wasTouchingWater";
    inline constexpr const char* Entity_wasTouchingWater_Sig = "Z";
    inline constexpr const char* Entity_isShiftKeyDown = "isSneaking";
    inline constexpr const char* Entity_isShiftKeyDown_Sig = "()Z";
    inline constexpr const char* LivingEntity_discardFriction = "discardFriction";
    inline constexpr const char* LivingEntity_discardFriction_Sig = "Z";
    inline constexpr const char* LivingEntity_jumping = "jumping";
    inline constexpr const char* LivingEntity_jumping_Sig = "Z";
    inline constexpr const char* Block_Class = "net/minecraft/block/Block";
    inline constexpr const char* Block_getName = "getName";
    inline constexpr const char* Block_getName_Sig = "()Lnet/minecraft/util/text/ITextComponent;";
    inline constexpr const char* Block_getDescriptionId = "getDescriptionId";
    inline constexpr const char* Block_getDescriptionId_Sig = "()Ljava/lang/String;";
    inline constexpr const char* List_size = "size";
    inline constexpr const char* List_size_Sig = "()I";
    inline constexpr const char* List_get = "get";
    inline constexpr const char* List_get_Sig = "(I)Ljava/lang/Object;";
    inline constexpr const char* Vec3_x = "x";
    inline constexpr const char* Vec3_y = "y";
    inline constexpr const char* Vec3_z = "z";
    inline constexpr const char* Vec3_D_Sig = "D";
    inline constexpr const char* AABB_minX = "minX";
    inline constexpr const char* AABB_minY = "minY";
    inline constexpr const char* AABB_minZ = "minZ";
    inline constexpr const char* AABB_maxX = "maxX";
    inline constexpr const char* AABB_maxY = "maxY";
    inline constexpr const char* AABB_maxZ = "maxZ";
    inline constexpr const char* AABB_D_Sig = "D";
    inline constexpr const char* AABB_inflate = "inflate";
    inline constexpr const char* AABB_inflate_Sig = "(DDD)Lnet/minecraft/util/math/AxisAlignedBB;";
    inline constexpr const char* RS_getModelViewMatrix = "getModelViewMatrix";
    inline constexpr const char* RS_getModelViewMatrix_Sig = "()Lorg/joml/Matrix4f;";
    inline constexpr const char* RS_getProjectionMatrix = "getProjectionMatrix";
    inline constexpr const char* RS_getProjectionMatrix_Sig = "()Lorg/joml/Matrix4f;";
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
    inline constexpr const char* ClientPacketListener_Class = "net/minecraft/client/network/NetHandlerPlayClient";
    inline constexpr const char* ServerboundInteractPacket_Class = "net/minecraft/network/play/client/CPacketUseEntity";
    inline constexpr const char* MovePacketPos_Init = "<init>";
    inline constexpr const char* InteractPacket_createAttackPacket = "createAttackPacket";
    inline constexpr const char* InteractPacket_createAttackPacket_Sig = "(Lnet/minecraft/entity/Entity;Z)Lnet/minecraft/network/play/client/CPacketUseEntity;";
    inline constexpr const char* HitResult_Class = "net/minecraft/util/math/RayTraceResult";
    inline constexpr const char* HitResult_getType = "getType";
    inline constexpr const char* HitResult_getType_Sig = "()Lnet/minecraft/world/phys/HitResult$Type;";
    inline constexpr const char* Player_attackStrengthTicker = "attackStrengthTicker";
    inline constexpr const char* Player_attackStrengthTicker_Sig = "I";
    inline constexpr const char* LivingEntity_isDeadOrDying = "isDeadOrDying";
    inline constexpr const char* LivingEntity_isDeadOrDying_Sig = "()Z";
    inline constexpr const char* LivingEntity_attackStrengthTicker = "attackStrengthTicker";
    inline constexpr const char* LivingEntity_attackStrengthTicker_Sig = "I";
    inline constexpr const char* Player_getOffhandItem = "getOffhandItem";
    inline constexpr const char* Player_getOffhandItem_Sig = "()Lnet/minecraft/item/ItemStack;";
    inline constexpr const char* LivingEntity_hurtTime = "hurtTime";
    inline constexpr const char* LivingEntity_hurtTime_Sig = "I";
    inline constexpr const char* Minecraft_Class_Sig = "Lnet/minecraft/client/Minecraft;";
    inline constexpr const char* MC_instance_Sig = "Lnet/minecraft/client/Minecraft;";
    inline constexpr const char* Options_fov_Sig = "Lnet/minecraft/client/settings/GameSettings;";
    inline constexpr const char* OptionInstance_Class = "net/minecraft/client/settings/GameSettings";
    inline constexpr const char* OptionInstance_get = "get";
    inline constexpr const char* OptionInstance_get_Sig = "()Ljava/lang/Object;";
    inline constexpr const char* MC_window = "window";
    inline constexpr const char* MC_window_Sig = "Lcom/mojang/blaze3d/platform/Window;";
    inline constexpr const char* Window_Class = "com/mojang/blaze3d/platform/Window";
    inline constexpr const char* Window_getWidth = "getWidth";
    inline constexpr const char* Window_getWidth_Sig = "()I";
    inline constexpr const char* Window_getHeight = "getHeight";
    inline constexpr const char* Window_getHeight_Sig = "()I";
    inline constexpr const char* Component_getString = "getString";
    inline constexpr const char* Component_getString_Sig = "()Ljava/lang/String;";
    inline constexpr const char* Vec3_Init = "<init>";
    inline constexpr const char* Vec3_Init_Sig = "(DDD)V";
    inline constexpr const char* ClientLevel_entitiesForRendering = "entitiesForRendering";
    inline constexpr const char* ClientLevel_entitiesForRendering_Sig = "()Ljava/lang/Iterable;";
    inline constexpr const char* MinecartChest_Class_New = "net/minecraft/world/entity/vehicle/minecart/MinecartChest";
    inline constexpr const char* GameMode_startDestroyBlock = "startDestroyBlock";
    inline constexpr const char* GameMode_startDestroyBlock_Sig = "(Lnet/minecraft/util/math/BlockPos;Lnet/minecraft/util/EnumFacing;)Z";
    inline constexpr const char* GameMode_continueDestroyBlock = "continueDestroyBlock";
    inline constexpr const char* GameMode_continueDestroyBlock_Sig = "(Lnet/minecraft/util/math/BlockPos;Lnet/minecraft/util/EnumFacing;)Z";
    inline constexpr const char* GameMode_stopDestroyBlock = "stopDestroyBlock";
    inline constexpr const char* GameMode_stopDestroyBlock_Sig = "()V";
    inline constexpr const char* GameMode_attack_Sig_Legacy = "(Lnet/minecraft/entity/Entity;)V";
    inline constexpr const char* GameMode_useItemOn = "useItemOn";
    inline constexpr const char* GameMode_useItemOn_Sig = "(Lnet/minecraft/client/entity/EntityPlayerSP;Lnet/minecraft/util/EnumHand;Lnet/minecraft/util/math/RayTraceResult;)Lnet/minecraft/util/EnumActionResult;";
    inline constexpr const char* BlockHitResult_Class = "net/minecraft/util/math/RayTraceResult";
    inline constexpr const char* BlockHitResult_Init = "<init>";
    inline constexpr const char* BlockHitResult_Init_Sig = "(Lnet/minecraft/util/math/Vec3d;Lnet/minecraft/util/EnumFacing;Lnet/minecraft/util/math/BlockPos;Z)V";
    inline constexpr const char* InteractionResult_Class = "net/minecraft/util/EnumActionResult";
    inline constexpr const char* InteractionResult_consumesAction = "consumesAction";
    inline constexpr const char* InteractionResult_consumesAction_Sig = "()Z";
    inline constexpr const char* BlockPos_Class = "net/minecraft/util/math/BlockPos";
    inline constexpr const char* BlockPos_Init = "<init>";
    inline constexpr const char* BlockPos_Init_Sig = "(III)V";
    inline constexpr const char* Direction_Class = "net/minecraft/util/EnumFacing";
    inline constexpr const char* Direction_Class_Sig = "Lnet/minecraft/util/EnumFacing;";
    inline constexpr const char* Direction_DOWN = "DOWN";
    inline constexpr const char* Direction_DOWN_Sig = "Lnet/minecraft/util/EnumFacing;";
    inline constexpr const char* Direction_UP   = "UP";
    inline constexpr const char* Direction_UP_Sig = "Lnet/minecraft/util/EnumFacing;";
    inline constexpr const char* Player_inventory = "inventory";
    inline constexpr const char* Player_inventory_Sig = "Lnet/minecraft/entity/player/InventoryPlayer;";
    inline constexpr const char* Inventory_Class = "net/minecraft/entity/player/InventoryPlayer";
    inline constexpr const char* Inventory_getSelectedSlot = "getSelectedSlot";
    inline constexpr const char* Inventory_getSelectedSlot_Sig = "()I";
    inline constexpr const char* Inventory_setSelectedSlot = "setSelectedSlot";
    inline constexpr const char* Inventory_setSelectedSlot_Sig = "(I)V";
    inline constexpr const char* Inventory_getItem = "getItem";
    inline constexpr const char* Inventory_getItem_Sig = "(I)Lnet/minecraft/item/ItemStack;";
    inline constexpr const char* Inventory_getContainerSize = "getContainerSize";
    inline constexpr const char* Inventory_getContainerSize_Sig = "()I";
    inline constexpr const char* ItemStack_isEmpty = "isEmpty";
    inline constexpr const char* ItemStack_isEmpty_Sig = "()Z";
    inline constexpr const char* ItemStack_getHoverName = "getHoverName";
    inline constexpr const char* ItemStack_getHoverName_Sig = "()Lnet/minecraft/util/text/ITextComponent;";
    inline constexpr const char* Item_Class = "net/minecraft/item/Item";
    inline constexpr const char* BlockItem_Class = "net/minecraft/item/ItemBlock";
    inline constexpr const char* BlockStateBase_isAir = "isAir";
    inline constexpr const char* BlockStateBase_isAir_Sig = "()Z";
    inline constexpr const char* Player_containerMenu = "containerMenu";
    inline constexpr const char* Player_containerMenu_Sig = "Lnet/minecraft/inventory/Container;";
    inline constexpr const char* Player_inventoryMenu = "inventoryMenu";
    inline constexpr const char* Player_inventoryMenu_Sig = "Lnet/minecraft/inventory/ContainerPlayer;";
    inline constexpr const char* AbstractContainerMenu_Class = "net/minecraft/inventory/Container";
    inline constexpr const char* AbstractContainerMenu_containerId = "containerId";
    inline constexpr const char* AbstractContainerMenu_containerId_Sig = "I";
    inline constexpr const char* ClickType_Class = "net/minecraft/inventory/ClickType";
    inline constexpr const char* ClickType_Class_Sig = "Lnet/minecraft/inventory/ClickType;";
    inline constexpr const char* ClickType_QUICK_MOVE = "QUICK_MOVE";
    inline constexpr const char* ClickType_QUICK_MOVE_Sig = "Lnet/minecraft/inventory/ClickType;";
    inline constexpr const char* ClickType_SWAP = "SWAP";
    inline constexpr const char* ClickType_SWAP_Sig = "Lnet/minecraft/inventory/ClickType;";
    inline constexpr const char* GameMode_handleInventoryMouseClick = "handleInventoryMouseClick";
    inline constexpr const char* GameMode_handleInventoryMouseClick_Sig = "(IIILnet/minecraft/inventory/ClickType;Lnet/minecraft/entity/player/EntityPlayer;)V";
    inline constexpr const char* MC_getDeltaTracker = "getDeltaTracker";
    inline constexpr const char* MC_getDeltaTracker_Sig = "()Lnet/minecraft/util/Timer;";
    inline constexpr const char* Timer_Class = "net/minecraft/util/Timer";
    inline constexpr const char* GameMode_destroyDelay = "destroyDelay";
    inline constexpr const char* GameMode_destroyDelay_Sig = "I";
    inline constexpr const char* ServerData_ip = "ip";
    inline constexpr const char* ServerData_ip_Sig = "Ljava/lang/String;";

}  // namespace Mappings
