#include <windows.h>
#include <stdio.h>
#include <string>
#include "detours.h"

using namespace std;

#pragma comment(lib, "detours.lib")

bool is_runas_admin()
{
    SID_IDENTIFIER_AUTHORITY ntauth = SECURITY_NT_AUTHORITY;
    PSID admingroup = NULL;
    BOOL isadmin = FALSE;
    if (AllocateAndInitializeSid(&ntauth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admingroup)) {
        CheckTokenMembership(NULL, admingroup, &isadmin);
    }
    if (admingroup) FreeSid(admingroup);

    return isadmin;
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    string loader;
    string basepath;
    string config;
    string gamebin;
    string cdahook;
    string binpath;
    char   buffer[MAX_PATH];

    GetModuleFileName(hInstance, buffer, MAX_PATH);
    loader = buffer;
    basepath = loader;
    basepath.erase(basepath.rfind('\\'));
    config = basepath + "\\smartcda.ini";

    bool runasadmin = GetPrivateProfileInt("smartcda", "runasadmin", 0, config.c_str()) == 1;
    if (runasadmin && !is_runas_admin()) {
        SHELLEXECUTEINFO sei;
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = "runas";
        sei.lpFile = loader.c_str();
        sei.nShow = SW_NORMAL;
        ShellExecuteEx(&sei);
        return 0;
    }

    GetPrivateProfileString("smartcda", "gamebin", "game.exe", buffer, MAX_PATH, config.c_str());
    gamebin = basepath + '\\' + buffer;
    binpath = gamebin;
    binpath.erase(binpath.rfind('\\'));

    GetPrivateProfileString("smartcda", "cdahook", "smartcda.dll", buffer, MAX_PATH, config.c_str());
    cdahook = basepath + '\\' + buffer;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    si.cb = sizeof(STARTUPINFO);
    DetourCreateProcessWithDll(gamebin.c_str(), NULL, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE,
                               NULL, binpath.c_str(), &si, &pi, cdahook.c_str(), NULL);

    return 0;
}
