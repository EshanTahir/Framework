#pragma once

#include "Core/application.h"
#include "Core/log.h"
#include "Core/window.h"

#include "Events/appEvent.h"
#include "Events/event.h"
#include "Events/inputEvent.h"

#include "Scene/components.h"
#include "Scene/entity.h"
#include "Scene/scene.h"
#include "Scene/sceneHandler.h"
#include "Scene/systems.h"

static AppEvent& app_event = AppEvent::GetInstance();
static InputEvent& input_event = InputEvent::GetInstance();

static SceneHandler& scene_handler = SceneHandler::GetInstance();
static Window& window = Window::GetInstance();
