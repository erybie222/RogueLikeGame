#include "Game.h"

Entity::Entity(int spriteId, uint32_t width, uint32_t height, float pos_x, float pos_y) :
	spriteId(spriteId), width(width), height(height), pos{ pos_x,pos_y } {}

ImVec2 Entity::getPosition() const {
	return pos;
}
uint32_t Entity::getWidth() const {
	return width;
}
uint32_t Entity::getHeight() const {
	return height;
}

int Entity::getSpriteId() const {
	return spriteId;
}
