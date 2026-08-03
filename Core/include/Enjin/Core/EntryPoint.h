#pragma once

#include "Enjin/Core/Application.h"

// Implemented by the application: return your Enjin::Application subclass.
Enjin::Application* CreateApplication();

// Platform-specific entry point
#ifdef ENJIN_PLATFORM_WINDOWS
    #include <Windows.h>
    // Windows.h defines CreateWindow as a macro, which mangles any later
    // declaration of Enjin::CreateWindow (Platform/Window.h)
    #ifdef CreateWindow
        #undef CreateWindow
    #endif

    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
        (void)hInstance;
        (void)hPrevInstance;
        (void)lpCmdLine;
        (void)nCmdShow;

        Enjin::Application* app = CreateApplication();
        int result = app->Run();
        delete app;
        return result;
    }
#else
    int main(int argc, char* argv[]) {
        (void)argc;
        (void)argv;

        Enjin::Application* app = CreateApplication();
        int result = app->Run();
        delete app;
        return result;
    }
#endif
