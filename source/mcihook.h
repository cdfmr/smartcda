#pragma once

#include <windows.h>

void mci_hook(HMODULE hdll);
void mci_unhook();
void mci_getvol(float &lvol, float &rvol);
void mci_setvol(float lvol, float rvol);
void mci_mute();
void mci_unmute();
bool mci_ismute();
