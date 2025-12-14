#include "Core/Application.h"
#include <windows.h>

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd
)
{
    // Unused parameters
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    // Create and initialize application
    Application app;

    if (!app.Initialize())
    {
        MessageBoxW(nullptr, L"Failed to initialize application!", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Run main loop
    app.Run();

    // Cleanup
    app.Shutdown();

    return 0;
}