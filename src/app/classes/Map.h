#pragma once
#include <string>
#include <vector>
#include <functional>

class Assets;

enum class GatewaySide {
    Top,
    Bottom,
    Left,
    Right
};

struct Gateway {
    int targetMapIndex;
    float posX;
    float posY;
    GatewaySide side;
};

class Map {
public:
    Map() = default;
    bool loadFromFile(const std::string& path);
    int getRows() const { return rows; };
    int getColumns() const {return columns; };
    char tileAt(int r, int c) const;
    void forEachTile(const std::function<void(int,int,char)>& fn) const;
    void addGateway(int targetIndex, float posX, float posY) {
        gateways_.push_back(Gateway{ targetIndex, posX, posY, GatewaySide::Top });
    }
    const std::vector<Gateway>& gateways() const { return gateways_; }
    GatewaySide gatewaySide(int gatewayIndex) {
        return gateways_[gatewayIndex].side;
    }
    void setGatewaySide(int gatewayIndex, GatewaySide side) {
        if (gatewayIndex >= 0 && gatewayIndex < static_cast<int>(gateways_.size())) {
            gateways_[gatewayIndex].side = side;
        }
    }
private:
 int rows = 0;
    int columns = 0;
    std::vector<std::vector<char>> grid;
    std::vector<Gateway> gateways_;
};