#include <stdint.h>
#include <math.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <audiere.h>
#include <detours.h>
#include "config.h"
#include "log.h"
#include "utils.h"
#include "fshook.h"

MCIERROR (WINAPI *sys_mciSendCommandA) (
    MCIDEVICEID IDDevice,
    UINT        uMsg,
    DWORD_PTR   fdwCommand,
    DWORD_PTR   dwParam
) = mciSendCommandA;

MCIERROR (WINAPI *sys_mciSendStringA) (
    LPCSTR lpszCommand,
    LPSTR  lpszReturnString,
    UINT   cchReturn,
    HWND   hwndCallback
) = mciSendStringA;

typedef struct _audtrack {
    std::string filename;
    uint32_t    position;  // samples
    uint32_t    length;    // samples
} audtrack;

struct _audplay {
    audiere::AudioDevicePtr  device;    // audiere device
    audiere::OutputStreamPtr stream;    // audiere stream
    audiere::StopCallbackPtr callback;  // audiere callback
    float                    volume;    // audiere volume
    float                    pan;       // audiere pan
    std::vector<audtrack>    tracks;    // tracks
    uint32_t                 tcount;    // track count
    uint32_t                 timefmt;   // time format
    uint32_t                 track;     // current track
    uint32_t                 endpos;    // stop at this position
    HWND                     cbwnd;     // callback window

    _audplay() {
        reset();
    }
    void reset() {
        device   = 0;
        stream   = 0;
        callback = 0;
        volume   = 1.0;
        pan      = 0.0;
        tcount   = 0;
        timefmt  = 0;
        track    = 0;
        endpos   = 0;
        cbwnd    = 0;
        tracks.clear();
    }
} audplay;

// readable mci parameters

#define case_result(msg) case msg: return #msg;

const char * parse_mci_msg(UINT uMsg)
{
    static char buffer[8];
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

    typedef std::vector<std::pair<DWORD, const char *>> fvec;
    #define new_pair(vec, flag) vec.push_back(std::make_pair(flag, #flag));

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

// unit conversion

#define SAMPLE_RATE       44100
#define FRAME_RATE        75
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
            if (track > 0 && track <= audplay.tcount)
                return audplay.tracks[track].position          +
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
            return pos / 1000 * SAMPLE_RATE + pos % 1000 * SAMPLE_RATE / 1000;
    }
}

// mci position to track:samples
void pos2tsmp(DWORD pos, uint32_t timefmt, uint32_t *track, uint32_t *samples)
{
    uint32_t sp = pos2smp(pos, timefmt);
    for (size_t i = 1; i <= audplay.tcount; i++) {
        audtrack *t = &audplay.tracks[i];
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

    *track = config.audfirst;
    *samples = 0;
}

// mci time string to position
DWORD str2pos(const std::string &value)
{
    DWORD pos = 0;
    int i = 0;
    std::istringstream strm(value);
    std::string str;
    while (std::getline(strm, str, ':')) {
        pos |= ((DWORD)((BYTE)atoi(str.c_str())) << i);
        i += 8;
        if (i >= 32) break;
    }

    return pos;
}

// mci value from key
std::string key2val(const std::vector<std::string> &params,
                    const std::string &key)
{
    auto it = std::find_if(params.begin(), params.end(),
                           [&key](const std::string str) {
                               return str == key;
                           });
    if (it != params.end() && ++it != params.end()) {
        return *it;
    }
    return std::string();
}

// play back

void mci_getvol(float &lvol, float &rvol)
{
    debug("  AUDIERE : volume %.2f pan %.2f", audplay.volume, audplay.pan);
    if (audplay.pan <= 0) {
        lvol = audplay.volume;
        rvol = audplay.volume * (1.0f + audplay.pan);
    } else {
        lvol = audplay.volume * (1.0f - audplay.pan);
        rvol = audplay.volume;
    }
}

void mci_setvol(float lvol, float rvol)
{
    audplay.volume = max(lvol, rvol);
    audplay.pan = fabs(audplay.volume) < 0.000001 ? 0 :
                  lvol >= rvol ? rvol / lvol - 1.0f : 1.0f - lvol / rvol;
    debug("  AUDIERE : volume %.2f pan %.2f", audplay.volume, audplay.pan);
    if (audplay.stream) {
        audplay.stream->setVolume(audplay.volume);
        audplay.stream->setPan(audplay.pan);
    }
}

static float svolume = 1.0;

void mci_mute()
{
    svolume = audplay.volume;
    audplay.volume = 0.0;
    if (audplay.stream) {
        audplay.stream->setVolume(audplay.volume);
    }
}

void mci_unmute()
{
    audplay.volume = svolume;
    if (audplay.stream) {
        audplay.stream->setVolume(audplay.volume);
    }
}

bool mci_ismute()
{
    return fabs(audplay.volume) < 0.000001;
}

void scan_tracks()
{
    #define GAP (2 * SAMPLE_RATE)

    // dummy track, make track number the same as vector index
    audtrack track;
    track.position = 0;
    track.length = 0;
    audplay.tracks.push_back(track);

    // data tracks
    DWORD pos = GAP;
    for (size_t i = 1; i < config.audfirst; i++) {
        audtrack track;
        track.position = pos;
        track.length = GAP;
        audplay.tracks.push_back(track);
        pos += (track.length + GAP);
    }

    // audio tracks
    int i = config.audfirst;
    while (true) {
        const char *formats[] = { "flac", "ogg", "mp3", nullptr };
        audiere::OutputStreamPtr stream = 0;
        std::string filename;
        for (size_t j = 0; formats[j] != nullptr; j++) {
            char buffer[16];
            sprintf(buffer, "track%02d.%s", i, formats[j]);
            filename = config.basepath + '\\' + config.audpath + '\\' + buffer;
            stream = audiere::OpenSound(audplay.device, filename.c_str(), true);
            if (stream) break;
        }
        if (!stream) break;

        audtrack track;
        track.filename = filename;
        track.position = pos;
        track.length = stream->getLength();
        audplay.tracks.push_back(track);
        debug("AUDIERE scan track : %s", filename.c_str());

        stream = 0;
        pos += (track.length + GAP);
        i++;
    }

    audplay.tcount = audplay.tracks.size() - 1;
}

// forward declaration
DWORD mci_stop(DWORD fdwCommand, DWORD dwParam);

class mci_callback : public audiere::RefImplementation<audiere::StopCallback>
{
public:
    void ADR_CALL streamStopped(audiere::StopEvent* event)
    {
        if (event->getReason() == audiere::StopEvent::STREAM_ENDED) {
            debug("AUDIERE stream ended");

            // next track
            uint32_t next = audplay.track + 1;
            mci_stop(0, 0);
            if (next <= audplay.tcount) {
                audtrack *track = &audplay.tracks[next];
                if (audplay.endpos == 0 || audplay.endpos > track->position) {
                    audplay.stream = audiere::OpenSound(audplay.device,
                                                        track->filename.c_str(),
                                                        true);
                    if (audplay.stream) {
                        audplay.stream->setVolume(audplay.volume);
                        audplay.stream->setPan(audplay.pan);
                        audplay.stream->play();
                        audplay.track = next;
                        debug("  AUDIERE play : %s", track->filename.c_str());
                    }
                }
            }

            // notify
            if (audplay.cbwnd) {
                PostMessage(audplay.cbwnd, MM_MCINOTIFY,
                            MCI_NOTIFY_SUCCESSFUL, (DWORD)audplay.device.get());
            }
        }
    }
};

DWORD mci_open(DWORD fdwCommand, DWORD dwParam)
{
    MCI_OPEN_PARMS *params = (MCI_OPEN_PARMS *)dwParam;

    bool cdaudio = false;
    if (fdwCommand & MCI_OPEN_TYPE) {
        if (fdwCommand & MCI_OPEN_TYPE_ID) {
            cdaudio =
                LOWORD((DWORD)params->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO;
        } else {
            cdaudio = !strcmp(params->lpstrDeviceType, "cdaudio");
        }
    }

    // cd audio
    if (cdaudio) {
        debug("  MCI_OPEN : cdaudio");

        if (!audplay.device) {
            if (!(audplay.device = audiere::OpenDevice())) {
                debug("  AUDIERE failed to open audio device")
                return MCIERR_DEVICE_NOT_READY;
            }

            debug("  DEVICE ID : 0x%08x", (DWORD)audplay.device.get());
            audplay.callback = new mci_callback();
            audplay.device->registerCallback(audplay.callback.get());
            scan_tracks();
        }

        params->wDeviceID = (DWORD)audplay.device.get();
        return MMSYSERR_NOERROR;

    // other media
    } else {
        LPCTSTR element = nullptr;
        std::string reloc;
        if ((fdwCommand & MCI_OPEN_ELEMENT) &&
            !(fdwCommand & MCI_OPEN_ELEMENT_ID)) {
            debug("  MCI_OPEN : %s", params->lpstrElementName);
            if (fs_reloc(std::string(params->lpstrElementName), reloc)) {
                debug("  RELOCATE : %s", reloc.c_str());
                element = params->lpstrElementName;
                params->lpstrElementName = reloc.c_str();
            }
        }

        MCIERROR result = sys_mciSendCommandA(0, MCI_OPEN, fdwCommand, dwParam);
        if (element) {
            params->lpstrElementName = element;
        }
        return result;
    }
}

DWORD mci_play(DWORD fdwCommand, DWORD dwParam)
{
    MCI_PLAY_PARMS *params = (MCI_PLAY_PARMS *)dwParam;
    debug("  MCI_PLAY : from(%u) to(%u) callback(0x%08x)",
             params->dwFrom, params->dwTo, params->dwCallback);

    if (!audplay.device) return MCIERR_DEVICE_NOT_READY;

    audplay.endpos = 0;
    if (fdwCommand & MCI_TO)
        audplay.endpos = pos2smp(params->dwTo, audplay.timefmt);
    if (fdwCommand & MCI_NOTIFY)
        audplay.cbwnd = (HWND)params->dwCallback;

    uint32_t track, pos;
    if (fdwCommand & MCI_FROM) {
        pos2tsmp(params->dwFrom, audplay.timefmt, &track, &pos);
    } else if (audplay.stream) {
        track = audplay.track;
        pos = audplay.stream->getPosition();
    } else {
        track = config.audfirst;
        pos = 0;
    }

    if (audplay.stream && audplay.track == track) {
        if (abs(pos - audplay.stream->getPosition() > SAMPLE_RATE)) {
            audplay.stream->setPosition(pos);
        }
    } else {
        mci_stop(0, 0);
        audplay.stream =
            audiere::OpenSound(audplay.device,
                               audplay.tracks[track].filename.c_str(),
                               true);
        if (!audplay.stream) return MCIERR_FILE_READ;
        audplay.stream->setVolume(audplay.volume);
        audplay.stream->setPan(audplay.pan);
        audplay.stream->setPosition(pos);
        audplay.track = track;
    }
    debug("  AUDIERE play : %s", audplay.tracks[track].filename.c_str());
    audplay.stream->play();

    return MMSYSERR_NOERROR;
}

DWORD mci_pause(DWORD fdwCommand, DWORD dwParam)
{
    if (audplay.stream) {
        audplay.stream->stop();
    }

    return MMSYSERR_NOERROR;
}

DWORD mci_resume(DWORD fdwCommand, DWORD dwParam)
{
    if (audplay.stream) {
        audplay.stream->play();
    }

    return MMSYSERR_NOERROR;
}

DWORD mci_stop(DWORD fdwCommand, DWORD dwParam)
{
    mci_pause(fdwCommand, dwParam);
    audplay.stream = 0;
    audplay.track  = 0;

    return MMSYSERR_NOERROR;
}

DWORD mci_close(DWORD fdwCommand, DWORD dwParam)
{
    mci_stop(fdwCommand, dwParam);
    audplay.device = 0;
    audplay.callback = 0;

    float svol = audplay.volume;
    float span = audplay.pan;
    audplay.reset();
    audplay.volume = svol;
    audplay.pan = span;

    return MMSYSERR_NOERROR;
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
        audplay.timefmt = params->dwTimeFormat;
        debug("  MCI_SET_TIME_FORMAT: %s", parse_mci_tmfmt(audplay.timefmt));
    }

    return MMSYSERR_NOERROR;
}

DWORD mci_status(DWORD fdwCommand, DWORD dwParam)
{
    MCI_STATUS_PARMS *params = (MCI_STATUS_PARMS *)dwParam;
    debug("  MCI_STATUS : flags(%s) item(%s) track(%u)",
             parse_mci_cmd(MCI_STATUS, fdwCommand),
             parse_mci_item(params->dwItem),
             params->dwTrack);

    params->dwReturn = 0;
    if (!audplay.device) return MCIERR_DEVICE_NOT_READY;
    if (!(fdwCommand & MCI_STATUS_ITEM)) return MCIERR_UNRECOGNIZED_COMMAND;

    switch (params->dwItem) {
        case MCI_STATUS_LENGTH: {
            DWORD length = 0;
            if (fdwCommand & MCI_TRACK) {
                if (params->dwTrack > 0 && params->dwTrack <= audplay.tcount) {
                    length = audplay.tracks[params->dwTrack].length;
                }
            } else if (!audplay.tracks.empty()) {
                audtrack *last = &(*audplay.tracks.rbegin());
                length = last->position + last->length;
            }
            if (audplay.timefmt == MCI_FORMAT_TMSF ||
                audplay.timefmt == MCI_FORMAT_MSF) {
                params->dwReturn = smp2msf(length);
            } else { // MCI_FORMAT_MILLISECONDS
                params->dwReturn = smp2mis(length);
            }
        }
        break;

        case MCI_STATUS_POSITION: {
            if (fdwCommand & MCI_TRACK) {
                if (params->dwTrack > 0 && params->dwTrack <= audplay.tcount) {
                    if (audplay.timefmt == MCI_FORMAT_TMSF) {
                        params->dwReturn = params->dwTrack;
                    } else if (audplay.timefmt == MCI_FORMAT_MSF) {
                        params->dwReturn =
                            smp2msf(audplay.tracks[params->dwTrack].position);
                    } else { // MCI_FORMAT_MILLISECONDS
                        params->dwReturn =
                            smp2mis(audplay.tracks[params->dwTrack].position);
                    }
                }
            } else if (audplay.stream) {
                if (audplay.timefmt == MCI_FORMAT_TMSF) {
                    params->dwReturn =
                        tsmp2tmsf(audplay.track, audplay.stream->getPosition());
                } else if (audplay.timefmt == MCI_FORMAT_MSF) {
                    params->dwReturn =
                        smp2msf(audplay.tracks[audplay.track].position +
                                audplay.stream->getPosition());
                } else { // MCI_FORMAT_MILLISECONDS
                    params->dwReturn =
                        smp2mis(audplay.tracks[audplay.track].position +
                                audplay.stream->getPosition());
                }
            }
        }
        break;

        case MCI_STATUS_MODE:
            params->dwReturn = (!audplay.stream) ?              MCI_MODE_STOP  :
                               (!audplay.stream->isPlaying()) ? MCI_MODE_PAUSE :
                                                                MCI_MODE_PLAY;
            break;

        case MCI_STATUS_TIME_FORMAT:
            params->dwReturn = audplay.timefmt;
            break;

        case MCI_STATUS_MEDIA_PRESENT:
        case MCI_STATUS_READY:
            params->dwReturn = TRUE;
            break;

        case MCI_STATUS_NUMBER_OF_TRACKS:
            params->dwReturn = audplay.tcount;
            break;

        case MCI_STATUS_CURRENT_TRACK:
            params->dwReturn = audplay.track;
            break;
    }

    return MMSYSERR_NOERROR;
}

MCIERROR WINAPI hook_mciSendCommandA(MCIDEVICEID IDDevice,
                                     UINT        uMsg,
                                     DWORD_PTR   fdwCommand,
                                     DWORD_PTR   dwParam)
{
    debug("mciSendCommandA : device(0x%08x) message(%s) flags(%s)",
          IDDevice, parse_mci_msg(uMsg), parse_mci_cmd(uMsg, fdwCommand));

    if (uMsg != MCI_OPEN &&
        IDDevice != (DWORD)audplay.device.get() &&
        // some buggy games only store low-order word of the device id
        IDDevice != LOWORD((DWORD)audplay.device.get())) {
        return sys_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    }

    switch (uMsg) {
        case MCI_OPEN:
            return mci_open  (fdwCommand, dwParam);
        case MCI_CLOSE:
            return mci_close (fdwCommand, dwParam);
        case MCI_PLAY:
            return mci_play  (fdwCommand, dwParam);
        case MCI_PAUSE:
            return mci_pause (fdwCommand, dwParam);
        case MCI_RESUME:
            return mci_resume(fdwCommand, dwParam);
        case MCI_STOP:
            return mci_stop  (fdwCommand, dwParam);
        case MCI_SET:
            return mci_set   (fdwCommand, dwParam);
        case MCI_STATUS:
            return mci_status(fdwCommand, dwParam);
        default:
            return MMSYSERR_NOERROR;
    }
}

MCIERROR WINAPI hook_mciSendStringA(LPCSTR lpszCommand,
                                    LPSTR  lpszReturnString,
                                    UINT   cchReturn,
                                    HWND   hwndCallback)
{
    debug("mciSendStringA : command(%s) callback(0x%08x)",
          lpszCommand, (DWORD)hwndCallback);

    std::string command = str_trim(std::string(lpszCommand));
    std::string verb, device, param;
    auto it = std::find_if(command.begin(), command.end(), isspace);
    verb = str_lower(std::string(command.begin(), it));
    command = str_ltrim(std::string(it, command.end()));
    if (!command.empty() && command[0] == '"') {
        size_t p = command.find('"', 1);
        if (p != std::string::npos) {
            device = str_trim(command.substr(1, p - 1));
            param = str_ltrim(command.substr(p + 1));
        } else {
            device = str_ltrim(command.substr(1));
        }
    } else {
        it = std::find_if(command.begin(), command.end(), isspace);
        device = std::string(command.begin(), it);
        param = str_ltrim(std::string(it, command.end()));
    }

    static std::string cdalias;
    bool cdaudio = (str_lower(device) == "cdaudio") ||
                   (!cdalias.empty() && str_lower(device) == cdalias);
    if (!cdaudio && verb == "open") {
        auto vparam = str_split(str_lower(param));
        std::string stype = key2val(vparam, "type");
        if (stype == "cdaudio") {
            cdaudio = true;
        }
    }

    if (!cdaudio) {
        if (verb == "open") {
            std::string reloc;
            if (fs_reloc(device, reloc)) {
                debug("  MCI_OPEN : %s", device.c_str());
                debug("  RELOCATE : %s", reloc.c_str());
                command = verb + " \"" + reloc + '"';
                if (!param.empty()) {
                    command = command + ' ' + param;
                }
                lpszCommand = command.c_str();
            }
        }
        return sys_mciSendStringA(lpszCommand, lpszReturnString,
                                  cchReturn, hwndCallback);
    }

    if (verb == "open") {
        auto vparam = str_split(str_lower(param));
        cdalias = key2val(vparam, "alias");
        MCI_OPEN_PARMS params;
        params.lpstrDeviceType = "cdaudio";
        return mci_open(MCI_OPEN_TYPE, (DWORD)&params);
    }

    if (verb == "close") {
        return mci_close(0, 0);
    }

    if (verb == "play") {
        auto vparam = str_split(str_lower(param));
        std::string sfrom = key2val(vparam, "from");
        std::string sto = key2val(vparam, "to");
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

    if (verb == "pause") {
        return mci_pause(0, 0);
    }

    if (verb == "resume") {
        return mci_resume(0, 0);
    }

    if (verb == "stop") {
        return mci_stop(0, 0);
    }

    if (verb == "set") {
        auto vparam = str_split(str_lower(param));
        if (vparam.size() >= 3 &&
            vparam[0] == "time" && vparam[1] == "format") {
            MCI_SET_PARMS setp;
            if (vparam[2] == "tmsf") {
                setp.dwTimeFormat = MCI_FORMAT_TMSF;
            } else if (vparam[2] == "msf") {
                setp.dwTimeFormat = MCI_FORMAT_MSF;
            } else if (vparam[2] == "milliseconds") {
                setp.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
            } else {
                return MMSYSERR_NOERROR;
            }
            return mci_set(MCI_SET_TIME_FORMAT, (DWORD)&setp);
        }
    }

    if (verb == "status") {
        auto vparam = str_split(str_lower(param));
        MCI_STATUS_PARMS params;
        DWORD ret = 0;

        if (vparam.size() >= 2 &&
            vparam[0] == "media" && vparam[1] == "present") {
            strcpy(lpszReturnString, "true");
            return ret;
        }

        if (!vparam.empty() && vparam[0] == "ready") {
            strcpy(lpszReturnString, "true");
            return ret;
        }

        if (vparam.size() >= 3 &&
            vparam[0] == "number" && vparam[1] == "of" &&
            vparam[1] == "tracks") {
            params.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            sprintf(lpszReturnString, "%u", params.dwReturn);
            return ret;
        }

        if (!vparam.empty() && vparam[0] == "length") {
            DWORD flag = MCI_STATUS_ITEM;
            params.dwItem = MCI_STATUS_LENGTH;
            if (vparam.size() >= 3 && vparam[1] == "track") {
                flag |= MCI_TRACK;
                params.dwTrack = atoi(vparam[2].c_str());
            }
            ret = mci_status(flag, (DWORD)&params);
            switch (audplay.timefmt) {
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

        if (vparam.size() >= 2 &&
            vparam[0] == "time" && vparam[1] == "format") {
            params.dwItem = MCI_STATUS_TIME_FORMAT;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            switch (params.dwReturn) {
                case MCI_FORMAT_TMSF:
                    strcpy(lpszReturnString, "tmsf");
                    break;
                case MCI_FORMAT_MSF:
                    strcpy(lpszReturnString, "msf");
                    break;
                default:
                    strcpy(lpszReturnString, "milliseconds");
                    break;
            }
            return ret;
        }

        if (!vparam.empty() && vparam[0] == "mode") {
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

        if (vparam.size() >= 2 &&
            vparam[0] == "current" && vparam[1] == "track") {
            params.dwItem = MCI_STATUS_CURRENT_TRACK;
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            sprintf(lpszReturnString, "%u", params.dwReturn);
            return ret;
        }

        if (!vparam.empty() && vparam[0] == "position") {
            DWORD flag = MCI_STATUS_ITEM;
            params.dwItem = MCI_STATUS_POSITION;
            if (vparam.size() >= 3 && vparam[1] == "track") {
                flag |= MCI_TRACK;
                params.dwTrack = atoi(vparam[2].c_str());
            }
            ret = mci_status(MCI_STATUS_ITEM, (DWORD)&params);
            switch (audplay.timefmt) {
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
    }

    return MMSYSERR_NOERROR;
}

void trace_mci_param(UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam,
                     bool output)
{
    if (!dwParam) return;

    switch (uMsg) {
        case MCI_OPEN: {
            MCI_OPEN_PARMS *params = (MCI_OPEN_PARMS *)dwParam;
            if (!output) {
                std::string input;
                if (fdwCommand & MCI_OPEN_TYPE) {
                    const char *type = "other";
                    if (!(fdwCommand & MCI_OPEN_TYPE_ID)) {
                        type = params->lpstrDeviceType;
                    } else if (LOWORD((DWORD)params->lpstrDeviceType) ==
                               MCI_DEVTYPE_CD_AUDIO) {
                        type = "cdaudio";
                    }
                    input += std::string("device(") + type + "), ";
                }
                if ((fdwCommand & MCI_OPEN_ELEMENT) &&
                    !(fdwCommand & MCI_OPEN_ELEMENT_ID)) {
                    const char *elem = params->lpstrElementName;
                    input += std::string("element(\"") + elem + "\"), ";
                }
                if (input.substr(input.size() - 2) == ", ") {
                    input.erase(input.size() - 2);
                }
                debug("  MCI_OPEN input : %s", input.c_str());
            } else {
                debug("  MCI_OPEN output : device(0x%08x)", params->wDeviceID);
            }
        }
        break;

        case MCI_PLAY: {
            if (!output) {
                MCI_PLAY_PARMS *params = (MCI_PLAY_PARMS *)dwParam;
                debug("  MCI_PLAY input: from(%d), to(%d), callback(0x%08x)",
                      fdwCommand & MCI_FROM ? params->dwFrom : 0,
                      fdwCommand & MCI_TO   ? params->dwTo   : 0,
                      params->dwCallback);
            }
        }
        break;

        case MCI_SEEK: {
            if (!output) {
                MCI_SEEK_PARMS *params = (MCI_SEEK_PARMS *)dwParam;
                debug("  MCI_SEEK input : to(%d), callback(0x%08x)",
                      fdwCommand & MCI_TO ? params->dwTo : 0,
                      params->dwCallback);
            }
        }
        break;

        case MCI_SET: {
            if (!output) {
                MCI_SET_PARMS *params = (MCI_SET_PARMS *)dwParam;
                debug("  MCI_SET input : timeformat(%s), callback(0x%08x)",
                      fdwCommand & MCI_SET_TIME_FORMAT ?
                          parse_mci_tmfmt(params->dwTimeFormat) : "0",
                      params->dwCallback);
            }
        }
        break;

        case MCI_STATUS: {
            MCI_STATUS_PARMS *params = (MCI_STATUS_PARMS *)dwParam;
            if (!output) {
                debug("  MCI_STATUS input : item(%s), track(%d), "
                          "callback(0x%08x)",
                      fdwCommand & MCI_STATUS_ITEM ?
                          parse_mci_item(params->dwItem) : "0",
                      params->dwTrack, params->dwCallback);
            } else {
                switch (params->dwItem) {
                    case MCI_STATUS_MODE:
                        debug("  MCI_STATUS output : return(%s)",
                              parse_mci_mode(params->dwReturn));
                        break;
                    case MCI_STATUS_TIME_FORMAT:
                        debug("  MCI_STATUS output : return(%s)",
                              parse_mci_tmfmt(params->dwReturn));
                        break;
                    default:
                        debug("  MCI_STATUS output : return(%u)",
                              params->dwReturn);
                        break;
                }
            }
        }
        break;

        default: {
            if (!output && dwParam) {
                MCI_GENERIC_PARMS *params = (MCI_GENERIC_PARMS *)dwParam;
                debug("  MCI_GENERIC input : callback(0x%08x)",
                      params->dwCallback);
            }
        }
        break;
    }
}

MCIERROR WINAPI trace_mciSendCommandA(
    MCIDEVICEID IDDevice,
    UINT        uMsg,
    DWORD_PTR   fdwCommand,
    DWORD_PTR   dwParam)
{
    debug("TRACE mciSendCommandA(0x%08x, %s, %s, 0x%08x)",
          IDDevice, parse_mci_msg(uMsg), parse_mci_cmd(uMsg, fdwCommand),
          dwParam);
    trace_mci_param(uMsg, fdwCommand, dwParam, false);
    MCIERROR result = sys_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    trace_mci_param(uMsg, fdwCommand, dwParam, true);
    return result;
}

MCIERROR WINAPI trace_mciSendStringA(
    LPCSTR lpszCommand,
    LPSTR  lpszReturnString,
    UINT   cchReturn,
    HWND   hwndCallback)
{
    MCIERROR result = sys_mciSendStringA(lpszCommand, lpszReturnString,
                                        cchReturn, hwndCallback);
    debug("TRACE mciSendStringA(\"%s\", lpszReturnString, %u, 0x%08x): %s",
          lpszCommand, cchReturn, (DWORD)hwndCallback, lpszReturnString);
    return result;
}

static void hook(HMODULE hDLL)
{
    if (config.audpath.empty() && !config.cddrive) {
        return;
    }

    audplay.volume = config.audvol / (float)100.0;

    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_mciSendCommandA, hook_mciSendCommandA);
    DetourAttach(&(PVOID&)sys_mciSendStringA, hook_mciSendStringA);
    DetourTransactionCommit();
}

static void unhook()
{
    if (config.audpath.empty() && !config.cddrive) {
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_mciSendCommandA, hook_mciSendCommandA);
    DetourDetach(&(PVOID&)sys_mciSendStringA, hook_mciSendStringA);
    DetourTransactionCommit();
}

static void trace(HMODULE hDLL)
{
    DisableThreadLibraryCalls(hDLL);
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)sys_mciSendCommandA, trace_mciSendCommandA);
    DetourAttach(&(PVOID&)sys_mciSendStringA, trace_mciSendStringA);
    DetourTransactionCommit();
}

static void untrace()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)sys_mciSendCommandA, trace_mciSendCommandA);
    DetourDetach(&(PVOID&)sys_mciSendStringA, trace_mciSendStringA);
    DetourTransactionCommit();
}

void mci_hook(HMODULE hdll) {
    config.trace ? trace(hdll) : hook(hdll);
}

void mci_unhook() {
    config.trace ? untrace() : unhook();
}
