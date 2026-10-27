#include <windows.h>
#include <detours.h>
#include "config.h"
#include "utils.h"
#include "log.h"

LONG (WINAPI *sys_RegCreateKeyA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    PHKEY phkResult
) = RegCreateKeyA;

LONG (WINAPI *sys_RegCreateKeyExA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD Reserved,
    LPSTR lpClass,
    DWORD dwOptions,
    REGSAM samDesired,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    PHKEY phkResult,
    LPDWORD lpdwDisposition
) = RegCreateKeyExA;

LONG (WINAPI *sys_RegDeleteKeyA) (
    HKEY hKey,
    LPCSTR lpSubKey
) = RegDeleteKeyA;

LONG (WINAPI *sys_RegOpenKeyA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    PHKEY phkResult
) = RegOpenKeyA;

LONG (WINAPI *sys_RegOpenKeyExA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
) = RegOpenKeyExA;

LONG (WINAPI *sys_RegQueryValueA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    LPSTR lpValue,
    PLONG lpcbValue
) = RegQueryValueA;

LONG (WINAPI *sys_RegSetValueA) (
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD dwType,
    LPCSTR lpData,
    DWORD cbData
) = RegSetValueA;

LONG WINAPI hook_RegCreateKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegCreateKeyA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegCreateKeyA(hKey, lpSubKey, phkResult);
}

LONG WINAPI hook_RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved,
                                  LPSTR lpClass, DWORD dwOptions,
                                  REGSAM samDesired,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  PHKEY phkResult, LPDWORD lpdwDisposition)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegCreateKeyExA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegCreateKeyExA(hKey, lpSubKey, Reserved, lpClass, dwOptions,
                               samDesired, lpSecurityAttributes, phkResult,
                               lpdwDisposition);
}

LONG WINAPI hook_RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegDeleteKeyA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegDeleteKeyA(hKey, lpSubKey);
}

LONG WINAPI hook_RegOpenKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegOpenKeyA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegOpenKeyA(hKey, lpSubKey, phkResult);
}

LONG WINAPI hook_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions,
                               REGSAM samDesired, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegOpenKeyExA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LONG WINAPI hook_RegQueryValueA(HKEY hKey, LPCSTR lpSubKey,
                                LPSTR lpValue, PLONG lpcbValue)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegQueryValueA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegQueryValueA(hKey, lpSubKey, lpValue, lpcbValue);
}

LONG WINAPI hook_RegSetValueA(HKEY hKey, LPCSTR lpSubKey, DWORD dwType,
                              LPCSTR lpData, DWORD cbData)
{
    if (hKey == HKEY_LOCAL_MACHINE &&
        str_find(std::string(lpSubKey), config.regprefix) == 0) {
        debug("RegSetValueA : HKEY_LOCAL_MACHINE\\%s", lpSubKey);
        debug("  RELOCATE : HKEY_CURRENT_USER\\%s", lpSubKey);
        hKey = HKEY_CURRENT_USER;
    }

    return sys_RegSetValueA(hKey, lpSubKey, dwType, lpData, cbData);
}

LONG WINAPI trace_RegCreateKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegCreateKeyA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegCreateKeyA(hKey, lpSubKey, phkResult);
}

LONG WINAPI trace_RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved,
                                  LPSTR lpClass, DWORD dwOptions,
                                  REGSAM samDesired,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  PHKEY phkResult, LPDWORD lpdwDisposition)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegCreateKeyExA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegCreateKeyExA(hKey, lpSubKey, Reserved, lpClass, dwOptions,
                               samDesired, lpSecurityAttributes, phkResult,
                               lpdwDisposition);
}

LONG WINAPI trace_RegDeleteKeyA(HKEY hKey, LPCSTR lpSubKey)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegDeleteKeyA(HKEY_LOCAL_MACHINE, %s)", lpSubKey);
    }

    return sys_RegDeleteKeyA(hKey, lpSubKey);
}

LONG WINAPI trace_RegOpenKeyA(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegOpenKeyA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegOpenKeyA(hKey, lpSubKey, phkResult);
}

LONG WINAPI trace_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions,
                                REGSAM samDesired, PHKEY phkResult)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegOpenKeyExA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LONG WINAPI trace_RegQueryValueA(HKEY hKey, LPCSTR lpSubKey,
                                 LPSTR lpValue, PLONG lpcbValue)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegQueryValueA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegQueryValueA(hKey, lpSubKey, lpValue, lpcbValue);
}

LONG WINAPI trace_RegSetValueA(HKEY hKey, LPCSTR lpSubKey, DWORD dwType,
                               LPCSTR lpData, DWORD cbData)
{
    if (hKey == HKEY_LOCAL_MACHINE) {
        debug("TRACE RegSetValueA(HKEY_LOCAL_MACHINE, %s, ...)", lpSubKey);
    }

    return sys_RegSetValueA(hKey, lpSubKey, dwType, lpData, cbData);
}

static void hook(HMODULE hDLL)
{
    if (config.regprefix.empty()) {
        return;
    }

    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_RegCreateKeyA, hook_RegCreateKeyA);
    DetourAttach(&(PVOID&)sys_RegCreateKeyExA, hook_RegCreateKeyExA);
    DetourAttach(&(PVOID&)sys_RegDeleteKeyA, hook_RegDeleteKeyA);
    DetourAttach(&(PVOID&)sys_RegOpenKeyA, hook_RegOpenKeyA);
    DetourAttach(&(PVOID&)sys_RegOpenKeyExA, hook_RegOpenKeyExA);
    DetourAttach(&(PVOID&)sys_RegQueryValueA, hook_RegQueryValueA);
    DetourAttach(&(PVOID&)sys_RegSetValueA, hook_RegSetValueA);
    DetourTransactionCommit();
}

static void unhook()
{
    if (config.regprefix.empty()) {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_RegCreateKeyA, hook_RegCreateKeyA);
    DetourDetach(&(PVOID&)sys_RegCreateKeyExA, hook_RegCreateKeyExA);
    DetourDetach(&(PVOID&)sys_RegDeleteKeyA, hook_RegDeleteKeyA);
    DetourDetach(&(PVOID&)sys_RegOpenKeyA, hook_RegOpenKeyA);
    DetourDetach(&(PVOID&)sys_RegOpenKeyExA, hook_RegOpenKeyExA);
    DetourDetach(&(PVOID&)sys_RegQueryValueA, hook_RegQueryValueA);
    DetourDetach(&(PVOID&)sys_RegSetValueA, hook_RegSetValueA);
    DetourTransactionCommit();
}

static void trace(HMODULE hDLL)
{
    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_RegCreateKeyA, trace_RegCreateKeyA);
    DetourAttach(&(PVOID&)sys_RegCreateKeyExA, trace_RegCreateKeyExA);
    DetourAttach(&(PVOID&)sys_RegDeleteKeyA, trace_RegDeleteKeyA);
    DetourAttach(&(PVOID&)sys_RegOpenKeyA, trace_RegOpenKeyA);
    DetourAttach(&(PVOID&)sys_RegOpenKeyExA, trace_RegOpenKeyExA);
    DetourAttach(&(PVOID&)sys_RegQueryValueA, trace_RegQueryValueA);
    DetourAttach(&(PVOID&)sys_RegSetValueA, trace_RegSetValueA);
    DetourTransactionCommit();
}

static void untrace()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_RegCreateKeyA, trace_RegCreateKeyA);
    DetourDetach(&(PVOID&)sys_RegCreateKeyExA, trace_RegCreateKeyExA);
    DetourDetach(&(PVOID&)sys_RegDeleteKeyA, trace_RegDeleteKeyA);
    DetourDetach(&(PVOID&)sys_RegOpenKeyA, trace_RegOpenKeyA);
    DetourDetach(&(PVOID&)sys_RegOpenKeyExA, trace_RegOpenKeyExA);
    DetourDetach(&(PVOID&)sys_RegQueryValueA, trace_RegQueryValueA);
    DetourDetach(&(PVOID&)sys_RegSetValueA, trace_RegSetValueA);
    DetourTransactionCommit();
}

void reg_hook(HMODULE hdll) {
    config.trace ? trace(hdll) : hook(hdll);
}

void reg_unhook() {
    config.trace ? untrace() : unhook();
}
