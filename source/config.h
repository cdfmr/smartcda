#pragma once

#include <stdint.h>
#include <string>

typedef struct _config {
    bool        trace;     // trace only
    bool        debug;     // print debug messages
    std::string basepath;  // base path
    std::string audpath;   // path of audio tracks
    uint32_t    audfirst;  // first audio track
    uint32_t    audvol;    // volume, 0~100
    bool        audvhack;  // forced volume control
    char        cddrive;   // fake cdrom drive
    std::string cdlabel;   // fake cdrom label
    std::string cdroot;    // relocate files on cdrom here
    std::string regprefix; // relocate reg keys
} tconfig;

extern tconfig config;

void load_config(const std::string &filename);
