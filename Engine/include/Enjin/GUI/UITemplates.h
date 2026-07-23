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

    // Create a victory/defeat screen: dark overlay, colored message, optional
    // "Play Again" button that dispatches the "gameover_restart" UI event.
    // Spawned automatically by GameplayLoop when a GameOverComponent triggers ---
    // ONE game-over UI source rendered identically on desktop, editor play, and web.
    ENJIN_API UICanvasComponent CreateGameOverScreen(bool won, const std::string& message,
                                                     bool allowRestart = true);

} // namespace UITemplates

} // namespace Enjin::GUI
