#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "audiere.h"
#include "detours.h"

using namespace std;
using namespace audiere;

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "audiere.lib")
#pragma comment(lib, "detours.lib")
#pragma warning(disable: 4996)

extern "C" __declspec(dllexport) void _stdcall dummy() {}

// logs --------------------------------------------------------------------------------------------

char * now()
{
    static char timebuf[32];
    time_t t;
    time(&t);
    struct tm *tminfo = localtime(&t);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tminfo);
    return timebuf;
}

#ifdef NDEBUG
#define debuglog(format, ...)
#else
#define debuglog(format, ...) \
{ \
    char buffer[1024]; \
    sprintf(buffer, format, __VA_ARGS__); \
    OutputDebugString(buffer); \
}
#endif

FILE *flog = NULL;
#define filelog(format, ...) \
{ \
    if (flog) { \
        fprintf(flog, "%s - ", now()); \
        fprintf(flog, format, __VA_ARGS__); \
        fprintf(flog, "\n"); \
        fflush(flog); \
    } \
}

#define doublelog(format, ...) \
{ \
    debuglog(format, __VA_ARGS__); \
    filelog(format, __VA_ARGS__); \
}

// hooked apis -------------------------------------------------------------------------------------

MCIERROR (WINAPI *sys_mciSendCommand) (
    MCIDEVICEID IDDevice,
    UINT        uMsg,
    DWORD_PTR   fdwCommand,
    DWORD_PTR   dwParam
) = mciSendCommand;

MCIERROR (WINAPI *sys_mciSendString) (
    LPCTSTR lpszCommand,
    LPTSTR  lpszReturnString,
    UINT    cchReturn,
    HWND    hwndCallback
) = mciSendString;

MMRESULT (WINAPI *sys_auxSetVolume) (
    UINT  uDeviceID,
    DWORD dwVolume
) = auxSetVolume;

UINT (WINAPI *sys_GetDriveType) (
    LPCTSTR lpRootPathName
) = GetDriveType;

BOOL (WINAPI *sys_GetVolumeInformation) (
    LPCTSTR lpRootPathName,
    LPTSTR  lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPTSTR  lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize
) = GetVolumeInformation;

HANDLE (WINAPI *sys_CreateFile) (
    LPCTSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
) = CreateFile;

// global variables --------------------------------------------------------------------------------

// configuration
struct _config {
    string   basepath;	// base path
    string	 oggpath;   // path for ogg files
    uint32_t ftrack;    // first audio track
    uint32_t volume;    // volume, 0~100
    bool	 hookvol;   // hook auxSetVolume
    char	 cddrive;   // cdrom drive cheating
    string	 cdvolume;  // cdrom volume
    string   freloc;    // file to be relocated
    bool	 trace;     // track only
} config;

// audio track
typedef struct _oggtrack {
    string   filename;  // file name
    uint32_t position;  // position in samples
    uint32_t length;    // length in samples
} oggtrack;

// play back variables
struct _oggplay {
    AudioDevicePtr   device;    // audiere device
    OutputStreamPtr  stream;    // audiere stream
    StopCallbackPtr  callback;  // audiere callback
    float			 volume;	// audiere volume
    vector<oggtrack> tracks;    // tracks
    uint32_t		 tcount;	// track count
    uint32_t         timefmt;   // time format
    uint32_t         track;     // current track
    uint32_t         endpos;    // stop at this position
    HWND             cbwnd;     // callback window

    _oggplay() { reset(); }
    void reset() {
        device   = 0;
        stream   = 0;
        callback = 0;
        tcount   = 0;
        timefmt  = 0;
        track    = 0;
        endpos   = 0;
        cbwnd    = 0;
        volume   = 100.0;
        tracks.clear();
    }
} oggplay;

// readable mci parameters -------------------------------------------------------------------------

const char * parse_mci_msg(UINT uMsg)
{
    static char buffer[8];
    #define case_result(msg) case msg: return #msg;
    switch (uMsg) {
        case_result(MCI_OPEN);
        case_result(MCI_CLOSE);
        case_result(MCI_PLAY);
        case_result(MCI_SEEK);
        case_result(MCI_STOP);
        case_result(MCI_PAUSE);
        case_result(MCI_SET);
        case_result(MCI_STATUS);
        case_result(MCI_RESUME);
        default: {
            sprintf(buffer, "0x%04x", uMsg);
            return buffer;
        }
    }
}

const char * parse_mci_item(DWORD item)
{
    static char buffer[8];
    #define case_result(msg) case msg: return #msg;
    switch (item) {
        case_result(MCI_STATUS_CURRENT_TRACK);
        case_result(MCI_STATUS_LENGTH);
        case_result(MCI_STATUS_MODE);
        case_result(MCI_STATUS_NUMBER_OF_TRACKS);
        case_result(MCI_STATUS_POSITION);
        case_result(MCI_STATUS_READY);
        case_result(MCI_STATUS_TIME_FORMAT);
        default: {
            sprintf(buffer, "0x%04x", item);
            return buffer;
        }
    }
}

const char * parse_mci_mode(DWORD mode)
{
    static char buffer[8];
    #define case_result(msg) case msg: return #msg;
    switch (mode) {
        case_result(MCI_MODE_NOT_READY);
        case_result(MCI_MODE_PAUSE);
        case_result(MCI_MODE_PLAY);
        case_result(MCI_MODE_STOP);
        case_result(MCI_MODE_OPEN);
        case_result(MCI_MODE_RECORD);
        case_result(MCI_MODE_SEEK);
        default: {
            sprintf(buffer, "0x%04x", mode);
            return buffer;
        }
    }
}

const char * parse_mci_tmfmt(DWORD fmt)
{
    static char buffer[8];
    #define case_result(msg) case msg: return #msg;
    switch (fmt) {
        case_result(MCI_FORMAT_BYTES);
        case_result(MCI_FORMAT_FRAMES);
        case_result(MCI_FORMAT_HMS);
        case_result(MCI_FORMAT_MILLISECONDS);
        case_result(MCI_FORMAT_MSF);
        case_result(MCI_FORMAT_SAMPLES);
        case_result(MCI_FORMAT_TMSF);
        default: {
            sprintf(buffer, "0x%04x", fmt);
            return buffer;
        }
    }
}

const char * parse_mci_cmd(UINT uMsg, DWORD_PTR fdwCommand)
{
    static char flagbuf[1024];
    flagbuf[0] = 0;

    typedef vector<pair<DWORD, const char *> > fvec;
    #define new_pair(vec, flag) vec.push_back(make_pair(flag, #flag));

    fvec flagcomm;
    new_pair(flagcomm, MCI_NOTIFY);
    new_pair(flagcomm, MCI_WAIT);

    fvec flagopen;
    new_pair(flagopen, MCI_OPEN_TYPE);
    new_pair(flagopen, MCI_OPEN_TYPE_ID);
    new_pair(flagopen, MCI_OPEN_ELEMENT);

    fvec flagplay;
    new_pair(flagplay, MCI_FROM);
    new_pair(flagplay, MCI_TO);

    fvec flagseek;
    new_pair(flagseek, MCI_SEEK_TO_END);
    new_pair(flagseek, MCI_SEEK_TO_START);
    new_pair(flagseek, MCI_TO);

    fvec flagset;
    new_pair(flagset, MCI_SET_TIME_FORMAT);

    fvec flagstatus;
    new_pair(flagstatus, MCI_STATUS_ITEM);
    new_pair(flagstatus, MCI_TRACK);
    new_pair(flagstatus, MCI_STATUS_MEDIA_PRESENT);
    new_pair(flagstatus, MCI_CDA_STATUS_TYPE_TRACK);

    fvec *flags = NULL;
    switch (uMsg) {
        case MCI_OPEN:   flags = &flagopen;   break;
        case MCI_PLAY:   flags = &flagplay;   break;
        case MCI_SEEK:   flags = &flagseek;   break;
        case MCI_SET:    flags = &flagset;    break;
        case MCI_STATUS: flags = &flagstatus; break;
    }

    if (flags) {
        for (fvec::iterator it = flags->begin(); it != flags->end(); it++) {
            if (fdwCommand & it->first) {
                if (flagbuf[0]) strcat(flagbuf, " | ");
                strcat(flagbuf, it->second);
                fdwCommand &= ~it->first;
            }
        }
    }

    for (fvec::iterator it = flagcomm.begin(); it != flagcomm.end(); it++) {
        if (fdwCommand & it->first) {
            if (flagbuf[0]) strcat(flagbuf, " | ");
            strcat(flagbuf, it->second);
            fdwCommand &= ~it->first;
        }
    }

    if (fdwCommand) {
        if (flagbuf[0]) strcat(flagbuf, " | ");
        char buffer[16];
        sprintf(buffer, "0x%08x", fdwCommand);
        strcat(flagbuf, buffer);
    }

    if (!flagbuf[0]) strcpy(flagbuf, "0");

    return flagbuf;
}

// unit conversion ---------------------------------------------------------------------------------

#define SAMPLE_RATE 	  44100
#define FRAME_RATE  	  75
#define SAMPLES_PER_FRAME (SAMPLE_RATE / FRAME_RATE)

// samples to minute:second:frame
uint32_t smp2msf(uint32_t samples)
{
    uint32_t minute = samples / SAMPLE_RATE / 60;
    uint32_t second = samples / SAMPLE_RATE % 60;
    uint32_t frame  = samples % SAMPLE_RATE / SAMPLES_PER_FRAME;
    return MCI_MAKE_MSF(minute, second, frame);
}

// track:samples to track:minute:second:frame
uint32_t tsmp2tmsf(uint32_t track, uint32_t samples)
{
    return uint8_t(track) | (smp2msf(samples) << 8);
}

// samples to milliseconds
uint32_t smp2mis(uint32_t samples)
{
    return (uint32_t)((float)samples / SAMPLE_RATE * 1000);
}

// mci position to samples
uint32_t pos2smp(DWORD pos, uint32_t timefmt)
{
    switch (timefmt) {
        case MCI_FORMAT_TMSF: {
            uint32_t track = MCI_TMSF_TRACK(pos);
            if (track > 0 && track <= oggplay.tcount)
                return oggplay.tracks[track].position          +
                       MCI_TMSF_MINUTE(pos) * SAMPLE_RATE * 60 +
                       MCI_TMSF_SECOND(pos) * SAMPLE_RATE      +
                       MCI_TMSF_FRAME (pos) * SAMPLES_PER_FRAME;
            else
                return 0;
        }

        case MCI_FORMAT_MSF:
            return MCI_MSF_MINUTE(pos) * SAMPLE_RATE * 60 +
                   MCI_MSF_SECOND(pos) * SAMPLE_RATE      +
                   MCI_MSF_FRAME (pos) * SAMPLES_PER_FRAME;

        default: // MCI_FORMAT_MILLISECONDS
            return (pos / 1000 * SAMPLE_RATE) + (pos % 1000 * SAMPLE_RATE / 1000);
    }
}

// mci position to track:samples
void pos2tsmp(DWORD pos, uint32_t timefmt, uint32_t *track, uint32_t *samples)
{
    uint32_t sp = pos2smp(pos, timefmt);
    for (size_t i = 1; i <= oggplay.tcount; i++) {
        oggtrack *t = &oggplay.tracks[i];
        if (sp < t->position) {
            *track = i;
            *samples = 0;
            return;
        } else if (sp >= t->position && sp < t->position + t->length) {
            *track = i;
            *samples = sp - t->position;
            return;
        }
    }
    *track = config.ftrack;
    *samples = 0;
}

// play back ---------------------------------------------------------------------------------------

void scan_tracks()
{
    #define GAP (2 * SAMPLE_RATE)

    // dummy track, make track number the same as vector index
    oggtrack track;
    track.position = 0;
    track.length = 0;
    oggplay.tracks.push_back(track);

    // data tracks
    DWORD pos = GAP;
    for (size_t i = 1; i < config.ftrack; i++) {
        oggtrack track;
        track.position = pos;
        track.length = GAP;
        oggplay.tracks.push_back(track);
        pos += (track.length + GAP);
    }

    // audio tracks
    AudioDevicePtr device = OpenDevice();
    if (device) {
        int i = config.ftrack;
        while (true) {
            char buffer[16];
            sprintf(buffer, "track%02d.ogg", i);
            string filename = config.oggpath + '\\' + buffer;
            OutputStreamPtr stream = OpenSound(device, filename.c_str(), true, FF_OGG);
            if (!stream) break;

            oggtrack track;
            track.filename = filename;
            track.position = pos;
            track.length = stream->getLength();
            oggplay.tracks.push_back(track);

            stream = 0;
            pos += (track.length + GAP);
            i++;
        }
        device = 0;
    }

    oggplay.tcount = oggplay.tracks.size() - 1;
}

// forward declaration
DWORD mci_stop(DWORD fdwCommand, DWORD dwParam);

class mci_callback : public RefImplementation<StopCallback>
{
public:
    void ADR_CALL streamStopped(StopEvent* event)
    {
        if (event->getReason() == StopEvent::STREAM_ENDED) {
            debuglog("mci_callback");

            // next track
            uint32_t next = oggplay.track + 1;
            mci_stop(0, 0);
            if (next <= oggplay.tcount) {
                oggtrack *track = &oggplay.tracks[next];
                if (oggplay.endpos == 0 || oggplay.endpos > track->position) {
                    oggplay.stream = OpenSound(oggplay.device, track->filename.c_str(),
                                               true, FF_OGG);
                    if (oggplay.stream) {
                        oggplay.stream->setVolume(oggplay.volume);
                        oggplay.stream->play();
                        oggplay.track = next;
                    }
                }
            }

            // notify
            if (oggplay.cbwnd) {
                PostMessage(oggplay.cbwnd, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL, 1);
            }
        }
    }
};

DWORD mci_open(DWORD fdwCommand, DWORD dwParam)
{
    debuglog("mci_open");

    MCI_OPEN_PARMS *params = (MCI_OPEN_PARMS *)dwParam;

    bool iscd = false;
    if (fdwCommand & MCI_OPEN_TYPE) {
        if (fdwCommand & MCI_OPEN_TYPE_ID) {
            iscd = LOWORD((DWORD)params->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO;
        } else {
            iscd = !strcmp(params->lpstrDeviceType, "cdaudio");
        }
    }
    if (!iscd) return MCIERR_INVALID_DEVICE_NAME;

    if (!(oggplay.device = OpenDevice())) return MCIERR_DEVICE_NOT_READY;
    oggplay.callback = new mci_callback();
    oggplay.device->registerCallback(oggplay.callback.get());
    oggplay.volume = config.volume / (float)100.0;
    scan_tracks();

    params->wDeviceID = 1;

    return 0;
}

DWORD mci_set(DWORD fdwCommand, DWORD dwParam)
{
    if (fdwCommand & MCI_SET_TIME_FORMAT) {
        MCI_SET_PARMS *params = (MCI_SET_PARMS *)dwParam;
        if (params->dwTimeFormat != MCI_FORMAT_MILLISECONDS &&
            params->dwTimeFormat != MCI_FORMAT_MSF &&
            params->dwTimeFormat != MCI_FORMAT_TMSF) {
            return -1;
        }
        oggplay.timefmt = params->dwTimeFormat;
        debuglog("mci_set: timefmt(%s)", parse_mci_tmfmt(oggplay.timefmt));
    }

    return 0;
}

DWORD mci_status(DWORD fdwCommand, DWORD dwParam)
{
    MCI_STATUS_PARMS *params = (MCI_STATUS_PARMS *)dwParam;
    debuglog("mci_status: flags(%s) item(%s) Track(%u)",
             parse_mci_cmd(MCI_STATUS, fdwCommand), parse_mci_item(params->dwItem),
             params->dwTrack);

    params->dwReturn = 0;
    if (!oggplay.device) return MCIERR_DEVICE_NOT_READY;
    if (!(fdwCommand & MCI_STATUS_ITEM)) return MCIERR_UNRECOGNIZED_COMMAND;

    switch (params->dwItem) {
        case MCI_STATUS_LENGTH: {
            DWORD length = 0;
            if (fdwCommand & MCI_TRACK) {
                if (params->dwTrack > 0 && params->dwTrack <= oggplay.tcount) {
                    length = oggplay.tracks[params->dwTrack].length;
                }
            } else if (!oggplay.tracks.empty()) {
                oggtrack *last = &(*oggplay.tracks.rbegin());
                length = last->position + last->length;
            }
            if (oggplay.timefmt == MCI_FORMAT_TMSF || oggplay.timefmt == MCI_FORMAT_MSF)
                params->dwReturn = smp2msf(length);
            else // MCI_FORMAT_MILLISECONDS
                params->dwReturn = smp2mis(length);
        }
        break;

        case MCI_STATUS_POSITION: {
            if (fdwCommand & MCI_TRACK) {
                if (params->dwTrack > 0 && params->dwTrack <= oggplay.tcount) {
                    if (oggplay.timefmt == MCI_FORMAT_TMSF)
                        params->dwReturn = params->dwTrack;
                    else if (oggplay.timefmt == MCI_FORMAT_MSF)
                        params->dwReturn = smp2msf(oggplay.tracks[params->dwTrack].position);
                    else // MCI_FORMAT_MILLISECONDS
                        params->dwReturn = smp2mis(oggplay.tracks[params->dwTrack].position);
                }
            } else if (oggplay.stream) {
                if (oggplay.timefmt == MCI_FORMAT_TMSF)
                    params->dwReturn = tsmp2tmsf(oggplay.track, oggplay.stream->getPosition());
                else if (oggplay.timefmt == MCI_FORMAT_MSF)
                    params->dwReturn = smp2msf(oggplay.tracks[oggplay.track].position +
                                               oggplay.stream->getPosition());
                else // MCI_FORMAT_MILLISECONDS
                    params->dwReturn = smp2mis(oggplay.tracks[oggplay.track].position +
                                               oggplay.stream->getPosition());
            }
        }
        break;

        case MCI_STATUS_MODE:
            params->dwReturn = (!oggplay.stream) ?				MCI_MODE_STOP :
                               (!oggplay.stream->isPlaying()) ? MCI_MODE_PAUSE :
                                                                MCI_MODE_PLAY;
            break;

        case MCI_STATUS_TIME_FORMAT:
            params->dwReturn = oggplay.timefmt;
            break;

        case MCI_STATUS_MEDIA_PRESENT:
        case MCI_STATUS_READY:
            params->dwReturn = TRUE;
            break;

        case MCI_STATUS_NUMBER_OF_TRACKS:
            params->dwReturn = oggplay.tcount;
            break;

        case MCI_STATUS_CURRENT_TRACK:
            params->dwReturn = oggplay.track;
            break;
    }

    return 0;
}

DWORD mci_pause(DWORD fdwCommand, DWORD dwParam)
{
    debuglog("mci_pause");

    if (oggplay.stream) {
        oggplay.stream->stop();
    }

    return 0;
}

DWORD mci_resume(DWORD fdwCommand, DWORD dwParam)
{
    debuglog("mci_resume");

    if (oggplay.stream) {
        oggplay.stream->play();
    }

    return 0;
}

DWORD mci_stop(DWORD fdwCommand, DWORD dwParam)
{
    debuglog("mci_stop");

    mci_pause(fdwCommand, dwParam);
    oggplay.stream = 0;
    oggplay.track  = 0;

    return 0;
}

DWORD mci_play(DWORD fdwCommand, DWORD dwParam)
{
    MCI_PLAY_PARMS *params = (MCI_PLAY_PARMS *)dwParam;
    debuglog("mci_play: from(%u) to(%u) callback(0x%08x)",
             params->dwFrom, params->dwTo, params->dwCallback);

    if (!oggplay.device) return MCIERR_DEVICE_NOT_READY;

    oggplay.endpos = 0;
    if (fdwCommand & MCI_TO)
        oggplay.endpos = pos2smp(params->dwTo, oggplay.timefmt);
    if (fdwCommand & MCI_NOTIFY)
        oggplay.cbwnd = (HWND)params->dwCallback;

    uint32_t track, pos;
    if (fdwCommand & MCI_FROM) {
        pos2tsmp(params->dwFrom, oggplay.timefmt, &track, &pos);
    } else if (oggplay.stream) {
        track = oggplay.track;
        pos = oggplay.stream->getPosition();
    } else {
        track = config.ftrack;
        pos = 0;
    }

    if (oggplay.stream && oggplay.track == track) {
        if (abs(pos - oggplay.stream->getPosition() > SAMPLE_RATE))
            oggplay.stream->setPosition(pos);
    } else {
        mci_stop(0, 0);
        oggplay.stream = OpenSound(oggplay.device, oggplay.tracks[track].filename.c_str(),
                                   true, FF_OGG);
        if (!oggplay.stream) return MCIERR_FILE_READ;
        oggplay.stream->setVolume(oggplay.volume);
        oggplay.stream->setPosition(pos);
        oggplay.track = track;
    }
    oggplay.stream->play();

    return 0;
}

DWORD mci_close(DWORD fdwCommand, DWORD dwParam)
{
    debuglog("mci_close");

    mci_stop(fdwCommand, dwParam);
    oggplay.device = 0;
    oggplay.callback = 0;

    float save = oggplay.volume;
    oggplay.reset();
    oggplay.volume = save;

    return 0;
}

// substitutes -------------------------------------------------------------------------------------

MCIERROR WINAPI hook_mciSendCommand(MCIDEVICEID IDDevice,
                                    UINT        uMsg,
                                    DWORD_PTR   fdwCommand,
                                    DWORD_PTR   dwParam)
{
    switch (uMsg) {
        case MCI_OPEN:
            return mci_open  (fdwCommand, dwParam);
        case MCI_CLOSE:
            return mci_close (fdwCommand, dwParam);
        case MCI_SET:
            return mci_set   (fdwCommand, dwParam);
        case MCI_STATUS:
            return mci_status(fdwCommand, dwParam);
        case MCI_PLAY:
            return mci_play  (fdwCommand, dwParam);
        case MCI_STOP:
            return mci_stop  (fdwCommand, dwParam);
        case MCI_PAUSE:
            return mci_pause (fdwCommand, dwParam);
        case MCI_RESUME:
            return mci_resume(fdwCommand, dwParam);
        default:
            debuglog("unknown mci action: device(0x%08x) message(%s) flags(%s)",
                     IDDevice, parse_mci_msg(uMsg), parse_mci_cmd(uMsg, fdwCommand));
            return 0;
    }
}

// convert mci time string to position
DWORD str2pos(const string &value)
{
    DWORD pos = 0;
    int i = 0;
    istringstream strm(value);
    string str;
    while (getline(strm, str, ':')) {
        pos |= ((DWORD)((BYTE)atoi(str.c_str())) << i);
        i += 8;
        if (i >= 32) break;
    }

    return pos;
}

MCIERROR WINAPI hook_mciSendString(LPCTSTR lpszCommand,
                                   LPTSTR  lpszReturnString,
                                   UINT    cchReturn,
                                   HWND    hwndCallback)
{
    debuglog("mciSendString: command(%s) callback(0x%08x)", lpszCommand, (DWORD)hwndCallback);

    string command(lpszCommand);
    transform(command.begin(), command.end(), command.begin(), ::tolower);
    if (command.find("cdaudio") == string::npos) {
        return sys_mciSendString(lpszCommand, lpszReturnString, cchReturn, hwndCallback);
    }

    if (command.find("open") != string::npos) {
        MCI_OPEN_PARMS params;
        params.lpstrDeviceType = "cdaudio";
        return mci_open(MCI_OPEN_TYPE, (DWORD)&params);
    }

    if (command.find("close") != string::npos) {
        return mci_close(0, 0);
    }

    if (command.find("stop") != string::npos) {
        return mci_stop(0, 0);
    }

    if (command.find("pause") != string::npos) {
        return mci_pause(0, 0);
    }

    if (command.find("set") != string::npos && command.find("time format") != string::npos) {
        MCI_SET_PARMS params;
        params.dwTimeFormat = (command.find("tmsf") != string::npos) ? MCI_FORMAT_TMSF :
                              (command.find("msf")  != string::npos) ? MCI_FORMAT_MSF :
                                                                       MCI_FORMAT_MILLISECONDS;
        return mci_set(MCI_SET_TIME_FORMAT, (DWORD)&params);
    }

    if (command.find("status") != string::npos) {
        MCI_STATUS_PARMS params;
        DWORD ret;

        if (command.find("mode") != string::npos) {
            params.dwItem = MCI_STATUS_MODE;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            switch (params.dwReturn) {
                case MCI_MODE_PLAY:
                    strcpy(lpszReturnString, "playing");
                    break;
                case MCI_MODE_PAUSE:
                    strcpy(lpszReturnString, "paused");
                    break;
                default:
                    strcpy(lpszReturnString, "stopped");
                    break;
            }
            return ret;
        }

        if (command.find("length track") != string::npos) {
            params.dwItem = MCI_STATUS_LENGTH;
            params.dwTrack = atoi(command.substr(command.find("track") + 6).c_str());
            ret = mci_status(MCI_STATUS_ITEM | MCI_TRACK, (DWORD)&params);
            switch (oggplay.timefmt) {
                case MCI_FORMAT_TMSF:
                case MCI_FORMAT_MSF:
                    sprintf(lpszReturnString, "%02d:%02d:%02d",
                            MCI_MSF_MINUTE(params.dwReturn),
                            MCI_MSF_SECOND(params.dwReturn),
                            MCI_MSF_FRAME(params.dwReturn));
                    break;
                case MCI_FORMAT_MILLISECONDS:
                    sprintf(lpszReturnString, "%u", params.dwReturn);
                    break;
                default:
                    strcpy(lpszReturnString, "0");
                    break;
            }
            return ret;
        }

        if (command.find("position") != string::npos) {
            params.dwItem = MCI_STATUS_POSITION;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            switch (oggplay.timefmt) {
                case MCI_FORMAT_TMSF:
                    sprintf(lpszReturnString, "%02d:%02d:%02d:%02d",
                            MCI_TMSF_TRACK (params.dwReturn),
                            MCI_TMSF_MINUTE(params.dwReturn),
                            MCI_TMSF_SECOND(params.dwReturn),
                            MCI_TMSF_FRAME (params.dwReturn));
                    break;
                case MCI_FORMAT_MSF:
                    sprintf(lpszReturnString, "%02d:%02d:%02d",
                            MCI_MSF_MINUTE(params.dwReturn),
                            MCI_MSF_SECOND(params.dwReturn),
                            MCI_MSF_FRAME (params.dwReturn));
                    break;
                case MCI_FORMAT_MILLISECONDS:
                    sprintf(lpszReturnString, "%u", params.dwReturn);
                    break;
                default:
                    strcpy(lpszReturnString, "0");
                    break;
            }
            return ret;
        }

        if (command.find("number of tracks") != string::npos) {
            params.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            sprintf(lpszReturnString, "%u", params.dwReturn);
            return ret;
        }

        if (command.find("current track") != string::npos) {
            params.dwItem = MCI_STATUS_CURRENT_TRACK;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            sprintf(lpszReturnString, "%u", params.dwReturn);
            return ret;
        }
    }

    if (command.find("play") != string::npos) {
        string sfrom, sto;
        if (command.find("from") != string::npos) {
            sfrom = command.substr(command.find("from") + 5);
            if (sfrom.find(" ") != string::npos) sfrom.erase(sfrom.find(" "));
        }
        if (command.find("to") != string::npos) {
            sto = command.substr(command.find("to") + 3);
        }

        MCI_PLAY_PARMS params;
        DWORD flag = MCI_NOTIFY;
        if (!sfrom.empty()) {
            flag |= MCI_FROM;
            params.dwFrom = str2pos(sfrom);
        }
        if (!sto.empty()) {
            flag |= MCI_TO;
            params.dwTo = str2pos(sto);
        }
        params.dwCallback = (DWORD)hwndCallback;
        return mci_play(flag, (DWORD)&params);
    }

    return 0;
}

MMRESULT WINAPI hook_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    debuglog("call auxSetVolume(0x%08x, 0x%04x)", uDeviceID, dwVolume);

    AUXCAPS caps;
    memset(&caps, 0, sizeof(caps));
    auxGetDevCaps(uDeviceID, &caps, sizeof(caps));
    if (caps.wTechnology == AUXCAPS_CDAUDIO || uDeviceID == 0xFFFFFFFF /*raiden2 hack*/) {
        oggplay.volume = LOWORD(dwVolume) / (float)65535.0;
        if (oggplay.stream) {
            oggplay.stream->setVolume(oggplay.volume);
        }
    }

    return 0;
}

UINT WINAPI hook_GetDriveType(LPCTSTR lpRootPathName)
{
    if (lpRootPathName && tolower(lpRootPathName[0]) == tolower(config.cddrive))
        return DRIVE_CDROM;
    return sys_GetDriveType(lpRootPathName);
}

BOOL WINAPI hook_GetVolumeInformation(
    LPCTSTR lpRootPathName,
    LPTSTR  lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPTSTR  lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize)
{
    sys_GetVolumeInformation(lpRootPathName,
                             lpVolumeNameBuffer,
                             nVolumeNameSize,
                             lpVolumeSerialNumber,
                             lpMaximumComponentLength,
                             lpFileSystemFlags,
                             lpFileSystemNameBuffer,
                             nFileSystemNameSize);
    if (lpRootPathName && tolower(lpRootPathName[0]) == tolower(config.cddrive))
        strcpy(lpVolumeNameBuffer, config.cdvolume.c_str());
    return TRUE;
}

HANDLE WINAPI hook_CreateFile(
    LPCTSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile)
{
    char filereloc[MAX_PATH];
    sprintf(filereloc, "%c:\\%s", config.cddrive, config.freloc.c_str());
    debuglog("file relocation: %s", filereloc);
    if (stricmp(filereloc, lpFileName) == 0) {
        sprintf(filereloc, "%s\\%s", config.basepath.c_str(), config.freloc.c_str());
        lpFileName = filereloc;
    }

    return sys_CreateFile(lpFileName,
                          dwDesiredAccess,
                          dwShareMode,
                          lpSecurityAttributes,
                          dwCreationDisposition,
                          dwFlagsAndAttributes,
                          hTemplateFile);
}

// trace routines ----------------------------------------------------------------------------------

void trace_mci_param(UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam, bool output)
{
    switch (uMsg) {
        case MCI_OPEN: {
            MCI_OPEN_PARMS *params = (MCI_OPEN_PARMS *)dwParam;
            if (!output) {
                string input;
                if (fdwCommand & MCI_OPEN_TYPE) {
                    char *type = !(fdwCommand & MCI_OPEN_TYPE_ID) ? params->lpstrDeviceType :
                                 LOWORD((DWORD)params->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO ?
                                 "MCI_DEVTYPE_CD_AUDIO" : "OTHER";
                    input += string("lpstrDeviceType(") + type + "), ";
                }
                if ((fdwCommand & MCI_OPEN_ELEMENT) && !(fdwCommand & MCI_OPEN_ELEMENT_ID)) {
                    input += string("lpstrElementName(") + params->lpstrElementName + "), ";
                }
                if (input.substr(input.size() - 2) == ", ") input.erase(input.size() - 2);
                doublelog("  MCI_OPEN input: %s", input.c_str());
            } else {
                doublelog("  MCI_OPEN output: wDeviceID(0x%08x)", params->wDeviceID);
            }
        }
        break;

        case MCI_PLAY: {
            if (!output) {
                MCI_PLAY_PARMS *params = (MCI_PLAY_PARMS *)dwParam;
                doublelog("  MCI_PLAY input: dwFrom(%d), dwTo(%d), dwCallback(0x%08x)",
                          fdwCommand & MCI_FROM ? params->dwFrom : 0,
                          fdwCommand & MCI_TO ? params->dwTo : 0, params->dwCallback);
            }
        }
        break;

        case MCI_SEEK: {
            if (!output) {
                MCI_SEEK_PARMS *params = (MCI_SEEK_PARMS *)dwParam;
                doublelog("  MCI_SEEK input: dwTo(%d), dwCallback(0x%08x)",
                          fdwCommand & MCI_TO ? params->dwTo : 0, params->dwCallback);
            }
        }
        break;

        case MCI_SET: {
            if (!output) {
                MCI_SET_PARMS *params = (MCI_SET_PARMS *)dwParam;
                doublelog("  MCI_SET input: dwTimeFormat(%s), dwCallback(0x%08x)",
                          fdwCommand & MCI_SET_TIME_FORMAT ?
                          parse_mci_tmfmt(params->dwTimeFormat) : "0",
                          params->dwCallback);
            }
        }
        break;

        case MCI_STATUS: {
            MCI_STATUS_PARMS *params = (MCI_STATUS_PARMS *)dwParam;
            if (!output) {
                doublelog("  MCI_STATUS input: dwItem(%s), dwTrack(%d), dwCallback(0x%08x)",
                          fdwCommand & MCI_STATUS_ITEM ? parse_mci_item(params->dwItem) : "0",
                          params->dwTrack, params->dwCallback);
            } else {
                switch (params->dwItem) {
                    case MCI_STATUS_MODE:
                        doublelog("  MCI_STATUS output: dwReturn(%s)",
                                  parse_mci_mode(params->dwReturn));
                        break;
                    case MCI_STATUS_TIME_FORMAT:
                        doublelog("  MCI_STATUS output: dwReturn(%s)",
                                  parse_mci_tmfmt(params->dwReturn));
                        break;
                    default:
                        doublelog("  MCI_STATUS output: dwReturn(%u)", params->dwReturn);
                        break;
                }
            }
        }
        break;

        default: {
            if (!output && dwParam) {
                MCI_GENERIC_PARMS *params = (MCI_GENERIC_PARMS *)dwParam;
                doublelog("  MCI_GENERIC input: dwCallback(0x%08x)", params->dwCallback);
            }
        }
        break;
    }
}

MCIERROR WINAPI trace_mciSendCommand(
    MCIDEVICEID IDDevice,
    UINT        uMsg,
    DWORD_PTR   fdwCommand,
    DWORD_PTR   dwParam)
{
    doublelog("call mciSendCommand(0x%08x, %s, %s, 0x%08x)",
              IDDevice, parse_mci_msg(uMsg), parse_mci_cmd(uMsg, fdwCommand), dwParam);
    trace_mci_param(uMsg, fdwCommand, dwParam, false);
    MCIERROR result = sys_mciSendCommand(IDDevice, uMsg, fdwCommand, dwParam);
    trace_mci_param(uMsg, fdwCommand, dwParam, true);
    doublelog("  return: %u", result);
    return result;
}

MCIERROR WINAPI trace_mciSendString(
    LPCTSTR lpszCommand,
    LPTSTR  lpszReturnString,
    UINT    cchReturn,
    HWND    hwndCallback)
{
    MCIERROR result = sys_mciSendString(lpszCommand, lpszReturnString, cchReturn, hwndCallback);
    doublelog("call mciSendString(\"%s\", lpszReturnString, %u, 0x%08x): %s",
              lpszCommand, cchReturn, (DWORD)hwndCallback, lpszReturnString);
    return result;
}

MMRESULT WINAPI trace_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    MMRESULT result = sys_auxSetVolume(uDeviceID, dwVolume);
    doublelog("call auxSetVolume(0x%08x, 0x%04x): %u", uDeviceID, dwVolume, result);
    return result;
}

UINT WINAPI trace_GetDriveType(LPCTSTR lpRootPathName)
{
    const char *types[] = {
        "DRIVE_UNKNOWN",
        "DRIVE_NO_ROOT_DIR",
        "DRIVE_REMOVABLE",
        "DRIVE_FIXED",
        "DRIVE_REMOTE",
        "DRIVE_CDROM",
        "DRIVE_RAMDISK"
    };
    UINT result = sys_GetDriveType(lpRootPathName);
    doublelog("call GetDriveType(\"%s\"): %s", lpRootPathName, types[result]);
    return result;
}

BOOL WINAPI trace_GetVolumeInformation(
    LPCTSTR lpRootPathName,
    LPTSTR  lpVolumeNameBuffer,
    DWORD   nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPTSTR  lpFileSystemNameBuffer,
    DWORD   nFileSystemNameSize)
{
    BOOL result = sys_GetVolumeInformation(lpRootPathName,
                                           lpVolumeNameBuffer,
                                           nVolumeNameSize,
                                           lpVolumeSerialNumber,
                                           lpMaximumComponentLength,
                                           lpFileSystemFlags,
                                           lpFileSystemNameBuffer,
                                           nFileSystemNameSize);
    doublelog("call GetVolumeInformation(\"%s\", ...): \"%s\"", lpRootPathName, lpVolumeNameBuffer);
    return result;
}

// dll entrance ------------------------------------------------------------------------------------

void load_config(const char *cfgfile)
{
    config.basepath = cfgfile;
    config.basepath.erase(config.basepath.rfind('\\'));

    char buffer[MAX_PATH];
    GetPrivateProfileString("smartcda", "oggpath",  "", buffer, MAX_PATH, cfgfile);
    config.oggpath  = config.basepath + '\\' + buffer;
    GetPrivateProfileString("smartcda", "cddrive",  "", buffer, MAX_PATH, cfgfile);
    config.cddrive  = buffer[0];
    GetPrivateProfileString("smartcda", "cdvolume", "", buffer, MAX_PATH, cfgfile);
    config.cdvolume = buffer;
    GetPrivateProfileString("smartcda", "filereloc", "", buffer, MAX_PATH, cfgfile);
    config.freloc   = buffer;
    config.ftrack   = GetPrivateProfileInt("smartcda", "1staudio", 2, cfgfile);
    config.volume   = GetPrivateProfileInt("smartcda", "volume", 100, cfgfile);
    config.hookvol  = GetPrivateProfileInt("smartcda", "hookvol",  0, cfgfile) == 1;
    config.trace    = GetPrivateProfileInt("smartcda", "trace",    0, cfgfile) == 1;
}

void patch_memory(const char *cfgfile)
{
    #define MAX_ENTRY_LEN 32

    char buffer[1024];
    GetPrivateProfileString("smartcda", "memory", "", buffer, sizeof(buffer), cfgfile);
    string memory(buffer);

    while (!memory.empty()) {
        string entry;
        size_t semicolon = memory.find(';');
        if (semicolon != string::npos) {
            entry = memory.substr(0, semicolon);
            memory.erase(0, semicolon + 1);
        } else {
            entry = memory;
            memory.clear();
        }

        size_t colon = entry.find(':');
        if (colon != string::npos) {
            string saddr = entry.substr(0, colon);
            entry.erase(0, colon + 1);
            if (entry.size() <= MAX_ENTRY_LEN * 2) {
                debuglog("memory patch %s:%s", saddr.c_str(), entry.c_str());
                uint32_t addr = strtol(saddr.c_str(), NULL, 16);
                uint8_t value[MAX_ENTRY_LEN];
                for (size_t i = 0; i < entry.size() / 2; i++)
                    value[i] = (uint8_t)strtol(entry.substr(i * 2, 2).c_str(), NULL, 16);
                WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, (LPCVOID)value,
                                   entry.size() / 2, NULL);
            }
        }
    }
}

void hook(HMODULE hDLL)
{
    HANDLE thread = GetCurrentThread();

    DisableThreadLibraryCalls(hDLL);

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_mciSendCommand, hook_mciSendCommand);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_mciSendString, hook_mciSendString);
    DetourTransactionCommit();

    if (config.hookvol)
    {
        DetourTransactionBegin();
        DetourUpdateThread(thread);
        DetourAttach(&(PVOID&)sys_auxSetVolume, hook_auxSetVolume);
        DetourTransactionCommit();
    }

    if (config.cddrive) {
        DetourTransactionBegin();
        DetourUpdateThread(thread);
        DetourAttach(&(PVOID&)sys_GetDriveType, hook_GetDriveType);
        DetourTransactionCommit();

        if (!config.cdvolume.empty()) {
            DetourTransactionBegin();
            DetourUpdateThread(thread);
            DetourAttach(&(PVOID&)sys_GetVolumeInformation, hook_GetVolumeInformation);
            DetourTransactionCommit();
        }

        if (!config.freloc.empty()) {
            DetourTransactionBegin();
            DetourUpdateThread(thread);
            DetourAttach(&(PVOID&)sys_CreateFile, hook_CreateFile);
            DetourTransactionCommit();
        }
    }
}

void unhook()
{
    HANDLE thread = GetCurrentThread();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_mciSendCommand, hook_mciSendCommand);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_mciSendString, hook_mciSendString);
    DetourTransactionCommit();

    if (config.hookvol) {
        DetourTransactionBegin();
        DetourUpdateThread(thread);
        DetourDetach(&(PVOID&)sys_auxSetVolume, hook_auxSetVolume);
        DetourTransactionCommit();
    }

    if (config.cddrive) {
        DetourTransactionBegin();
        DetourUpdateThread(thread);
        DetourDetach(&(PVOID&)sys_GetDriveType, hook_GetDriveType);
        DetourTransactionCommit();

        if (!config.cdvolume.empty()) {
            DetourTransactionBegin();
            DetourUpdateThread(thread);
            DetourDetach(&(PVOID&)sys_GetVolumeInformation, hook_GetVolumeInformation);
            DetourTransactionCommit();
        }

        if (!config.freloc.empty()) {
            DetourTransactionBegin();
            DetourUpdateThread(thread);
            DetourDetach(&(PVOID&)sys_CreateFile, hook_CreateFile);
            DetourTransactionCommit();
        }
    }
}

void trace_hook(HMODULE hDLL)
{
    flog = fopen("trace.log", "a");
    doublelog("smartcda launched");

    HANDLE thread = GetCurrentThread();

    DisableThreadLibraryCalls(hDLL);

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_mciSendCommand, trace_mciSendCommand);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_mciSendString, trace_mciSendString);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_auxSetVolume, trace_auxSetVolume);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_GetDriveType, trace_GetDriveType);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourAttach(&(PVOID&)sys_GetVolumeInformation, trace_GetVolumeInformation);
    DetourTransactionCommit();
}

void trace_unhook()
{
    HANDLE thread = GetCurrentThread();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_mciSendCommand, trace_mciSendCommand);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_mciSendString, trace_mciSendString);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_auxSetVolume, trace_auxSetVolume);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_GetDriveType, trace_GetDriveType);
    DetourTransactionCommit();

    DetourTransactionBegin();
    DetourUpdateThread(thread);
    DetourDetach(&(PVOID&)sys_GetVolumeInformation, trace_GetVolumeInformation);
    DetourTransactionCommit();

    doublelog("smartcda quit");
    fprintf(flog, "\n");
    fclose(flog);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH: {
            char buffer[MAX_PATH];
            GetModuleFileName(hinstDLL, buffer, MAX_PATH);
            string cfgfile(buffer);
            cfgfile.erase(cfgfile.rfind('\\'));
            cfgfile += "\\smartcda.ini";
            load_config(cfgfile.c_str());

            if (config.trace)
                trace_hook(hinstDLL);
            else {
                patch_memory(cfgfile.c_str());
                hook(hinstDLL);
            }
        }
        break;

        case DLL_PROCESS_DETACH:
            config.trace ? trace_unhook() : unhook();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}
