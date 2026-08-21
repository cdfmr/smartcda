#include <ctype.h>
#include <string>
#include <vector>
#include <algorithm>
#include "utils.h"
#include "log.h"

void mem_patch(const std::string &conf)
{
    auto patches = ini_read_sec(conf, "memory");
    for (auto it = patches.begin(); it != patches.end(); it++) {
        std::string sval(it->second);
        sval.erase(std::remove_if(sval.begin(), sval.end(), [](char c) {
                       return isspace(c);
                   }), sval.end());
        std::vector<uint8_t> data;
        for (size_t i = 0; i + 1 < sval.size(); i += 2) {
            std::string bstr(sval, i, 2);
            data.push_back((uint8_t)strtol(bstr.c_str(), nullptr, 16));
        }

        if (!data.empty()) {
            uint32_t addr = strtol(it->first.c_str(), nullptr, 16);
            debug("MEMORY PATCH %08x -> %s", addr, it->second.c_str());
            WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr,
                               (LPCVOID)data.data(), data.size(), nullptr);
        }
    }
}
