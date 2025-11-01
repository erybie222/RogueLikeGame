#pragma once
#include <vector>

class Entity;
class Assets;

void setupGameEntities(std::vector<Entity*>& entities, Assets* assets);

Entity* spawn(std::vector<Entity*>& entities, Assets* assets, const char* path, uint32_t width, uint32_t height, float posX, float posY);