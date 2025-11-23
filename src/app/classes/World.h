#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include "StaticEntity.h"
#include "Map.h"
#include "Entity.h"
#include "npc/Npc.h"
#include "Player.h"

class Assets;
class StaticEntity;
class Entity;
class Npc;
class World {
public:
    explicit World(Assets* assets) noexcept : assets_(assets) {}


    StaticEntity& spawnTile(const std::string& texturePath,
                      uint32_t width, uint32_t height,
                      float pos_x, float pos_y, bool solid);

    Player& spawnPlayer(const std::string& texturePath,
                        uint32_t width, uint32_t height,
                        float pos_x, float pos_y, int maxHp);

    Npc& spawnNpc(const std::string& texturePath,
                        uint32_t width, uint32_t height,
                        float pos_x, float pos_y, int maxHp);

    void buildFromMap(const std::string& wallTexturePath,
        const std::string& floorTexturePath,
        const std::string& doorTexturePath,
        uint32_t tileW, uint32_t tileH);

    void update(float dt);
    void clear();
	void newScene();

    void setScreenBounds(float width, float height) { 
        screenWidth_ = width; 
     screenHeight_ = height; 
    }

    Map* getMap(size_t index) const noexcept {
        if (index < maps_.size()) return maps_[index].get();
        return nullptr;
	}
    Player* getPlayer() const noexcept { return player_.get(); }
    const std::vector<std::unique_ptr<Entity>>& entities() const noexcept { return entities_; }
    void forEachEntity(const std::function<void(Entity&)>& fn);
    bool remove(Entity* ptr);
	void setCurrentMapIndex(int index) { currentMapIndex = index; }
	int getCurrentMapIndex() const { return currentMapIndex; }
    void addMap(const std::string& path);
    int playerInGateway();
private:
    template <class T, class... Args>
    T& addEntity(Args&&... args) {
        auto up = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *up;
        entities_.emplace_back(std::move(up));
        return ref;
    }

    // Collision helpers (renamed for clarity)
    static bool intersectsAABB(const Entity& a, const Entity& b);
    static bool intersectsAABBAt(const Entity& a, const Entity& b, float aPosX, float aPosY);
    static void moveWithCollisions(Entity& mover, float dx, float dy, const std::vector<std::unique_ptr<Entity>>& entities);
    static void pushOutOfSolids(Entity& mover, const std::vector<std::unique_ptr<Entity>>& entities);
    void clampToScreen(Entity& mover);
    GatewaySide getSide(int gatewayIndex);
    void spawnPlayerInNewScene(GatewaySide entrySide, float sourceGatewayX, float sourceGatewayY);
    int gatewayIndex = -1;
	int currentMapIndex = 0;
    std::vector<std::unique_ptr<Map>> maps_;
 Assets* assets_{nullptr};
    std::vector<std::unique_ptr<Entity>> entities_;
    std::unique_ptr<Player> player_{nullptr}; 
    float screenWidth_ = 0.0f;
    float screenHeight_ = 0.0f;
};
