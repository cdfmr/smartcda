#include <stdint.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <windows.h>

bool is_run_as_admin()
{
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    PSID admins = NULL;
    BOOL is_admin = FALSE;
    if (AllocateAndInitializeSid(&auth, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &admins)) {
        CheckTokenMembership(NULL, admins, &is_admin);
    }
    if (admins) FreeSid(admins);

    return (is_admin == TRUE);
}

std::string get_module_file_name(HMODULE handle)
{
    size_t cap = 1024;
    while (true) {
        std::vector<char> buf(cap);
        DWORD size = GetModuleFileName(handle, buf.data(), buf.size());
        if (size < buf.size() - 1) {
            return std::string(buf.data(), size);
        }
        cap *= 2;
    }
}

std::string get_env(const std::string &name)
{
    size_t cap = 1024;
    std::vector<char> buf(cap);
    DWORD size = GetEnvironmentVariable(name.c_str(), buf.data(), buf.size());
    if (size > buf.size()) {
        buf.resize(size);
        size = GetEnvironmentVariable(name.c_str(), buf.data(), buf.size());
    }
    return std::string(buf.data(), size);
}

uint32_t ini_read_int(const std::string &file,
                      const std::string &section, const std::string &key,
                      uint32_t def)
{
    return GetPrivateProfileInt(section.c_str(), key.c_str(), def,
                                file.c_str());
}

bool ini_read_bool(const std::string &file,
                   const std::string &section, const std::string &key,
                   bool def)
{
    uint32_t value = GetPrivateProfileInt(section.c_str(), key.c_str(),
                                          (int)def, file.c_str());
    return (value > 0);
}

std::string ini_read_str(const std::string &file,
                         const std::string &section, const std::string &key,
                         const std::string &def)
{
    size_t cap = 1024;
    while (true) {
        std::vector<char> buf(cap);
        DWORD size = GetPrivateProfileString(
                         section.c_str(), key.c_str(), def.c_str(),
                         buf.data(), buf.size(), file.c_str());
        if (size < buf.size() - 1) {
            return std::string(buf.data(), size);
        }
        cap *= 2;
    }
}

std::map<std::string, std::string> ini_read_sec(const std::string &file,
                                                const std::string &section)
{
    size_t cap = 1024;
    std::vector<char> buf(cap);
    while (true) {
        DWORD size = GetPrivateProfileSection(
                         section.c_str(), buf.data(), buf.size(), file.c_str());
        if (size < buf.size() - 2) { // double null terminator
            buf.resize(size + 2);
            break;
        }
        buf.resize(buf.size() * 2);
    }

    size_t p = 0;
    std::map<std::string, std::string> values;
    for (size_t i = 0; i < buf.size(); i++) {
        if (buf[i] == 0) {
            std::string kv = std::string(buf.data() + p, i - p);
            size_t equal = kv.find('=');
            if (equal != std::string::npos) {
                std::string key = kv.substr(0, equal);
                std::string value = kv.substr(equal + 1);
                if (!key.empty()) {
                    values[key] = value;
                }
            }
            p = i + 1;
            if (buf[p] == 0) { // double null terminator
                break;
            }
        }
    }
    return values;
}

size_t str_find(const std::string &str, const std::string &sub,
                bool case_sensitive)
{
    if (case_sensitive) {
        return str.find(sub);
    }

    auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
                          [](char a, char b) {
                              return tolower(a) == tolower(b);
                          });
    return (it != str.end()) ? (it - str.begin()) : std::string::npos;
}

std::string str_ltrim(const std::string &str)
{
    auto start = std::find_if(str.begin(), str.end(), [](char c) {
                                  return !isspace(c);
                              });
    if (start != str.end()) {
        return str.substr(start - str.begin());
    }

    return std::string();
}

std::string str_rtrim(const std::string &str)
{
    auto end = std::find_if(str.rbegin(), str.rend(), [](char c) {
                                return !isspace(c);
                            });
    if (end != str.rend()) {
        return str.substr(0, end.base() - str.begin());
    }

    return std::string();
}

std::string str_trim(const std::string &str)
{
    return str_ltrim(str_rtrim(str));
}

std::string str_lower(const std::string &str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string str_upper(const std::string &str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::vector<std::string> str_split(const std::string &str)
{
    std::string remain = str_trim(str);
    std::vector<std::string> result;
    while (!remain.empty()) {
        auto it = std::find_if(remain.begin(), remain.end(), isspace);
        if (it == remain.end()) {
            result.push_back(remain);
            remain.clear();
        } else {
            result.push_back(std::string(remain.begin(), it));
            remain = str_ltrim(std::string(it, remain.end()));
        }
    }

    return result;
}

std::wstring str_towide(const std::string &mbs)
{
    if (mbs.empty()) {
        return std::wstring();
    }

    int len = MultiByteToWideChar(CP_ACP, 0, mbs.c_str(), mbs.size() + 1,
                                  nullptr, 0);
    if (len <= 0) {
        return std::wstring();
    }

    wchar_t *wcs = new wchar_t[len];
    memset(wcs, 0, sizeof(wchar_t) * len);
    len = MultiByteToWideChar(CP_ACP, 0, mbs.c_str(), mbs.size() + 1, wcs, len);
    return (len > 0) ? std::wstring(wcs) : std::wstring();
}

std::string str_toansi(const std::wstring &wcs)
{
    if (wcs.empty()) {
        return std::string();
    }

    int len = WideCharToMultiByte(CP_ACP, 0, wcs.c_str(), wcs.size() + 1,
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return std::string();
    }

    char *mbs = new char[len];
    memset(mbs, 0, len);
    len = WideCharToMultiByte(CP_ACP, 0, wcs.c_str(), wcs.size() + 1,
                              mbs, len, nullptr, nullptr);
    return (len > 0) ? std::string(mbs) : std::string();
}

wchar_t c2w(char c)
{
    std::wstring wcs = str_towide(std::string(1, c));
    return wcs.empty() ? L'\0' : wcs[0];
}

char w2c(wchar_t wc)
{
    std::string mbs = str_toansi(std::wstring(1, wc));
    return mbs.empty() ? '\0' : mbs[0];
}
