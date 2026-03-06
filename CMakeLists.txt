cmake_minimum_required(VERSION 3.20)

# ===== Project =====
project(RogueLikeGame VERSION 1.0 LANGUAGES CXX)

# Przydatne dla indeksowania/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Wyjścia binarek do build/<cfg>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ===== Opcje jakości =====
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ===== Opcjonalne zależności z vcpkg =====
# Automatycznie włącz, jeśli używany jest toolchain vcpkg; w przeciwnym razie wyłącz.
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_USE_VCPKG_DEFAULT ON)
else()
    set(_USE_VCPKG_DEFAULT OFF)
endif()
option(ENABLE_VCPKG_DEPS "Find and link deps via vcpkg (GLFW, GLM, Vulkan, ImGui)" ${_USE_VCPKG_DEFAULT})

# ===== Sources =====
# Dodaj swoje pliki źródłowe tutaj (na start mamy tylko main.cpp)
add_executable(RogueLikeGame
        src/app/main.cpp

        src/engine/gfx/VulkanImGuiApp.cpp
        src/engine/vk/Window.cpp
        src/engine/vk/Instance.cpp
        src/engine/vk/Device.cpp
        src/engine/vk/Swapchain.cpp
        src/engine/vk/SyncAndDescriptors.cpp
        src/engine/gfx/ImGuiInit.cpp
        src/game/entities/Entity.cpp
        src/engine/gfx/Assets.cpp
        src/app/GameSetup.cpp
        src/game/entities/Player.cpp
        src/game/world/Map.cpp
        src/game/npc/Npc.cpp
        src/game/entities/LivingEntity.cpp
        src/game/world/World.cpp
        src/game/npc/INpcController.cpp
        src/game/npc/INpcController.h
        src/game/npc/MeleeController.cpp
        src/game/npc/MeleeController.h
        src/utils/MathUtils.h
        src/game/entities/animation/AnimationController.cpp
        src/game/npc/RangeController.cpp
        src/game/npc/RangeController.h
        src/game/entities/Projectile.cpp
        src/game/entities/Projectile.h
        src/game/combat/CombatUtils.h
        src/game/combat/CombatUtils.cpp
        src/game/factory/NpcFactory.cpp
        src/game/factory/NpcFactory.h
        src/game/factory/PlayerFactory.cpp
        src/game/factory/PlayerFactory.h
        src/game/item/Item.h
        src/game/item/Equipment.h
        src/game/item/Consumable.h
        src/game/item/Usable.h
        src/game/item/consumable/HealthPotion.h
        src/game/item/consumable/StrengthPotion.h
        src/game/item/consumable/SpeedPotion.h
        src/game/item/Inventory.cpp
        src/game/item/Inventory.h
        src/game/factory/ItemFactory.cpp
        src/game/factory/ItemFactory.h
        src/game/item/usable/Key.h
        src/app/AiServerApp.cpp
)

# Katalogi z nagłówkami
# Dodajemy główny katalog src - wszystkie include'y będą relatywne do niego
target_include_directories(RogueLikeGame PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ===== Dependencies z vcpkg (opcjonalne) =====
if(ENABLE_VCPKG_DEPS)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(Vulkan REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(cppzmq CONFIG REQUIRED)

    find_path(TMXLITE_INCLUDE_DIR NAMES tmxlite/Map.hpp)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        find_library(TMXLITE_LIBRARY NAMES tmxlite-d tmxlite)
    else()
        find_library(TMXLITE_LIBRARY NAMES tmxlite)
    endif()
    if(NOT TMXLITE_INCLUDE_DIR OR NOT TMXLITE_LIBRARY)
        message(FATAL_ERROR "Nie znaleziono tmxlite. Zainstaluj port 'tmxlite' (vcpkg.json) i zrób reconfigure.")
    endif()
    set(TMXLITE_INCLUDE_DIRS ${TMXLITE_INCLUDE_DIR})
    set(TMXLITE_LINK_LIBRARIES ${TMXLITE_LIBRARY})

    # ZAMIENNIK za find_package(stb CONFIG REQUIRED):
    # Szukamy katalogu zawierającego stb_image.h (często jest to <vcpkg>/installed/<triplet>/include/stb)
    find_path(STB_INCLUDE_DIR
        NAMES stb_image.h
        PATH_SUFFIXES stb
    )
    if(NOT STB_INCLUDE_DIR)
        message(FATAL_ERROR "Nie znaleziono stb_image.h. Zainstaluj port 'stb' (vcpkg.json) i zrób reconfigure.")
    endif()

    target_link_libraries(RogueLikeGame PRIVATE
            imgui::imgui
            glfw
            Vulkan::Vulkan
            glm::glm
            ${TMXLITE_LINK_LIBRARIES}
            cppzmq
    )

    # Dodaj include dla stb, tmxlite i Vulkan/vcpkg (jawnie, dla VS)
    target_include_directories(RogueLikeGame PRIVATE ${STB_INCLUDE_DIR})
    target_include_directories(RogueLikeGame PRIVATE ${TMXLITE_INCLUDE_DIRS})
    if(DEFINED Vulkan_INCLUDE_DIR)
        target_include_directories(RogueLikeGame PRIVATE ${Vulkan_INCLUDE_DIR})
    endif()
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        target_include_directories(RogueLikeGame PRIVATE ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)
    endif()
else()
    message(STATUS "ENABLE_VCPKG_DEPS=OFF -> budujemy bez zewnętrznych bibliotek (Hello World)")
endif()

# ===== Warnings / kompilator =====
if(MSVC)
    target_compile_options(RogueLikeGame PRIVATE /W4 /permissive- /Zc:preprocessor)
    if(WARNINGS_AS_ERRORS)
        target_compile_options(RogueLikeGame PRIVATE /WX)
    endif()
    # Do not use /external:I for include dirs; put all in AdditionalIncludeDirectories for compatibility
    set_property(TARGET RogueLikeGame PROPERTY MSVC_USE_EXTERNAL_INCLUDE_DIRS OFF)
else()
    target_compile_options(RogueLikeGame PRIVATE -Wall -Wextra -Wpedantic)
    if(WARNINGS_AS_ERRORS)
        target_compile_options(RogueLikeGame PRIVATE -Werror)
    endif()
endif()

# ===== Platformowe definicje =====
if(WIN32)
    target_compile_definitions(RogueLikeGame PRIVATE VK_USE_PLATFORM_WIN32_KHR NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ===== Testy (smoke test) =====
include(CTest)
if(BUILD_TESTING)
    enable_testing()
    # Uruchamia executable z parametrem --smoke
    add_test(NAME smoke
        COMMAND $<TARGET_FILE:RogueLikeGame> --smoke
        WORKING_DIRECTORY $<TARGET_FILE_DIR:RogueLikeGame>)
endif()

# ===== Windows: kopiowanie dll (opcjonalnie) =====
# Jeśli potrzeba, możesz dodać reguły kopiujące wymagane .dll do folderu bin.
# Na start zwykle nie jest to konieczne, bo glfw z vcpkg linkuje statycznie
# (w zależności od tripletu/ustawień).
if(WIN32 AND ENABLE_VCPKG_DEPS)
    # Copy all runtime DLLs from vcpkg installed bin folder to the target's output dir
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        # Dla Debug kopiuj z debug/bin, dla Release z bin
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(_VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
        else()
            set(_VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
        if(EXISTS "${_VCPKG_BIN_DIR}")
            add_custom_command(TARGET RogueLikeGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${_VCPKG_BIN_DIR}" "$<TARGET_FILE_DIR:RogueLikeGame>"
                COMMENT "Copying vcpkg runtime DLLs to output directory")
        endif()
    endif()
endif()

# ===== Kopiowanie assets do katalogu bin (runtime) =====
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET RogueLikeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:RogueLikeGame>/assets"
        COMMENT "Copying assets to output directory")
endif()

