#include <windows.h>
#include <detours.h>
#include "config.h"
#include "log.h"
#include "mcihook.h"

UINT (WINAPI *sys_auxGetNumDevs) (
) = auxGetNumDevs;

MMRESULT (WINAPI *sys_auxGetDevCapsA) (
    UINT       uDeviceID,
    LPAUXCAPSA lpCaps,
    UINT       cbCaps
) = auxGetDevCapsA;

MMRESULT (WINAPI *sys_auxGetVolume) (
    UINT    uDeviceID,
    LPDWORD lpdwVolume
) = auxGetVolume;

MMRESULT (WINAPI *sys_auxSetVolume) (
    UINT  uDeviceID,
    DWORD dwVolume
) = auxSetVolume;

static UINT auxid = (UINT)-1;

UINT WINAPI hook_auxGetNumDevs()
{
    debug("auxGetNumDevs");

    UINT result = sys_auxGetNumDevs();
    bool hascd = false;
    for (size_t i = 0; i < result; i++) {
        AUXCAPSA caps;
        memset(&caps, 0, sizeof(caps));
        sys_auxGetDevCapsA(i, &caps, sizeof(caps));
        if (caps.wTechnology == AUXCAPS_CDAUDIO) {
            hascd = true;
            break;
        }
    }

    if (!hascd) {
        auxid = result++;
        debug("  AUX : append fake cd audio device as #%d", auxid);
    }

    return result;
}

MMRESULT WINAPI hook_auxGetDevCapsA(UINT uDeviceID,
                                    LPAUXCAPSA lpCaps, UINT cbCaps)
{
    debug("auxGetDevCapsA : device(%d)", uDeviceID);

    if (auxid == uDeviceID && auxid != (UINT)-1) {
        debug("  AUX : return fake cd audio device");
        lpCaps->wMid = 0xFE;
        lpCaps->wPid = 0xE1;
        lpCaps->vDriverVersion = MAKEWORD(0, 1);
        strcpy(lpCaps->szPname, "SmartCDA");
        lpCaps->wTechnology = AUXCAPS_CDAUDIO;
        lpCaps->wReserved1 = 0;
        lpCaps->dwSupport = AUXCAPS_VOLUME | AUXCAPS_LRVOLUME;
        return MMSYSERR_NOERROR;
    }

    return sys_auxGetDevCapsA(uDeviceID, lpCaps, cbCaps);
}

MMRESULT WINAPI hook_auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
    debug("auxGetVolume : device(%d)", uDeviceID);

    if (auxid == uDeviceID && auxid != (UINT)-1) {
        float lvol, rvol;
        mci_getvol(lvol, rvol);
        *lpdwVolume = MAKELONG(lvol * 65535, rvol * 65535);
        debug("  AUX : 0x%08x", *lpdwVolume);
        return MMSYSERR_NOERROR;
    }

    return sys_auxGetVolume(uDeviceID, lpdwVolume);
}

MMRESULT WINAPI hook_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    debug("auxSetVolume : device(%d) volume(0x%08x)", uDeviceID, dwVolume);

    if (config.audvhack || // homm2 chinese
        (auxid == uDeviceID && auxid != (UINT)-1)) {
        mci_setvol(LOWORD(dwVolume) / 65535.0f, HIWORD(dwVolume) / 65535.0f);
        return MMSYSERR_NOERROR;
    }

    AUXCAPSA caps;
    memset(&caps, 0, sizeof(caps));
    sys_auxGetDevCapsA(uDeviceID, &caps, sizeof(caps));
    if (caps.wTechnology == AUXCAPS_CDAUDIO) {
        mci_setvol(LOWORD(dwVolume) / 65535.0f, HIWORD(dwVolume) / 65535.0f);
    }
    return sys_auxSetVolume(uDeviceID, dwVolume);
}

UINT WINAPI trace_auxGetNumDevs()
{
    UINT result = sys_auxGetNumDevs();
    debug("TRACE auxGetNumDevs() : %u", result);
    return result;
}

MMRESULT WINAPI trace_auxGetDevCapsA(UINT uDeviceID,
                                     LPAUXCAPSA lpCaps, UINT cbCaps)
{
    MMRESULT result = sys_auxGetDevCapsA(uDeviceID, lpCaps, cbCaps);
    debug("TRACE auxGetDevCapsA(%d, ...) : %u", uDeviceID, result);
    debug("  Technology : %s",
          lpCaps->wTechnology == AUXCAPS_CDAUDIO ? "CD Audio" :
          lpCaps->wTechnology == AUXCAPS_AUXIN ? "Aux In" : "Unknown");
    return result;
}

MMRESULT WINAPI trace_auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
    MMRESULT result = sys_auxGetVolume(uDeviceID, lpdwVolume);
    debug("TRACE auxGetVolume(%d, 0x%08x) : %u",
          uDeviceID, *lpdwVolume, result);
    return result;
}

MMRESULT WINAPI trace_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    MMRESULT result = sys_auxSetVolume(uDeviceID, dwVolume);
    debug("TRACE auxSetVolume(%d, 0x%08x) : %u",
          uDeviceID, dwVolume, result);
    return result;
}

static void hook(HMODULE hDLL)
{
    if (config.audpath.empty()) {
        return;
    }

    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_auxGetNumDevs, hook_auxGetNumDevs);
    DetourAttach(&(PVOID&)sys_auxGetDevCapsA, hook_auxGetDevCapsA);
    DetourAttach(&(PVOID&)sys_auxGetVolume, hook_auxGetVolume);
    DetourAttach(&(PVOID&)sys_auxSetVolume, hook_auxSetVolume);
    DetourTransactionCommit();
}

static void unhook()
{
    if (config.audpath.empty()) {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_auxGetNumDevs, hook_auxGetNumDevs);
    DetourDetach(&(PVOID&)sys_auxGetDevCapsA, hook_auxGetDevCapsA);
    DetourDetach(&(PVOID&)sys_auxGetVolume, hook_auxGetVolume);
    DetourDetach(&(PVOID&)sys_auxSetVolume, hook_auxSetVolume);
    DetourTransactionCommit();
}

static void trace(HMODULE hDLL)
{
    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_auxGetNumDevs, trace_auxGetNumDevs);
    DetourAttach(&(PVOID&)sys_auxGetDevCapsA, trace_auxGetDevCapsA);
    DetourAttach(&(PVOID&)sys_auxGetVolume, trace_auxGetVolume);
    DetourAttach(&(PVOID&)sys_auxSetVolume, trace_auxSetVolume);
    DetourTransactionCommit();
}

static void untrace()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_auxGetNumDevs, trace_auxGetNumDevs);
    DetourDetach(&(PVOID&)sys_auxGetDevCapsA, trace_auxGetDevCapsA);
    DetourDetach(&(PVOID&)sys_auxGetVolume, trace_auxGetVolume);
    DetourDetach(&(PVOID&)sys_auxSetVolume, trace_auxSetVolume);
    DetourTransactionCommit();
}

void aux_hook(HMODULE hdll) {
    config.trace ? trace(hdll) : hook(hdll);
}

void aux_unhook() {
    config.trace ? untrace() : unhook();
}
