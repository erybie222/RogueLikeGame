#include "Game.h"
#include "Assets.h"
#include <vector>
#include "GameSetup.h"

Entity* spawn(std::vector<Entity*>& entities, Assets* assets, const char* path, uint32_t width, uint32_t height, float posX, float posY)
{
    int spriteId = assets->getOrLoad(path);
    Entity* e = new Entity(spriteId, width, height, posX, posY);
    entities.push_back(e);
    return e;
}

void setupGameEntities(std::vector<Entity*>& entities, Assets* assets)
{
    spawn(entities, assets, "assets/characters/hero.png", 64, 64, 256.0f, 256.0f);
    spawn(entities, assets, "assets/characters/angel.png", 64, 64, 400.0f, 256.0f);
    spawn(entities, assets, "assets/characters/angel.png", 64, 64, 400.0f, 400.0f);
}