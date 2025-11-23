#include "World.h"
#include "StaticEntity.h"
#include "Player.h"
#include "Assets.h"
#include <algorithm>
#include <utility>
#include <npc/MeleeController.h>
#include "LivingEntity.h"
#include <stdexcept>
#include <iostream>
#include "Map.h"


StaticEntity& World::spawnTile(const std::string &texturePath, uint32_t width, uint32_t height, float pos_x, float pos_y, bool solid) {
    const int entityId = assets_ ? assets_->getOrLoadIcon(texturePath) : -1;
    return addEntity<StaticEntity>(entityId, width, height, pos_x ,pos_y, solid);
}

Player& World::spawnPlayer(const std::string &texturePath, uint32_t width, uint32_t height, float pos_x, float pos_y, int maxHp) {
    const int playerId = assets_ ? assets_->getOrLoadIcon(texturePath) : -1;
    Player& p = addEntity<Player>(playerId, width , height , pos_x, pos_y,  maxHp);
    player_ = &p;
    p.setSolid(true);

    p.setAttackDamage(5);
    p.setAttackRange(50.0f);
    p.setAttackCooldown(0.7f);
    return p;
}

Npc& World::spawnNpc(const std::string &texturePath, uint32_t width, uint32_t height, float pos_x, float pos_y, int maxHp) {
    const int npcId = assets_ ? assets_->getOrLoadIcon(texturePath) : -1;
    Npc& n= addEntity<Npc>(npcId, width , height , pos_x, pos_y,  maxHp);
    n.setWorld(this);
    n.setController(std::make_unique<MeleeController>());
    n.setSolid(true);
    n.setAttackDamage(5);
    n.setAttackRange(80.0f);
    n.setAttackCooldown(0.7f);
    return n;
}


void World::buildFromMap(const std::string &wallTexturePath, const std::string &floorTexturePath, const std::string& doorTexturePath, uint32_t tileW, uint32_t tileH) {
    if (currentMapIndex >= maps_.size() || !maps_[currentMapIndex]) {
        throw std::runtime_error("Invalid map index or map not loaded");
    }
    
    maps_[currentMapIndex]->forEachTile([&](int i, int j, char t) {
        const float x = j * static_cast<float>(tileW);
        const float y = i * static_cast<float>(tileH);
        if (t == '*') {
            spawnTile(wallTexturePath, tileW, tileH, x, y, true);
        } else if (t == '-') {
            spawnTile(floorTexturePath, tileW, tileH, x, y, false);
        } else if (t >= '0' && t <= '9') {
            // t - '0' is the TARGET map index, not the index in gateways() vector.
            spawnTile(doorTexturePath, tileW, tileH, x, y, false);
            maps_[currentMapIndex]->addGateway(t - '0', x, y);
            int newGatewayIndex = static_cast<int>(maps_[currentMapIndex]->gateways().size()) -1; // index of newly added gateway
            maps_[currentMapIndex]->setGatewaySide(newGatewayIndex, getSide(newGatewayIndex));
        }
    });
}

GatewaySide World::getSide(int gatewayIndex) {
    const Gateway& g = maps_[currentMapIndex]->gateways()[gatewayIndex];
    int tileX = static_cast<int>(g.posX /64.0f); // Assuming64x64 tiles
    int tileY = static_cast<int>(g.posY /64.0f);
    
  if (tileY <=1) return GatewaySide::Top;
 if (tileY >= static_cast<int>(screenHeight_ /64.0f) -2) return GatewaySide::Bottom;
    if (tileX <=1) return GatewaySide::Left;
    if (tileX >= static_cast<int>(screenWidth_ /64.0f) -2) return GatewaySide::Right;

    return GatewaySide::Top;
}

bool World::intersectsAABB(const Entity &a, const Entity &b) {
    ImVec2 ap = a.getPosition();
    ImVec2 bp = b.getPosition();
    return !(ap.x + a.getWidth() <= bp.x || bp.x + b.getWidth() <= ap.x ||
        ap.y + a.getHeight() <= bp.y || bp.y + b.getHeight() <= ap.y);
}

bool World::intersectsAABBAt(const Entity &a, const Entity &b, float ax, float ay) {
    ImVec2 bp = b.getPosition();
    return !(ax + a.getWidth() <= bp.x || bp.x + b.getWidth() <= ax ||
        ay + a.getHeight() <= bp.y || bp.y + b.getHeight() <= ay);
}

void World::moveWithCollisions(Entity &mover, float dx, float dy, const std::vector<std::unique_ptr<Entity>> &entities) {
    // move along X then Y and resolve against solids
    ImVec2 p = mover.getPosition();
    float newX = p.x + dx;
    float newY = p.y;
    for (const auto& up : entities) {
        if (!up || up.get() == &mover) continue;
        if (!up->isSolid()) continue;
        if (intersectsAABBAt(mover, *up, newX, newY)) {
            if (dx >0) {
                newX = up->getPosition().x - mover.getWidth();
            } else if (dx <0) {
                newX = up->getPosition().x + up->getWidth();
            }
        }
    }

    // move Y
    float newY2 = newY + dy;
    for (const auto& up : entities) {
        if (!up || up.get() == &mover) continue;
        if (!up->isSolid()) continue;
        if (intersectsAABBAt(mover, *up, newX, newY2)) {
            if (dy >0) {
                newY2 = up->getPosition().y - mover.getHeight();
            } else if (dy <0) {
                newY2 = up->getPosition().y + up->getHeight();
            }
        }
    }
    mover.setPosition(newX, newY2);
}

void World::pushOutOfSolids(Entity &mover, const std::vector<std::unique_ptr<Entity>> &entities) {
    ImVec2 p = mover.getPosition();
    for (const auto& up : entities) {
        if (!up || up.get() == &mover) continue;
        if (!up->isSolid()) continue;
        if (!intersectsAABB(mover, *up)) continue;
        // Compute minimal axis separation
        ImVec2 op = up->getPosition();
        float dx1 = (op.x + up->getWidth()) - p.x; // push right
        float dx2 = (p.x + mover.getWidth()) - op.x; // push left
        float dy1 = (op.y + up->getHeight()) - p.y; // push down
        float dy2 = (p.y + mover.getHeight()) - op.y; // push up
        // choose smallest move
        float minX = std::min(dx1, dx2);
        float minY = std::min(dy1, dy2);
    if (minX < minY) {
     // resolve horizontally
   if (dx1 < dx2) p.x = op.x + up->getWidth();
          else p.x = op.x - mover.getWidth();
     } else {
    // resolve vertically
            if (dy1 < dy2) p.y = op.y + up->getHeight();
      else p.y = op.y - mover.getHeight();
        }
    }
    mover.setPosition(p.x, p.y);
}


void World::update(float dt) {

    for (auto& up : entities_) {
        if (!up) continue;
        up->update(dt);
    }


    for (auto& up : entities_) {
        if (!up) continue;

        auto* living = dynamic_cast<LivingEntity*>(up.get());
        if (!living) continue;
        ImVec2 velocity = living ->getVelocity();
        float dx = velocity.x * dt;
        float dy = velocity.y * dt;
        moveWithCollisions(*living , dx, dy ,entities_);
    }
}
void World::clear() {
    entities_.clear();
    player_ = nullptr;
}

void World::forEachEntity(const std::function<void(Entity &)> &fn) {
    for ( auto& up: entities_) {
      if (up) fn(*up);
    }
}

bool World::remove(Entity* ptr) {
    if (!ptr) return false;
    
    if (player_.get() == ptr) {
        player_.reset();
        return true;
    }
    
    auto it = std::remove_if(entities_.begin(), entities_.end(),
    [&](const std::unique_ptr<Entity>& up){ return up.get() == ptr; });
    const bool removed = (it != entities_.end());
    entities_.erase(it, entities_.end());
    return removed;
}

void World::addMap(const std::string& path) {
    auto m = std::make_unique<Map>();
    if (!m->loadFromFile(path)) {
        throw std::runtime_error("Could not load map file: " + path);
    }
    maps_.push_back(std::move(m));
}
