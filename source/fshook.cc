#include <wctype.h>
#include <string>
#include <windows.h>
#include <detours.h>
#include "config.h"
#include "log.h"
#include "utils.h"

UINT (WINAPI *sys_GetDriveTypeA) (
    LPCSTR lpRootPathName
) = GetDriveTypeA;

UINT (WINAPI *sys_GetDriveTypeW) (
    LPCWSTR lpRootPathName
) = GetDriveTypeW;

BOOL (WINAPI *sys_GetVolumeInformationA) (
    LPCSTR  lpRootPathName,
    LPSTR   lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPSTR   lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize
) = GetVolumeInformationA;

BOOL (WINAPI *sys_GetVolumeInformationW) (
    LPCWSTR  lpRootPathName,
    LPWSTR   lpVolumeNameBuffer,
    DWORD    nVolumeNameSize,
    LPDWORD  lpVolumeSerialNumber,
    LPDWORD  lpMaximumComponentLength,
    LPDWORD  lpFileSystemFlags,
    LPWSTR   lpFileSystemNameBuffer,
    DWORD    nFileSystemNameSize
) = GetVolumeInformationW;

HANDLE (WINAPI *sys_CreateFileA) (
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
) = CreateFileA;

HANDLE (WINAPI *sys_CreateFileW) (
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
) = CreateFileW;

HANDLE (WINAPI *sys_FindFirstFileA) (
    LPCSTR             lpFileName,
    LPWIN32_FIND_DATAA lpFindFileData
) = FindFirstFileA;

HANDLE (WINAPI *sys_FindFirstFileW) (
    LPCWSTR             lpFileName,
    LPWIN32_FIND_DATAW  lpFindFileData
) = FindFirstFileW;

DWORD (WINAPI *sys_GetFileAttributesA) (
    LPCSTR lpFileName
) = GetFileAttributesA;

DWORD (WINAPI *sys_GetFileAttributesW) (
    LPCWSTR lpFileName
) = GetFileAttributesW;

bool path_on_drive(const std::string &path, char drive)
{
    return path.size() >= 3 &&
           tolower(path[0]) == tolower(drive) &&
           path[1] == ':' &&
           path[2] == '\\';
}

bool path_on_drive_w(const std::wstring &path, wchar_t drive)
{
    return path.size() >= 3 &&
           towlower(path[0]) == towlower(drive) &&
           path[1] == L':' &&
           path[2] == L'\\';
}

bool path_exists(const std::string &path)
{
    WIN32_FIND_DATA fdata;
    HANDLE handle = sys_FindFirstFileA(path.c_str(), &fdata);
    if (handle != INVALID_HANDLE_VALUE) {
        FindClose(handle);
        return true;
    }
    return false;
}

bool path_exists_w(const std::wstring &path)
{
    WIN32_FIND_DATAW fdata;
    HANDLE handle = sys_FindFirstFileW(path.c_str(), &fdata);
    if (handle != INVALID_HANDLE_VALUE) {
        FindClose(handle);
        return true;
    }
    return false;
}

bool fs_reloc(const std::string &origin, std::string &reloc)
{
    if (config.cddrive && !config.cdroot.empty() &&
        path_on_drive(origin, config.cddrive) && !path_exists(origin)) {
        reloc =
            config.basepath + '\\' + config.cdroot + '\\' + origin.substr(3);
        return true;
    }
    return false;
}

bool fs_reloc_w(const std::wstring &origin, std::wstring &reloc)
{
    if (config.cddrive && !config.cdroot.empty() &&
        path_on_drive_w(origin, c2w(config.cddrive)) &&
        !path_exists_w(origin)) {
        reloc = str_towide(config.basepath) + L'\\' +
                str_towide(config.cdroot) + L'\\' +
                origin.substr(3);
        return true;
    }
    return false;
}

UINT WINAPI hook_GetDriveTypeA(LPCSTR lpRootPathName)
{
    if (lpRootPathName &&
        tolower(lpRootPathName[0]) == tolower(config.cddrive)) {
        debug("GetDriveTypeA(\"%s\") -> DRIVE_CDROM", lpRootPathName);
        return DRIVE_CDROM;
    }
    return sys_GetDriveTypeA(lpRootPathName);
}

UINT WINAPI hook_GetDriveTypeW(LPCWSTR lpRootPathName)
{
    if (lpRootPathName &&
        towlower(lpRootPathName[0]) == towlower(c2w(config.cddrive))) {
        debug("GetDriveTypeW(\"%s\") -> DRIVE_CDROM", ansi(lpRootPathName));
        return DRIVE_CDROM;
    }
    return sys_GetDriveTypeW(lpRootPathName);
}

BOOL WINAPI hook_GetVolumeInformationA(
    LPCSTR  lpRootPathName,
    LPSTR   lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPSTR   lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize)
{
    BOOL result = sys_GetVolumeInformationA(lpRootPathName,
                                            lpVolumeNameBuffer,
                                            nVolumeNameSize,
                                            lpVolumeSerialNumber,
                                            lpMaximumComponentLength,
                                            lpFileSystemFlags,
                                            lpFileSystemNameBuffer,
                                            nFileSystemNameSize);
    if (lpRootPathName &&
        tolower(lpRootPathName[0]) == tolower(config.cddrive)) {
        debug("GetVolumeInformationA(\"%s\", ...) -> \"%s\"",
              lpRootPathName, config.cdlabel.c_str());
        strcpy(lpVolumeNameBuffer, config.cdlabel.c_str());
        return TRUE;
    }
    return result;
}

BOOL WINAPI hook_GetVolumeInformationW(
    LPCWSTR  lpRootPathName,
    LPWSTR   lpVolumeNameBuffer,
    DWORD    nVolumeNameSize,
    LPDWORD  lpVolumeSerialNumber,
    LPDWORD  lpMaximumComponentLength,
    LPDWORD  lpFileSystemFlags,
    LPWSTR   lpFileSystemNameBuffer,
    DWORD    nFileSystemNameSize)
{
    BOOL result = sys_GetVolumeInformationW(lpRootPathName,
                                            lpVolumeNameBuffer,
                                            nVolumeNameSize,
                                            lpVolumeSerialNumber,
                                            lpMaximumComponentLength,
                                            lpFileSystemFlags,
                                            lpFileSystemNameBuffer,
                                            nFileSystemNameSize);
    if (lpRootPathName &&
        towlower(lpRootPathName[0]) == towlower(c2w(config.cddrive))) {
        debug("GetVolumeInformationW(\"%s\", ...) -> \"%s\"",
              ansi(lpRootPathName), config.cdlabel.c_str());
        wcscpy(lpVolumeNameBuffer, wide(config.cdlabel));
        return TRUE;
    }
    return result;
}

HANDLE WINAPI hook_CreateFileA(
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile)
{
    std::string reloc;
    if (fs_reloc(std::string(lpFileName), reloc)) {
        debug("CreateFileA(\"%s\", ...)", lpFileName);
        debug("  RELOCATE : %s", reloc.c_str());
        lpFileName = reloc.c_str();
    }
    return sys_CreateFileA(lpFileName,
                           dwDesiredAccess,
                           dwShareMode,
                           lpSecurityAttributes,
                           dwCreationDisposition,
                           dwFlagsAndAttributes,
                           hTemplateFile);
}

HANDLE WINAPI hook_CreateFileW (
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile)
{
    std::wstring reloc;
    if (fs_reloc_w(std::wstring(lpFileName), reloc)) {
        debug("CreateFileW(\"%s\", ...)", ansi(lpFileName));
        debug("  RELOCATE : %s", ansi(reloc));
        lpFileName = reloc.c_str();
    }
    return sys_CreateFileW(lpFileName,
                           dwDesiredAccess,
                           dwShareMode,
                           lpSecurityAttributes,
                           dwCreationDisposition,
                           dwFlagsAndAttributes,
                           hTemplateFile);
}

HANDLE WINAPI hook_FindFirstFileA(LPCSTR lpFileName,
                                  LPWIN32_FIND_DATAA lpFindFileData)
{
    std::string reloc;
    if (fs_reloc(std::string(lpFileName), reloc)) {
        debug("FindFirstFileA(\"%s\", ...)", lpFileName);
        debug("  RELOCATE : %s", reloc.c_str());
        lpFileName = reloc.c_str();
    }
    return sys_FindFirstFileA(lpFileName, lpFindFileData);
}

HANDLE WINAPI hook_FindFirstFileW(LPCWSTR lpFileName,
                                  LPWIN32_FIND_DATAW lpFindFileData)
{
    std::wstring reloc;
    if (fs_reloc_w(std::wstring(lpFileName), reloc)) {
        debug("FindFirstFileW(\"%s\", ...)", ansi(lpFileName));
        debug("  RELOCATE : %s", ansi(reloc));
        lpFileName = reloc.c_str();
    }
    return sys_FindFirstFileW(lpFileName, lpFindFileData);
}

DWORD WINAPI hook_GetFileAttributesA(LPCSTR lpFileName)
{
    std::string reloc;
    if (fs_reloc(std::string(lpFileName), reloc)) {
        debug("GetFileAttributesA(\"%s\")", lpFileName);
        debug("  RELOCATE : %s", reloc.c_str());
        lpFileName = reloc.c_str();
    }
    return sys_GetFileAttributesA(lpFileName);
}

DWORD WINAPI hook_GetFileAttributesW(LPCWSTR lpFileName)
{
    std::wstring reloc;
    if (fs_reloc_w(std::wstring(lpFileName), reloc)) {
        debug("GetFileAttributesW(\"%s\")", ansi(lpFileName));
        debug("  RELOCATE : %s", ansi(reloc));
        lpFileName = reloc.c_str();
    }
    return sys_GetFileAttributesW(lpFileName);
}

const char *drvtypes[] = {
    "DRIVE_UNKNOWN",
    "DRIVE_NO_ROOT_DIR",
    "DRIVE_REMOVABLE",
    "DRIVE_FIXED",
    "DRIVE_REMOTE",
    "DRIVE_CDROM",
    "DRIVE_RAMDISK"
};

UINT WINAPI trace_GetDriveTypeA(LPCSTR lpRootPathName) {
    UINT result = sys_GetDriveTypeA(lpRootPathName);
    debug("TRACE GetDriveTypeA(\"%s\") : %s",
          lpRootPathName, drvtypes[result > DRIVE_RAMDISK ? 0 : result]);
    return result;
}

UINT WINAPI trace_GetDriveTypeW(LPCWSTR lpRootPathName) {
    UINT result = sys_GetDriveTypeW(lpRootPathName);
    debug("TRACE GetDriveTypeW(\"%s\") : %s",
          ansi(lpRootPathName), drvtypes[result > DRIVE_RAMDISK ? 0 : result]);
    return result;
}

BOOL WINAPI trace_GetVolumeInformationA(
    LPCSTR  lpRootPathName,
    LPSTR   lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPSTR   lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize) {
    BOOL result = sys_GetVolumeInformationA(lpRootPathName,
                                            lpVolumeNameBuffer,
                                            nVolumeNameSize,
                                            lpVolumeSerialNumber,
                                            lpMaximumComponentLength,
                                            lpFileSystemFlags,
                                            lpFileSystemNameBuffer,
                                            nFileSystemNameSize);
    debug("TRACE GetVolumeInformationA(\"%s\", ...) : \"%s\"",
          lpRootPathName, lpVolumeNameBuffer);
    return result;
}

BOOL WINAPI trace_GetVolumeInformationW(
    LPCWSTR  lpRootPathName,
    LPWSTR   lpVolumeNameBuffer,
    DWORD    nVolumeNameSize,
    LPDWORD  lpVolumeSerialNumber,
    LPDWORD  lpMaximumComponentLength,
    LPDWORD  lpFileSystemFlags,
    LPWSTR   lpFileSystemNameBuffer,
    DWORD    nFileSystemNameSize) {
    BOOL result = sys_GetVolumeInformationW(lpRootPathName,
                                            lpVolumeNameBuffer,
                                            nVolumeNameSize,
                                            lpVolumeSerialNumber,
                                            lpMaximumComponentLength,
                                            lpFileSystemFlags,
                                            lpFileSystemNameBuffer,
                                            nFileSystemNameSize);
    debug("TRACE GetVolumeInformationW(\"%s\", ...) : \"%s\"",
          ansi(lpRootPathName), ansi(lpVolumeNameBuffer));
    return result;
}

HANDLE WINAPI trace_CreateFileA(
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile)
{
    debug("TRACE CreateFileA(\"%s\", ...)", lpFileName);
    return sys_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                           lpSecurityAttributes, dwCreationDisposition,
                           dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI trace_CreateFileW(
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile)
{
    debug("TRACE CreateFileW(\"%s\", ...)", ansi(lpFileName));
    return sys_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                           lpSecurityAttributes, dwCreationDisposition,
                           dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI trace_FindFirstFileA(LPCSTR lpFileName,
                                   LPWIN32_FIND_DATAA lpFindFileData)
{
    debug("TRACE FindFirstFileA(\"%s\", ...)", lpFileName);
    return sys_FindFirstFileA(lpFileName, lpFindFileData);
}

HANDLE WINAPI trace_FindFirstFileW(LPCWSTR lpFileName,
                                   LPWIN32_FIND_DATAW lpFindFileData)
{
    debug("TRACE FindFirstFileW(\"%s\", ...)", ansi(lpFileName));
    return sys_FindFirstFileW(lpFileName, lpFindFileData);
}

DWORD WINAPI trace_GetFileAttributesA(LPCSTR lpFileName)
{
    debug("TRACE GetFileAttributesA(\"%s\")", lpFileName);
    return sys_GetFileAttributesA(lpFileName);
}

DWORD WINAPI trace_GetFileAttributesW(LPCWSTR lpFileName)
{
    debug("TRACE GetFileAttributesW(\"%s\")", ansi(lpFileName));
    return sys_GetFileAttributesW(lpFileName);
}

static void hook(HMODULE hDLL)
{
    if (!config.cddrive) {
        return;
    }

    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_GetDriveTypeA, hook_GetDriveTypeA);
    DetourAttach(&(PVOID&)sys_GetDriveTypeW, hook_GetDriveTypeW);
    if (!config.cdlabel.empty()) {
        DetourAttach(&(PVOID&)sys_GetVolumeInformationA,
                              hook_GetVolumeInformationA);
        DetourAttach(&(PVOID&)sys_GetVolumeInformationW,
                              hook_GetVolumeInformationW);
    }
    if (!config.cdroot.empty()) {
        DetourAttach(&(PVOID&)sys_CreateFileA, hook_CreateFileA);
        DetourAttach(&(PVOID&)sys_CreateFileW, hook_CreateFileW);
        DetourAttach(&(PVOID&)sys_FindFirstFileA, hook_FindFirstFileA);
        DetourAttach(&(PVOID&)sys_FindFirstFileW, hook_FindFirstFileW);
        DetourAttach(&(PVOID&)sys_GetFileAttributesA, hook_GetFileAttributesA);
        DetourAttach(&(PVOID&)sys_GetFileAttributesW, hook_GetFileAttributesW);
    }
    DetourTransactionCommit();
}

static void unhook()
{
    if (!config.cddrive) {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_GetDriveTypeA, hook_GetDriveTypeA);
    DetourDetach(&(PVOID&)sys_GetDriveTypeW, hook_GetDriveTypeW);
    if (!config.cdlabel.empty()) {
        DetourDetach(&(PVOID&)sys_GetVolumeInformationA,
                              hook_GetVolumeInformationA);
        DetourDetach(&(PVOID&)sys_GetVolumeInformationW,
                              hook_GetVolumeInformationW);
    }
    if (!config.cdroot.empty()) {
        DetourDetach(&(PVOID&)sys_CreateFileA, hook_CreateFileA);
        DetourDetach(&(PVOID&)sys_CreateFileW, hook_CreateFileW);
        DetourDetach(&(PVOID&)sys_FindFirstFileA, hook_FindFirstFileA);
        DetourDetach(&(PVOID&)sys_FindFirstFileW, hook_FindFirstFileW);
        DetourDetach(&(PVOID&)sys_GetFileAttributesA, hook_GetFileAttributesA);
        DetourDetach(&(PVOID&)sys_GetFileAttributesW, hook_GetFileAttributesW);
    }
    DetourTransactionCommit();
}

static void trace(HMODULE hDLL)
{
    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_GetDriveTypeA, trace_GetDriveTypeA);
    DetourAttach(&(PVOID&)sys_GetDriveTypeW, trace_GetDriveTypeW);
    DetourAttach(&(PVOID&)sys_GetVolumeInformationA,
                          trace_GetVolumeInformationA);
    DetourAttach(&(PVOID&)sys_GetVolumeInformationW,
                          trace_GetVolumeInformationW);
    DetourAttach(&(PVOID&)sys_CreateFileA, trace_CreateFileA);
    DetourAttach(&(PVOID&)sys_CreateFileW, trace_CreateFileW);
    DetourAttach(&(PVOID&)sys_FindFirstFileA, trace_FindFirstFileA);
    DetourAttach(&(PVOID&)sys_FindFirstFileW, trace_FindFirstFileW);
    DetourAttach(&(PVOID&)sys_GetFileAttributesA, trace_GetFileAttributesA);
    DetourAttach(&(PVOID&)sys_GetFileAttributesW, trace_GetFileAttributesW);
    DetourTransactionCommit();
}

static void untrace()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_GetDriveTypeA, trace_GetDriveTypeA);
    DetourDetach(&(PVOID&)sys_GetDriveTypeW, trace_GetDriveTypeW);
    DetourDetach(&(PVOID&)sys_GetVolumeInformationA,
                          trace_GetVolumeInformationA);
    DetourDetach(&(PVOID&)sys_GetVolumeInformationW,
                          trace_GetVolumeInformationW);
    DetourDetach(&(PVOID&)sys_CreateFileA, trace_CreateFileA);
    DetourDetach(&(PVOID&)sys_CreateFileW, trace_CreateFileW);
    DetourDetach(&(PVOID&)sys_FindFirstFileA, trace_FindFirstFileA);
    DetourDetach(&(PVOID&)sys_FindFirstFileW, trace_FindFirstFileW);
    DetourDetach(&(PVOID&)sys_GetFileAttributesA, trace_GetFileAttributesA);
    DetourDetach(&(PVOID&)sys_GetFileAttributesW, trace_GetFileAttributesW);
    DetourTransactionCommit();
}

void fs_hook(HMODULE hdll) {
    config.trace ? trace(hdll) : hook(hdll);
}

void fs_unhook() {
    config.trace ? untrace() : unhook();
}
