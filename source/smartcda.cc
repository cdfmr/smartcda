#include <string>
#include <windows.h>
#include "log.h"
#include "config.h"
#include "utils.h"
#include "mcihook.h"
#include "auxhook.h"
#include "mixhook.h"
#include "fshook.h"
#include "reghook.h"
#include "hidpi.h"
#include "mempatch.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "audiere.lib")
#pragma comment(lib, "detours.lib")

extern "C" __declspec(dllexport) void _stdcall dummy() {}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH: {
            std::string smartcda = get_module_file_name(hinstDLL);
            std::string cfgfile = smartcda.substr(0, smartcda.rfind('\\')) +
                                  "\\smartcda.ini";
            load_config(cfgfile.c_str());
            if (config.debug) {
                open_log(config.basepath + "\\smartcda.log");
                debug("smartcda launched%s", config.trace ? " (trace)" : "");
            }

            mci_hook(hinstDLL);
            aux_hook(hinstDLL);
            mix_hook(hinstDLL);
            fs_hook(hinstDLL);
            reg_hook(hinstDLL);
            mem_patch(cfgfile);
            set_hidpi_mode(str_lower(config.hidpi));
        }
        break;

        case DLL_PROCESS_DETACH:
            mci_unhook();
            aux_unhook();
            mix_unhook();
            fs_unhook();
            reg_unhook();
            if (config.debug) {
                debug("smartcda quit");
                close_log();
            }
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}
