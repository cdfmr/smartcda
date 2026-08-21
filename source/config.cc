#include <windows.h>
#include "utils.h"
#include "config.h"

tconfig config;

void load_config(const std::string &filename)
{
    config.basepath = filename.substr(0, filename.rfind('\\'));
    config.trace = ini_read_bool(filename, "smartcda", "trace");
    config.debug = ini_read_bool(filename, "smartcda", "debug");
    config.audpath = ini_read_str(filename, "audio", "path");
    config.audfirst = max(ini_read_int(filename, "audio", "first", 2), 1);
    config.audvol = min(ini_read_int(filename, "audio", "volume", 100), 100);
    config.audvhack = ini_read_bool(filename, "audio", "volhack");
    std::string drive = ini_read_str(filename, "data", "drive");
    config.cddrive = drive.empty() ? 0 : drive[0];
    config.cdlabel = ini_read_str(filename, "data", "label");
    config.cdroot = ini_read_str(filename, "data", "root");

    if (config.trace) {
        config.debug = true;
    }
}
