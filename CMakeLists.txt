cmake_minimum_required(VERSION 3.20)

# ===== Project =====
project(RogueLikeGame VERSION 1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Wyjścia binarek do build/<cfg>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
        src/main.cpp
        src/app/VulkanImGuiApp.cpp
        src/app/Window.cpp
        src/app/Instance.cpp
        src/app/Device.cpp
        src/app/Swapchain.cpp
        src/app/SyncAndDescriptors.cpp
        src/app/ImGuiInit.cpp
)

# Jeśli masz własne nagłówki w ./include
target_include_directories(RogueLikeGame PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ===== Dependencies z vcpkg (opcjonalne) =====
if(ENABLE_VCPKG_DEPS)
    # GLFW
    find_package(glfw3 CONFIG REQUIRED)

    # GLM
    find_package(glm CONFIG REQUIRED)

    # Vulkan (loader + headers). W systemie musisz mieć runtime/sterownik Vulkan.
    find_package(Vulkan REQUIRED)

    # ImGui z backendami (w vcpkg włącz: imgui[glfw-binding,vulkan-binding])
    find_package(imgui CONFIG REQUIRED)

    # Linkowanie
    target_link_libraries(RogueLikeGame PRIVATE
            imgui::imgui
            glfw              # z pakietu glfw3
            Vulkan::Vulkan
            glm::glm
    )
else()
    message(STATUS "ENABLE_VCPKG_DEPS=OFF -> budujemy bez zewnętrznych bibliotek (Hello World)")
endif()

# ===== Warnings / kompilator =====
if(MSVC)
    target_compile_options(RogueLikeGame PRIVATE /W4 /permissive- /Zc:preprocessor)
else()
    target_compile_options(RogueLikeGame PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ===== Definitions przydatne dla ImGui backendów (opcjonalnie) =====
# Wcześniej było IMGUI_DISABLE_OBSOLETE_FUNCTIONS; usunięto, aby uniknąć ABI mismatch
# z prekompilowaną biblioteką imgui z vcpkg (assert: "Mismatched struct layout!").
# Jeśli chcesz użyć takich makr, skompiluj również imgui z identycznymi ustawieniami.

# ===== Windows: kopiowanie dll (opcjonalnie) =====
# Jeśli potrzeba, możesz dodać reguły kopiujące wymagane .dll do folderu bin.
# Na start zwykle nie jest to konieczne, bo glfw z vcpkg linkuje statycznie
# (w zależności od tripletu/ustawień).
if(WIN32 AND ENABLE_VCPKG_DEPS)
    # Copy all runtime DLLs from vcpkg installed bin folder to the target's output dir
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        if(EXISTS "${_VCPKG_BIN_DIR}")
            add_custom_command(TARGET RogueLikeGame POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${_VCPKG_BIN_DIR}" "$<TARGET_FILE_DIR:RogueLikeGame>"
                COMMENT "Copying vcpkg runtime DLLs to output directory")
        endif()
    endif()
endif()
