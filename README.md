# RogueLike Dungeon Crawler

A roguelike game written in **C++20** with **Vulkan** rendering and **ImGui** interface.

[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)](https://www.vulkan.org/)
[![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)](https://cmake.org/)

---

## About the Project

**RogueLike Dungeon Crawler** is a top-down action game where the player explores dungeons, fights enemies, and collects items. The project demonstrates advanced game programming techniques in C++.

### Key Features

- **Combat system** - melee and ranged attacks with animations
- **Map editor** - levels created in [Tiled](https://www.mapeditor.org/) and loaded from TMX files
- **Inventory system** - collecting and using items (potions, keys)
- **Interactive elements** - locked doors, treasure chests
- **Diverse enemies** - Orcs, Skeleton Archers, Knights with their own AI
- **Roguelike mechanics** - permadeath with respawn capability

---

## Technologies

| Category | Technology |
|----------|------------|
| **Language** | C++20 |
| **Graphics** | Vulkan API |
| **UI** | Dear ImGui |
| **Windowing** | GLFW |
| **Maps** | TMXLite + Tiled Map Editor |
| **Math** | GLM |
| **Build** | CMake + Ninja |
| **Dependencies** | vcpkg |

---

## Architecture

```
src/
├── engine/           # Game engine
│   └── gfx/          # Vulkan rendering, Assets
├── game/
│   ├── entities/     # Entity, Player, Npc, Projectile
│   │   └── animation/# Animation system
│   ├── factory/      # Factories (Player, Npc, Item)
│   ├── item/         # Item and inventory system
│   ├── npc/          # Enemy AI
│   └── world/        # Game world, maps, collisions
├── app/              # Main game loop
└── utils/            # Utility tools
```

### Key Design Patterns:
- **Entity Component System** - flexible entity architecture
- **Factory Pattern** - creation of players, NPCs, and items
- **State Machine** - enemy AI and animation system

---

## Getting Started

### Requirements
- Windows 10/11
- Visual Studio 2022 (Desktop development with C++) or Build Tools 2022
- CMake 3.20+
- Ninja

### Installing Tools

```powershell
# Via winget
winget install -e --id Kitware.CMake
winget install -e --id Ninja-build.Ninja

# Or via Chocolatey
choco install cmake ninja -y
```

### Build (PowerShell)

```powershell
# Clone with submodules
git clone --recursive https://github.com/erybie222/RogueLikeGame.git
cd RogueLikeGame

# Configure and build
cmake --preset win-ninja-debug
cmake --build --preset win-ninja-debug-build

# Run
.\build\win-ninja-debug\bin\RogueLikeGame.exe
```

### Troubleshooting

If the build uses the wrong architecture (x86 instead of x64), load the VS environment:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -host_arch=x64 -arch=x64 && set' |
  ForEach-Object {
    if ($_ -match '^(INCLUDE|LIB|LIBPATH|PATH)=') {
      $n,$v = $_ -split '=',2
      Set-Item -Path Env:$n -Value $v
    }
  }
```

---

## Creating Maps

Maps are created in **Tiled Map Editor** and saved as `.tmx` files in the `assets/maps/` folder.

Each map can contain:
- Tile layers (floors, walls)
- Objects (NPC spawns, chests, doors, map transitions)
- Tile properties (solid, door, locked)

---

## Controls

| Key | Action |
|-----|--------|
| `W A S D` | Movement |
| `Space` | Attack |
| `Q` | Switch attack mode (melee/ranged) |
| `E` | Interact (chests, doors) |
| `1-9` | Use inventory item |
| `ESC` | Pause |

---

## How to Play (Quick Start)

You don't need to compile the project to play. The game has been prepared as a Standalone executable.

1. Go to the [Releases](../../releases) tab on GitHub.
2. Download the latest `.zip` package with the game.
3. Extract the archive anywhere on your computer.
4. Make sure that the `RogueLikeGame.exe` file, the `assets` folder (containing textures), and the other files (`.dll`,`.pdb`) are all located in the same folder.
5. Run the `RogueLikeGame.exe` file and enjoy the game!

---
## Authors

- GitHub: [@erybie222](https://github.com/erybie222)
- GitHub: [@Kendrej](https://github.com/Kendrej)
