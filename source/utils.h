#pragma once

#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <windows.h>

bool is_run_as_admin();
std::string get_module_file_name(HMODULE handle);
std::string get_env(const std::string &name);

uint32_t ini_read_int(const std::string &file,
                      const std::string &section, const std::string &key,
                      uint32_t def = 0);
bool ini_read_bool(const std::string &file,
                   const std::string &section, const std::string &key,
                   bool def = false);
std::string ini_read_str(const std::string &file,
                         const std::string &section, const std::string &key,
                         const std::string &def = std::string());
std::map<std::string, std::string> ini_read_sec(const std::string &file,
                                                const std::string &section);

size_t str_find(const std::string &str, const std::string &sub,
                bool case_sensitive = false);
std::string str_ltrim(const std::string &str);
std::string str_rtrim(const std::string &str);
std::string str_trim(const std::string &str);
std::string str_lower(const std::string &str);
std::string str_upper(const std::string &str);
std::vector<std::string> str_split(const std::string &str);
std::wstring str_towide(const std::string &mbs);
std::string str_toansi(const std::wstring &wcs);

wchar_t c2w(char c);
char w2c(wchar_t wc);
#define wide(mbs) (str_towide(std::string(mbs)).c_str())
#define ansi(wcs) (str_toansi(std::wstring(wcs)).c_str())
