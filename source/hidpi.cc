#include <windows.h>
#include <tchar.h>
#include <string>

typedef enum SMARTCDA_DPI_AWARENESS {
    SMARTCDA_DPI_AWARENESS_UNAWARE = 0,
    SMARTCDA_DPI_AWARENESS_SYSTEM_AWARE = 1,
    SMARTCDA_DPI_AWARENESS_PER_MONITOR_AWARE = 2
};

DECLARE_HANDLE(SMARTCDA_DPI_AWARENESS_CONTEXT);
#define SMARTCDA_DPI_AWARENESS_CONTEXT_UNAWARE              ((SMARTCDA_DPI_AWARENESS_CONTEXT)-1)
#define SMARTCDA_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE         ((SMARTCDA_DPI_AWARENESS_CONTEXT)-2)
#define SMARTCDA_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE    ((SMARTCDA_DPI_AWARENESS_CONTEXT)-3)
#define SMARTCDA_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((SMARTCDA_DPI_AWARENESS_CONTEXT)-4)
#define SMARTCDA_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED    ((SMARTCDA_DPI_AWARENESS_CONTEXT)-5)

typedef BOOL (WINAPI *APISetProcessDPIAware) ();
typedef HRESULT (WINAPI *APISetProcessDpiAwareness) (SMARTCDA_DPI_AWARENESS value);
typedef BOOL (WINAPI *APISetProcessDpiAwarenessContext) (SMARTCDA_DPI_AWARENESS_CONTEXT value);

APISetProcessDPIAware SmartCDA_SetProcessDPIAware = nullptr;
APISetProcessDpiAwareness SmartCDA_SetProcessDpiAwareness = nullptr;
APISetProcessDpiAwarenessContext SmartCDA_SetProcessDpiAwarenessContext = nullptr;

void load_hidpi_apis()
{
    static bool loaded = false;
    if (loaded) return;
    loaded = true;

    HINSTANCE user32 = LoadLibrary(_T("user32.dll"));
    HINSTANCE shcore = LoadLibrary(_T("shcore.dll"));
    SmartCDA_SetProcessDPIAware =
        (APISetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
    SmartCDA_SetProcessDpiAwareness =
        (APISetProcessDpiAwareness)GetProcAddress(shcore, "SetProcessDpiAwareness");
    SmartCDA_SetProcessDpiAwarenessContext =
        (APISetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
}

void set_hidpi_mode_system_enhanced()
{
    if (SmartCDA_SetProcessDpiAwarenessContext) {
        SmartCDA_SetProcessDpiAwarenessContext(SMARTCDA_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
    } else if (SmartCDA_SetProcessDpiAwareness) {
        SmartCDA_SetProcessDpiAwareness(SMARTCDA_DPI_AWARENESS_UNAWARE);
    }
}

void set_hidpi_mode_system()
{
    if (SmartCDA_SetProcessDpiAwarenessContext) {
        SmartCDA_SetProcessDpiAwarenessContext(SMARTCDA_DPI_AWARENESS_CONTEXT_UNAWARE);
    } else if (SmartCDA_SetProcessDpiAwareness) {
        SmartCDA_SetProcessDpiAwareness(SMARTCDA_DPI_AWARENESS_UNAWARE);
    }
}

void set_hidpi_mode_application()
{
    if (SmartCDA_SetProcessDpiAwarenessContext) {
        SmartCDA_SetProcessDpiAwarenessContext(SMARTCDA_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else if (SmartCDA_SetProcessDpiAwareness) {
        SmartCDA_SetProcessDpiAwareness(SMARTCDA_DPI_AWARENESS_PER_MONITOR_AWARE);
    } else if (SmartCDA_SetProcessDPIAware) {
        SmartCDA_SetProcessDPIAware();
    }
}

void set_hidpi_mode(const std::string &mode)
{
    load_hidpi_apis();

    if (mode == "system-enhanced") {
        set_hidpi_mode_system_enhanced();
    } else if (mode == "system") {
        set_hidpi_mode_system();
    } else if (mode == "application") {
        set_hidpi_mode_application();
    }
}
