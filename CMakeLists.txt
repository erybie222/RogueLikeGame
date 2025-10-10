cmake_minimum_required(VERSION 3.20)

project(RogueLikeGame VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ODKOMENTOWAĆ KIEDY DODAMY INCLUDE
# target_include_directories(RogueLikeGame PRIVATE include)

add_executable(RogueLikeGame
        src/main.cpp
)

add_custom_target(run
        COMMAND $<TARGET_FILE:RogueLikeGame>
        DEPENDS RogueLikeGame
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
