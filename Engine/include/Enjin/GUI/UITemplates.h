#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/GUI/UICanvas.h"

#include <string>

namespace Enjin::GUI {

namespace UITemplates {

    // Create a main menu canvas with title, and New Game / Continue / Options / Quit buttons
    ENJIN_API UICanvasComponent CreateMainMenu(const std::string& gameTitle = "My Game");

    // Create a pause menu overlay with Resume / Options / Quit buttons
    ENJIN_API UICanvasComponent CreatePauseMenu();

    // Create an options menu with volume sliders, graphics checkboxes, and back button
    ENJIN_API UICanvasComponent CreateOptionsMenu();

} // namespace UITemplates

} // namespace Enjin::GUI
