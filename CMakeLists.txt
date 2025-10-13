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
# Jeżeli w kodzie korzystasz z backendów ImGui_ImplGlfw/ImGui_ImplVulkan,
# te define'y bywają pomocne; włącz/wyłącz w zależności od potrzeb.
target_compile_definitions(RogueLikeGame PRIVATE
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)

# ===== Windows: kopiowanie dll (opcjonalnie) =====
# Jeśli potrzeba, możesz dodać reguły kopiujące wymagane .dll do folderu bin.
# Na start zwykle nie jest to konieczne, bo glfw z vcpkg linkuje statycznie
# (w zależności od tripletu/ustawień).
