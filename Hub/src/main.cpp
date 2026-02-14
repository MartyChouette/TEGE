#include "HubApplication.h"
#include <iostream>

/**
 * @file main.cpp
 * @brief Entry point for the Enjin Hub application
 *
 * The Hub is a standalone launcher that manages Enjin projects and
 * engine versions. It uses GLFW + ImGui with an OpenGL backend
 * (no Vulkan dependency) for a lightweight UI.
 */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Enjin::Hub::HubApplication hub;

    if (!hub.Initialize()) {
        std::cerr << "Failed to initialize Enjin Hub" << std::endl;
        return 1;
    }

    int result = hub.Run();

    hub.Shutdown();

    return result;
}
