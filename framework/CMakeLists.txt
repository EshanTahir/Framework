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
    Audio/audio.cpp
    Audio/audioDebug.cpp
    Audio/musicPlayer.cpp
    Render/gl.cpp
    Render/renderer2d.cpp
    Render/renderTarget.cpp
    Render/presentPipeline.cpp
    Render/worldAtlas.cpp
    Scene/entity.cpp
    Scene/scene.cpp
    Scene/sceneHandler.cpp
    Scene/scripts.cpp
    Scene/physics.cpp
    Scene/renderer.cpp
    Scene/sound.cpp
    Scene/sfxPool.cpp
    Scene/ui.cpp
    Scene/particles.cpp
    Utils/rect.cpp
    Debug/debugHud.cpp
    Events/inputEvent.cpp
    Events/event.cpp
    Events/appEvent.cpp
    Events/keys.cpp
    Script/scriptEngine.cpp
    Script/scriptInternal.cpp
    Script/scriptBindsInput.cpp
    Script/scriptBindsPhysics.cpp
    Script/scriptBindsRender.cpp
    Script/scriptBindsAudio.cpp
    Script/scriptBindsUi.cpp
    Match/matchConfig.cpp
    Match/simClock.cpp
    Match/matchChannels.cpp
    Match/matchRand.cpp
    Match/contentHash.cpp
    Match/inputSampler.cpp
    Match/inputRecorder.cpp
    Match/matchSession.cpp
    Match/matchNetSync.cpp
    Match/matchEvents.cpp
    Match/inputHistory.cpp
    Net/netSockets.cpp
    Net/netSession.cpp
    Net/netAsync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/monocypher/monocypher.c
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
        ${FW_ENGINE_ROOT}/vendor/box2D/include/box2d
        ${FW_ENGINE_ROOT}/vendor/ENTT
        ${FW_ENGINE_ROOT}/vendor/FMOD/api/core/inc
        ${FW_ENGINE_ROOT}/vendor/json
        ${FW_ENGINE_ROOT}/vendor/monocypher
)

target_compile_definitions(Framework PUBLIC FW_USE_OPENGL=1 SDL_MAIN_HANDLED)

target_link_directories(Framework PUBLIC
    ${FW_ENGINE_ROOT}/vendor/SDL2/lib
    ${FW_ENGINE_ROOT}/vendor/box2D/lib
    ${FW_ENGINE_ROOT}/vendor/FMOD/api/core/lib/x64
)

target_link_libraries(Framework PUBLIC
    SDL2
    SDL2_image
    SDL2_ttf
    box2d
    fmod_vc
    opengl32
)
if (WIN32)
    target_link_libraries(Framework PUBLIC ws2_32)
endif()

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
