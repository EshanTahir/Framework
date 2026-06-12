# Engine library (no game sources). Linked by host games and samples/blank.

set(FRAMEWORK_SOURCES
    Core/application.cpp
    Core/log.cpp
    Core/entryPoint.cpp
    Core/window.cpp
    Core/paths.cpp
    Core/appIdentity.cpp
    Core/userDataStore.cpp
    Core/assetCache.cpp
    Core/gameSettings.cpp
    Core/glContext.cpp
    Render/gl.cpp
    Events/inputEvent.cpp
    Events/event.cpp
    Events/appEvent.cpp
    Events/keys.cpp
)

add_library(Framework STATIC ${FRAMEWORK_SOURCES})

get_filename_component(FW_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

target_include_directories(Framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FW_ENGINE_ROOT}/vendor/spdlog/include
        ${FW_ENGINE_ROOT}/vendor/spdlog/include/spdlog
        ${FW_ENGINE_ROOT}/vendor/spdlog/include/spdlog/sinks
        ${FW_ENGINE_ROOT}/vendor/SDL2/include/SDL2
        ${FW_ENGINE_ROOT}/vendor/json
)

target_compile_definitions(Framework PUBLIC FW_USE_OPENGL=1 SDL_MAIN_HANDLED)

target_link_directories(Framework PUBLIC
    ${FW_ENGINE_ROOT}/vendor/SDL2/lib
)

target_link_libraries(Framework PUBLIC
    SDL2
    SDL2_image
    SDL2_ttf
    opengl32
)

if (FW_ENABLE_LUAU AND TARGET Luau.VM)
    target_include_directories(Framework PUBLIC
        ${FW_ENGINE_ROOT}/vendor/luau/VM/include
        ${FW_ENGINE_ROOT}/vendor/luau/Compiler/include
        ${FW_ENGINE_ROOT}/vendor/luau/Ast/include
        ${FW_ENGINE_ROOT}/vendor/luau/Common/include
        ${FW_ENGINE_ROOT}/vendor/luau/Bytecode/include
    )
    target_link_libraries(Framework PUBLIC Luau.VM Luau.Compiler Luau.Ast Luau.Bytecode Luau.Common)
    target_compile_definitions(Framework PUBLIC FW_HAS_LUAU=1)
endif()
