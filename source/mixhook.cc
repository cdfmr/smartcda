#include <stdint.h>
#include <set>
#include <utility>
#include <windows.h>
#include <detours.h>
#include "config.h"
#include "log.h"
#include "mcihook.h"

MMRESULT (WINAPI *sys_mixerGetLineInfoA) (
    HMIXEROBJ    hmxobj,
    LPMIXERLINEA pmxl,
    DWORD        fdwInfo
) = mixerGetLineInfoA;

MMRESULT (WINAPI *sys_mixerGetLineControlsA) (
    HMIXEROBJ            hmxobj,
    LPMIXERLINECONTROLSA pmxlc,
    DWORD                fdwControls
) = mixerGetLineControlsA;

MMRESULT (WINAPI *sys_mixerGetControlDetailsA) (
    HMIXEROBJ             hmxobj,
    LPMIXERCONTROLDETAILS pmxcd,
    DWORD                 fdwDetails
) = mixerGetControlDetailsA;

MMRESULT (WINAPI *sys_mixerSetControlDetails) (
    HMIXEROBJ             hmxobj,
    LPMIXERCONTROLDETAILS pmxcd,
    DWORD                 fdwDetails
) = mixerSetControlDetails;

static std::set<std::pair<HMIXEROBJ, DWORD>> cdlines;
static std::set<std::pair<HMIXEROBJ, DWORD>> cdvctrls;
static std::set<std::pair<HMIXEROBJ, DWORD>> cdmctrls;

MMRESULT WINAPI hook_mixerGetLineInfoA(HMIXEROBJ    hmxobj,
                                       LPMIXERLINEA pmxl,
                                       DWORD        fdwInfo)
{
    debug("mixerGetLineInfoA()");
    MMRESULT result = sys_mixerGetLineInfoA(hmxobj, pmxl, fdwInfo);

    if (result == MMSYSERR_NOERROR &&
        pmxl->dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC) {
        cdlines.insert(std::make_pair(hmxobj, pmxl->dwLineID));
        debug("  AUDIO CD : mixer(0x%08x) line(%u)", hmxobj, pmxl->dwLineID);
    }

    return result;
}

MMRESULT WINAPI hook_mixerGetLineControlsA(HMIXEROBJ            hmxobj,
                                           LPMIXERLINECONTROLSA pmxlc,
                                           DWORD                fdwControls)
{
    debug("mixerGetLineControlsA()");
    MMRESULT result = sys_mixerGetLineControlsA(hmxobj, pmxlc, fdwControls);

    if (result == MMSYSERR_NOERROR &&
        cdlines.find(std::make_pair(hmxobj, pmxlc->dwLineID)) !=
        cdlines.end()) {
        for (size_t i = 0; i < pmxlc->cControls; i++) {
            LPMIXERCONTROL pmxctrl = (LPMIXERCONTROL)
                ((DWORD)pmxlc->pamxctrl + pmxlc->cbmxctrl * i);
            switch (pmxctrl->dwControlType) {
                case MIXERCONTROL_CONTROLTYPE_VOLUME:
                    cdvctrls.insert(std::make_pair(hmxobj,
                                                   pmxctrl->dwControlID));
                    debug("  AUDIO CD : mixer(0x%08x) control(%u) type(volume)",
                             hmxobj, pmxctrl->dwControlID);
                    break;
                case MIXERCONTROL_CONTROLTYPE_MUTE:
                    cdmctrls.insert(std::make_pair(hmxobj,
                                                   pmxctrl->dwControlID));
                    debug("  AUDIO CD : mixer(0x%08x) control(%u) type(mute)",
                             hmxobj, pmxctrl->dwControlID);
                    break;
            }
        }
    }

    return result;
}

MMRESULT WINAPI hook_mixerGetControlDetailsA(HMIXEROBJ             hmxobj,
                                             LPMIXERCONTROLDETAILS pmxcd,
                                             DWORD                 fdwDetails)
{
    debug("mixerGetControlDetailsA : mixer(0x%08x) control(%u)",
          hmxobj, pmxcd->dwControlID);
    MMRESULT result = sys_mixerGetControlDetailsA(hmxobj, pmxcd, fdwDetails);

    if ((fdwDetails & MIXER_GETCONTROLDETAILSF_LISTTEXT) ||
        (fdwDetails & MIXER_GETCONTROLDETAILSF_QUERYMASK)) {
        return result;
    }

    if (cdvctrls.find(std::make_pair(hmxobj, pmxcd->dwControlID)) !=
        cdvctrls.end()) {
        float lv, rv;
        mci_getvol(lv, rv);
        DWORD lvol = (DWORD)(lv * 65535);
        DWORD rvol = (DWORD)(rv * 65535);
        if (pmxcd->cChannels > 1) {
            *((DWORD *)pmxcd->paDetails) = lvol;
            *((DWORD *)((DWORD)pmxcd->paDetails + pmxcd->cbDetails)) = rvol;
        } else {
            *((DWORD *)pmxcd->paDetails) = max(lvol, rvol);
        }
        debug("  AUDIO CD : mixer(0x%08x) control(%u) volume(0x%08x)",
                 hmxobj, pmxcd->dwControlID, MAKELONG(lvol, rvol));
        return result;
    }

    if (cdmctrls.find(std::make_pair(hmxobj, pmxcd->dwControlID)) !=
        cdmctrls.end()) {
        bool mute = mci_ismute();
        *((BOOL *)pmxcd->paDetails) = (BOOL)mute;
        debug("  AUDIO CD : mixer(0x%08x) control(%u) %s",
                 hmxobj, pmxcd->dwControlID, mute ? "mute" : "unmute");
        return result;
    }

    return result;
}

MMRESULT WINAPI hook_mixerSetControlDetails(HMIXEROBJ             hmxobj,
                                            LPMIXERCONTROLDETAILS pmxcd,
                                            DWORD                 fdwDetails)
{
    debug("mixerSetControlDetails : mixer(0x%08x) control(%u)",
          hmxobj, pmxcd->dwControlID);

    if ((fdwDetails & MIXER_SETCONTROLDETAILSF_CUSTOM) ||
        (fdwDetails & MIXER_SETCONTROLDETAILSF_QUERYMASK)) {
        return sys_mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
    }

    if (cdvctrls.find(std::make_pair(hmxobj, pmxcd->dwControlID)) !=
        cdvctrls.end()) {
        DWORD *lvol = (DWORD *)pmxcd->paDetails;
        DWORD *rvol = lvol;
        if (pmxcd->cChannels > 1) {
            rvol = (DWORD *)((DWORD)pmxcd->paDetails + pmxcd->cbDetails);
        }
        debug("  AUDIO CD : mixer(0x%08x) control(%u) volume(0x%08x)",
                 hmxobj, pmxcd->dwControlID, MAKELONG(*lvol, *rvol));
        mci_setvol(*lvol / 65535.0f, *rvol / 65535.0f);
        return MMSYSERR_NOERROR;
    }

    if (cdmctrls.find(std::make_pair(hmxobj, pmxcd->dwControlID)) !=
        cdmctrls.end()) {
        BOOL mute = *((BOOL *)pmxcd->paDetails);
        debug("  AUDIO CD : mixer(0x%08x) control(%u) %s",
                 hmxobj, pmxcd->dwControlID, mute ? "mute" : "unmute");
        mute ? mci_mute() : mci_unmute();
        return MMSYSERR_NOERROR;
    }

    return sys_mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
}

MMRESULT WINAPI trace_mixerGetLineInfoA(HMIXEROBJ    hmxobj,
                                        LPMIXERLINEA pmxl,
                                        DWORD        fdwInfo)
{
    debug("TRACE mixerGetLineInfoA()");
    return sys_mixerGetLineInfoA(hmxobj, pmxl, fdwInfo);
}

MMRESULT WINAPI trace_mixerGetLineControlsA(HMIXEROBJ            hmxobj,
                                            LPMIXERLINECONTROLSA pmxlc,
                                            DWORD                fdwControls)
{
    debug("TRACE mixerGetLineControlsA()");
    return sys_mixerGetLineControlsA(hmxobj, pmxlc, fdwControls);
}

MMRESULT WINAPI trace_mixerGetControlDetailsA(HMIXEROBJ             hmxobj,
                                              LPMIXERCONTROLDETAILS pmxcd,
                                              DWORD                 fdwDetails)
{
    debug("TRACE mixerGetControlDetailsA()");
    return sys_mixerGetControlDetailsA(hmxobj, pmxcd, fdwDetails);
}

MMRESULT WINAPI trace_mixerSetControlDetails(HMIXEROBJ             hmxobj,
                                             LPMIXERCONTROLDETAILS pmxcd,
                                             DWORD                 fdwDetails)
{
    debug("TRACE mixerSetControlDetails()");
    return sys_mixerSetControlDetails(hmxobj, pmxcd, fdwDetails);
}

static void hook(HMODULE hDLL)
{
    if (config.audpath.empty()) {
        return;
    }

    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_mixerGetLineInfoA, hook_mixerGetLineInfoA);
    DetourAttach(&(PVOID&)sys_mixerGetLineControlsA,
                          hook_mixerGetLineControlsA);
    DetourAttach(&(PVOID&)sys_mixerGetControlDetailsA,
                          hook_mixerGetControlDetailsA);
    DetourAttach(&(PVOID&)sys_mixerSetControlDetails,
                          hook_mixerSetControlDetails);
    DetourTransactionCommit();
}

static void unhook()
{
    if (config.audpath.empty()) {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_mixerGetLineInfoA, hook_mixerGetLineInfoA);
    DetourDetach(&(PVOID&)sys_mixerGetLineControlsA,
                          hook_mixerGetLineControlsA);
    DetourDetach(&(PVOID&)sys_mixerGetControlDetailsA,
                          hook_mixerGetControlDetailsA);
    DetourDetach(&(PVOID&)sys_mixerSetControlDetails,
                          hook_mixerSetControlDetails);
    DetourTransactionCommit();
}

static void trace(HMODULE hDLL)
{
    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_mixerGetLineInfoA, trace_mixerGetLineInfoA);
    DetourAttach(&(PVOID&)sys_mixerGetLineControlsA,
                          trace_mixerGetLineControlsA);
    DetourAttach(&(PVOID&)sys_mixerGetControlDetailsA,
                          trace_mixerGetControlDetailsA);
    DetourAttach(&(PVOID&)sys_mixerSetControlDetails,
                          trace_mixerSetControlDetails);
    DetourTransactionCommit();
}

static void untrace()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_mixerGetLineInfoA, trace_mixerGetLineInfoA);
    DetourDetach(&(PVOID&)sys_mixerGetLineControlsA,
                          trace_mixerGetLineControlsA);
    DetourDetach(&(PVOID&)sys_mixerGetControlDetailsA,
                          trace_mixerGetControlDetailsA);
    DetourDetach(&(PVOID&)sys_mixerSetControlDetails,
                          trace_mixerSetControlDetails);
    DetourTransactionCommit();
}

void mix_hook(HMODULE hdll) {
    config.trace ? trace(hdll) : hook(hdll);
}

void mix_unhook() {
    config.trace ? untrace() : unhook();
}
