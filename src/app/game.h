#pragma once
#include <imgui.h>
#include <cstdint>

class Entity {
public:
	Entity(int spriteId, uint32_t width, uint32_t height, float pos_x, float pos_y);

	ImVec2 getPosition() const;
	uint32_t getWidth() const;
	uint32_t getHeight() const;
	int getSpriteId() const;
private:
	int spriteId;
	uint32_t width = 0;
	uint32_t height = 0;
	ImVec2 pos{ 0.0f, 0.0f };
	bool visible = true;
};

