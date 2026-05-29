cmake_minimum_required(VERSION 3.16)
project(FrameworkEngine)
set(CMAKE_CXX_STANDARD 17)

option(FW_ENABLE_LUAU "Embed Luau VM" ON)
option(FW_BUILD_BLANK_SAMPLE "Build samples/blank when this repo is the top-level project" OFF)

# Expect SDKs under ./vendor (not all committed). See README.
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/SDL2")
    message(WARNING "vendor/SDL2 not found - place SDL2, Box2D, FMOD, EnTT, spdlog, json, monocypher (and Luau if enabled) under vendor/")
endif()

if (FW_ENABLE_LUAU AND EXISTS "${CMAKE_SOURCE_DIR}/vendor/luau/CMakeLists.txt")
    set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/luau)
    message(STATUS "Luau scripting ENABLED")
endif()

add_subdirectory(framework)

if (FW_BUILD_BLANK_SAMPLE AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if (EXISTS "${CMAKE_SOURCE_DIR}/samples/blank/blank.cpp")
        add_executable(BlankHost samples/blank/blank.cpp)
        target_link_libraries(BlankHost PRIVATE Framework)
    endif()
endif()
