# SmartCDA

[中文说明](readme.zh.md)

SmartCDA is a small utility to make old games with CD sound tracks portable.

## Features

* Emulate Audio CD

  Re-implement MCI audio CD playback by playing FLAC, Ogg or MP3 files, support in-game volume control.

* Relocate Files on CD

  Simple virtual CDROM, allow games to read local files on hard disk instead of data files on CDROM.

* Relocate Registry Keys

  Relocate specific registry keys from HKEY_LOCAL_MACHINE to HKEY_CURRENT_USER, so games can save settings without administrator privilege.

* High DPI Scaling Mode

  Scale game window size on high DPI monitors, so we don't need to adjust compatibility settings manually.

* Memory Patcher

  Hack games by patching memory of game process.

## User Guide

1. Extract audio tracks and encode into FLAC, Ogg or MP3 files (named track02.ogg, track03.ogg, etc.), copy music files to the game folder or any sub folder.

2. If there are data files on the disc required by the game, copy them to the game folder or any sub folder.

3. Copy smartcda.exe, smartcda.dll, smartcda.ini and audiere.dll to the game folder.

4. Edit smartcda.ini to adjust settings.

5. Launch the game by running smartcda.exe.

## Dependencies

* [Audiere](https://github.com/cdfmr/audiere)

* [Detours](https://github.com/microsoft/Detours)

## License

Audiere is licensed under [LGPL](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html).

Detours is licensed under [MIT](https://github.com/microsoft/Detours/blob/main/LICENSE.md).

The icon file comes from Oxygen Icon Theme which is licensed under [LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html).

All other source files of SmartCDA project are licensed under [LGPL v3](https://www.gnu.org/licenses/lgpl-3.0.html). To make modifications not covered by LGPL, contact me for a sublicense.

## Buy Me a Coffee

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/E8B624L2PD)
