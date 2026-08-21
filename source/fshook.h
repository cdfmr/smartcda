#pragma once

#include <string>
#include <windows.h>

void fs_hook(HMODULE hdll);
void fs_unhook();
bool fs_reloc(const std::string &origin, std::string &reloc);
bool fs_reloc_w(const std::wstring &origin, std::wstring &reloc);
