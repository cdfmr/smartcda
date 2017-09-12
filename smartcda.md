# SmartCDA

Hook MCI functions to re-implement audio cd playback and play ogg music files
instead of cd tracks, so we can make some old games portable without cdrom.

## How to use

1. Extract cd tracks and encode into ogg files (track02.ogg, track03.ogg, etc.),
   copy them into game folder.
2. Copy smartcda.exe, smartcda.dll and smartcda.ini into game folder.
3. Edit smartcda.ini to adjust settings.
4. Launch game by executing smartcda.exe.

## Dependencies

* [Detours Express](https://www.microsoft.com/en-us/download/details.aspx?id=52586)
* [Audiere](http://audiere.sourceforge.net/)
