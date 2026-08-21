#include <string>
#include <windows.h>
#include <detours.h>
#include "utils.h"

#pragma comment(lib, "detours.lib")

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
    const char *smartcda = "smartcda";

    std::string loader = get_module_file_name(0);
    if (loader.empty()) {
        MessageBox(0, "can not detect program path.", smartcda, 0);
        return -1;
    }
    std::string basepath(loader, 0, loader.rfind(L'\\'));
    std::string config = basepath + '\\' + smartcda + ".ini";
    std::string cdahook = basepath + '\\' + smartcda + ".dll";

    bool run_as_admin = ini_read_bool(config, smartcda, "admin", false);
    if (run_as_admin && !is_run_as_admin()) {
        SHELLEXECUTEINFO sei;
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = "runas";
        sei.lpFile = loader.c_str();
        sei.nShow = SW_NORMAL;
        ShellExecuteEx(&sei);
        return 0;
    }

    std::string game = ini_read_str(config, smartcda, "game");
    if (game.empty()) {
        MessageBox(0, "invalid game settings.", smartcda, 0);
    }
    game = basepath + '\\' + game;
    std::string gamepath(game, 0, game.rfind('\\'));

    std::string path = get_env("PATH");
    path = basepath + ';' + path;
    SetEnvironmentVariable("PATH", path.c_str());

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    return DetourCreateProcessWithDll(
               game.c_str(), nullptr, nullptr, nullptr, FALSE,
               CREATE_DEFAULT_ERROR_MODE, nullptr,
               gamepath.c_str(), &si, &pi, cdahook.c_str(), nullptr) ?
           0 : -1;
}
