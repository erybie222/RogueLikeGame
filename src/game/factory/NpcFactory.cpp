#include "NpcFactory.h"
#include "../npc/Npc.h"
#include "../npc/MeleeController.h"
#include "../npc/RangeController.h"
#include "game/world/World.h"
#include <memory>

#include "ItemFactory.h"

Npc* NpcFactory::createNpc(NpcType type, World& world, ImVec2 pos)
{
    std::unique_ptr<INpcController> controller;

    int   width  = 64;
    int   height = 64;
    int   maxHp  = 100;

    float aggroRange = 500.0f;
    float maxSpeed   = 250.0f;
    float accel      = 2000.0f;

    int   meleeDamage   = 0;
    float meleeCooldown = 0.0f;
    float meleeRange    = 0.0f;

    int   rangedDamage   = 0;
    float rangedCooldown = 0.0f;
    float rangedRange    = 0.0f;

    std::string animBasePath;
    std::string animPrefix;
    int         animSquareSize = 100;

    int walkFrames  = 8;
    int idleFrames  = 6;
    int hurtFrames  = 4;
    int deathFrames = 4;

    int meleeAttackFrames     = 0;
    int meleeAttackTrigger    = 0;
    int rangedAttackFrames    = 0;
    int rangedAttackTrigger   = 0;

    int attackClass = 0;
    ItemId itemToDropId = ItemId::None;

    switch (type)
    {
        case NpcType::Skeleton_Archer:
            controller        = std::make_unique<RangeController>();
            attackClass =1;
            maxHp             = 10;
            aggroRange        = 600.0f;
            maxSpeed          = 180.0f;
            accel             = 1500.0f;
            rangedDamage      = 10;
            rangedCooldown    = 1.5f;
            rangedRange       = 400.0f;
            animBasePath      = "assets/animations/skeletonArcher/";
            animPrefix        = "SkeletonArcher";
            rangedAttackFrames  = 9;
            rangedAttackTrigger = 7;
            break;

        case NpcType::Orc:
            controller        = std::make_unique<MeleeController>();
            maxHp             = 10;
            aggroRange        = 400.0f;
            maxSpeed          = 200.0f;
            accel             = 2500.0f;
            meleeDamage       = 5;
            meleeCooldown     = 1.0f;
            meleeRange        = 90.0f;
            animBasePath      = "assets/animations/orc/";
            animPrefix        = "Orc";
            meleeAttackFrames   = 6;
            meleeAttackTrigger  = 4;
            break;
        case NpcType::Knight:
            controller        = std::make_unique<MeleeController>();
            maxHp             = 30;
            aggroRange        = 400.0f;
            maxSpeed          = 100.0f;
            accel             = 1000.0f;
            meleeDamage       = 15;
            meleeCooldown     = 1.0f;
            meleeRange        = 90.0f;
            animBasePath      = "assets/animations/knight/";
            animPrefix        = "Knight";
            meleeAttackFrames   = 6;
            meleeAttackTrigger  = 4;
            break;
        case NpcType::Elite_Orc:
            controller         = std::make_unique<MeleeController>();

            maxHp              = 100;
            aggroRange         = 400.0f;
            maxSpeed           = 120.0f;
            accel              = 1000.0f;
            meleeDamage        = 30;
            meleeCooldown      = 1.0f;
            meleeRange         = 160.0f;
            animBasePath       = "assets/animations/eliteOrc/";
            animPrefix         = "EliteOrc";
            meleeAttackFrames  = 9;
            meleeAttackTrigger = 5;
            break;
    }

    auto* npc = new Npc(static_cast<int>(type), width, height, pos.x, pos.y, maxHp);

    npc->setSolid(true);
    npc->setAttackClass(attackClass);
    npc->setController(std::move(controller));
    npc->setWorld(&world);
    npc->setAggroRange(aggroRange);
    npc->setMaxSpeed(maxSpeed);
    npc->setAcceleration(accel);

    npc->setMeleeDamage(meleeDamage);
    npc->setMeleeCooldown(meleeCooldown);
    npc->setMeleeRange(meleeRange);

    npc->setRangedDamage(rangedDamage);
    npc->setRangedCooldown(rangedCooldown);
    npc->setRangedRange(rangedRange);

    npc->setDroppingItem(ItemFactory::createItem(itemToDropId, world.getAssets()));

    npc->createAnimationController(
        world.getAssets(), animSquareSize,
        animBasePath + animPrefix + "-Walk-right.png", walkFrames,
        animBasePath + animPrefix + "-Walk-left.png", walkFrames,
        animBasePath + animPrefix + "-Idle-right.png", idleFrames,
        animBasePath + animPrefix + "-Idle-left.png", idleFrames,
        animBasePath + animPrefix + "-Hurt-right.png", hurtFrames,
        animBasePath + animPrefix + "-Hurt-left.png", hurtFrames,
        animBasePath + animPrefix + "-Death-right.png", deathFrames,
        animBasePath + animPrefix + "-Death-left.png", deathFrames);

    if (meleeAttackFrames > 0)
    {
        npc->createMeleeAttackAnimation(
            animSquareSize, meleeAttackTrigger,
            animBasePath + animPrefix + "-Attack-right.png", meleeAttackFrames,
            animBasePath + animPrefix + "-Attack-left.png", meleeAttackFrames);
    }

    if (rangedAttackFrames > 0)
    {
        npc->createRangedAttackAnimation(
            animSquareSize, rangedAttackTrigger,
            animBasePath + animPrefix + "-RangeAttack-right.png", rangedAttackFrames,
            animBasePath + animPrefix + "-RangeAttack-left.png", rangedAttackFrames);
    }

    return npc;
}
