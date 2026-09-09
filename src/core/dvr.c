#include "dvr.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <log/log.h>
#include <minIni.h>

#include "core/msp_displayport.h"
#include "core/settings.h"
#include "driver/hardware.h"
#include "record/record_definitions.h"
#include "ui/page_common.h"
#include "util/sdcard.h"
#include "util/system.h"
#include "util/time.h"

bool dvr_is_recording = false;

static time_t dvr_recording_start = 0;
static pthread_mutex_t dvr_mutex;

///////////////////////////////////////////////////////////////////
// The record process's own status, as it writes it to REC_dataFILE:
//-1=error;
// 0=idle,1=recording,2=stopped,3=No SD card,4=recorf file path error,
// 5=SD card Full,6=Encoder error
#define DVR_STATUS_RECORDING 1

// The flat two seconds the waits below replace. Kept as the cap, so a record
// process that never answers costs exactly what it used to, no more.
#define DVR_WAIT_MS 2000
#define DVR_POLL_MS 20

static int dvr_read_status(void) {
    int status = -1;
    FILE *fp = fopen(REC_dataFILE, "r");

    if (!fp)
        return -1;

    if (fscanf(fp, "%d", &status) != 1)
        status = -1;

    fclose(fp);

    return status;
}

void dvr_update_status() {
    pthread_mutex_lock(&dvr_mutex);
    if (dvr_is_recording) {
        if (dvr_read_status() != DVR_STATUS_RECORDING) {
            dvr_is_recording = false;
            system_script(REC_STOP);
            sleep(2); // wait for record process
        }
    }
    pthread_mutex_unlock(&dvr_mutex);
}

void dvr_enable_line_out(bool enable) {
    // audio_sel.sh forks amixer once per mixer control: out_off measured
    // ~100ms and out_on three times that. The menu/video switch calls this
    // every time with the state it already has, so only touch the mixer on a
    // real change. Nothing outside these two functions drives audio_sel.sh.
    static int last_enable = -1;
    char buf[128];

    if (g_setting.speed.skip_audio && last_enable == (int)enable)
        return;
    last_enable = enable;

    if (enable) {
        snprintf(buf, sizeof(buf), "%s out_on", AUDIO_SEL_SH);
        system_exec(buf);
        snprintf(buf, sizeof(buf), "%s out_linein_on", AUDIO_SEL_SH);
        system_exec(buf);
        snprintf(buf, sizeof(buf), "%s out_dac_off", AUDIO_SEL_SH);
        system_exec(buf);
    } else {
        snprintf(buf, sizeof(buf), "%s out_off", AUDIO_SEL_SH);
        system_exec(buf);
    }
}

void dvr_select_audio_source(uint8_t source) {
    char buf[128];
    char *audio_source[3] = {
        "in_mic1",
        "in_mic2",
        "in_linein"};

    if (source > 2)
        source = 2;

    // in_mic2 and friends clear all eight input switches before setting two,
    // twelve amixer processes in all, measured at ~365ms. The source does not
    // change from one switch to the next.
    static int last_source = -1;
    if (g_setting.speed.skip_audio && last_source == (int)source)
        return;
    last_source = source;

    snprintf(buf, sizeof(buf), "%s %s", AUDIO_SEL_SH, audio_source[source]);
    system_exec(buf);
}

void dvr_update_vi_conf(video_resolution_t fmt) {
    pthread_mutex_lock(&dvr_mutex);
    switch (fmt) {
    case VR_720P50:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 50, REC_CONF);
        break;
    case VR_720P60:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 60, REC_CONF);
        break;
    case VR_720P30:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 30, REC_CONF);
        break;
    case VR_540P90:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 90, REC_CONF);
        break;
    case VR_540P60:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 60, REC_CONF);
        break;
    case VR_960x720P60:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 60, REC_CONF);
        break;
    case VR_540P90_CROP:
        ini_putl("vi", "width", 1280, REC_CONF);
        ini_putl("vi", "height", 720, REC_CONF);
        ini_putl("vi", "fps", 90, REC_CONF);
        break;
    case VR_1080P30:
        ini_putl("vi", "width", 1920, REC_CONF);
        ini_putl("vi", "height", 1080, REC_CONF);
        ini_putl("vi", "fps", 30, REC_CONF);
        break;
    case VR_1080P24:
        ini_putl("vi", "width", 1920, REC_CONF);
        ini_putl("vi", "height", 1080, REC_CONF);
        ini_putl("vi", "fps", 50, REC_CONF);
        break;
    case VR_1080P50:
        ini_putl("vi", "width", 1920, REC_CONF);
        ini_putl("vi", "height", 1080, REC_CONF);
        ini_putl("vi", "fps", 50, REC_CONF);
        break;
    case VR_1080P60:
        ini_putl("vi", "width", 1920, REC_CONF);
        ini_putl("vi", "height", 1080, REC_CONF);
        ini_putl("vi", "fps", 59, REC_CONF); // If set fps to 60, DVR is wrong. I don't why. 59 or 61 is ok.
        break;
    }
    pthread_mutex_unlock(&dvr_mutex);

    LOGI("update_record_vi_conf: fmt=%d", fmt);
}

void dvr_toggle() {
    dvr_cmd(DVR_TOGGLE);
}

void dvr_star() {
    pthread_mutex_lock(&dvr_mutex);
    if (dvr_is_recording) {
        char current_dvr_file[256] = "";
        FILE *now_recording_file = fopen(NOW_RECORDING_FILE, "r");
        if (now_recording_file) {
            const size_t read_count = fread(current_dvr_file, 1, sizeof(current_dvr_file) - 1, now_recording_file);
            if (ferror(now_recording_file) == 0) {
                current_dvr_file[read_count] = '\0';
                strcat(current_dvr_file, REC_starSUFFIX);
                FILE *like_file = fopen(current_dvr_file, "a");
                if (like_file) {
                    unsigned recording_duration_s = time(NULL) - dvr_recording_start;
                    unsigned minutes = recording_duration_s / 60;
                    unsigned seconds = recording_duration_s % 60;
                    fprintf(like_file, REC_starFORMAT, minutes, seconds);
                    fclose(like_file);
                }
            }
            fclose(now_recording_file);
        }
    }
    pthread_mutex_unlock(&dvr_mutex);
}

static void dvr_update_record_conf() {
    if (g_setting.record.format_ts)
        ini_puts("record", "type", "ts", REC_CONF);
    else
        ini_puts("record", "type", "mp4", REC_CONF);

    if (g_source_info.source == SOURCE_HDZERO) {
        LOGI("CAM_MODE=%d", CAM_MODE);
        if (CAM_MODE == VR_1080P30 || CAM_MODE == VR_1080P24) {
            ini_putl("venc", "width", 1920, REC_CONF);
            ini_putl("venc", "height", 1080, REC_CONF);
        } else {
            ini_putl("venc", "width", 1280, REC_CONF);
            ini_putl("venc", "height", 720, REC_CONF);
        }

        if (CAM_MODE == VR_1080P30) { // 1080p30
            ini_putl("venc", "fps", 60, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
        } else if (CAM_MODE == VR_1080P24) {
            ini_putl("venc", "fps", 50, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
        } else if (CAM_MODE == VR_540P90 || CAM_MODE == VR_540P90_CROP) { // 90fps
            ini_putl("venc", "fps", 90, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
        } else {
            ini_putl("venc", "fps", 60, REC_CONF);
            ini_putl("venc", "kbps", 24000, REC_CONF);
            ini_putl("venc", "h265", 1, REC_CONF);
        }
    } else if (g_source_info.source == SOURCE_AV_IN || g_source_info.source == SOURCE_ANALOG) { // Analog
        ini_putl("venc", "width", 1280, REC_CONF);
        ini_putl("venc", "height", 720, REC_CONF);

        ini_putl("venc", "kbps", 24000, REC_CONF);
        ini_putl("venc", "h265", 1, REC_CONF);
        if (g_hw_stat.av_pal[g_hw_stat.av_chid])
            ini_putl("venc", "fps", 50, REC_CONF);
        else
            ini_putl("venc", "fps", 60, REC_CONF);
    } else if (g_source_info.source == SOURCE_HDMI_IN) {
        LOGI("g_hw_stat.hdmiin_vtmg=%d", g_hw_stat.hdmiin_vtmg);
        switch (g_hw_stat.hdmiin_vtmg) {
        case HDMIIN_VTMG_1080P60:
            ini_putl("venc", "width", 1920, REC_CONF);
            ini_putl("venc", "height", 1080, REC_CONF);
            ini_putl("venc", "fps", 60, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        case HDMIIN_VTMG_1080P50:
            ini_putl("venc", "width", 1920, REC_CONF);
            ini_putl("venc", "height", 1080, REC_CONF);
            ini_putl("venc", "fps", 50, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        case HDMIIN_VTMG_1080Pother:
            ini_putl("venc", "width", 1920, REC_CONF);
            ini_putl("venc", "height", 1080, REC_CONF);
            ini_putl("venc", "fps", 50, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        case HDMIIN_VTMG_720P50:
            ini_putl("venc", "width", 1280, REC_CONF);
            ini_putl("venc", "height", 720, REC_CONF);
            ini_putl("venc", "fps", 50, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        case HDMIIN_VTMG_720P60:
            ini_putl("venc", "width", 1280, REC_CONF);
            ini_putl("venc", "height", 720, REC_CONF);
            ini_putl("venc", "fps", 60, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        case HDMIIN_VTMG_720P100:
            ini_putl("venc", "width", 1280, REC_CONF);
            ini_putl("venc", "height", 720, REC_CONF);
            ini_putl("venc", "fps", 90, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        default:
            ini_putl("venc", "width", 1280, REC_CONF);
            ini_putl("venc", "height", 720, REC_CONF);
            ini_putl("venc", "fps", 60, REC_CONF);
            ini_putl("venc", "kbps", 34000, REC_CONF);
            ini_putl("venc", "h265", 0, REC_CONF);
            break;
        }
    }

    ini_putl("record", "audio", g_setting.record.audio, REC_CONF);
    dvr_select_audio_source(g_setting.record.audio_source);
    ini_putl("record", "naming", g_setting.record.naming, REC_CONF);
}

// gogglecmd only asks the record process to do something; it has not done it
// when the command returns, which is what the sleeps were covering. But the
// record process writes its status to REC_dataFILE either side of the work --
// REC_statusRun once the encoder is going and the file is open, the stop
// status after ffpack_close() -- so the file says when it is actually done.
//
// Anything other than the state asked for ends the wait, an unreadable file
// included: none of those can mean the recording is in the state we are
// waiting to leave.
static void dvr_poll_status(bool want_recording, const char *what) {
    uint32_t t0 = time_ms();

    while (time_ms() - t0 < DVR_WAIT_MS) {
        if ((dvr_read_status() == DVR_STATUS_RECORDING) == want_recording)
            break;

        usleep(DVR_POLL_MS * 1000);
    }

    LOGI("dvr: record %s took %ums", what, time_ms() - t0);
}

// Left true when a stop was issued and not waited for. The next start is the
// one thing that has to see the old file closed, so it collects it.
static bool dvr_stop_outstanding = false;

static void dvr_wait_stopped(void) {
    if (!g_setting.speed.dvr_stop_wait) {
        sleep(2); // wait for record process
        return;
    }

    dvr_poll_status(false, "stop");
}

// was_running is the status from before the start command went out, and it
// should be anything but "recording". If it already said "recording" the file
// is stale -- an earlier run that never wrote its stop -- and polling for a
// value that is already there would return at once, so the flat wait stands.
static void dvr_wait_started(bool was_running) {
    if (!g_setting.speed.dvr_start_wait || was_running) {
        sleep(2); // wait for record process
        return;
    }

    dvr_poll_status(true, "start");
}

void dvr_cmd(osd_dvr_cmd_t cmd) {
    LOGI("dvr_cmd: sdcard=%d, recording=%d, cmd=%d", g_sdcard_enable, dvr_is_recording, cmd);

    if (!g_sdcard_enable)
        return;

    pthread_mutex_lock(&dvr_mutex);

    bool start_rec = dvr_is_recording;

    switch (cmd) {
    case DVR_TOGGLE:
        start_rec = !dvr_is_recording;
        break;
    case DVR_STOP:
        start_rec = false;
        break;
    case DVR_START:
        start_rec = true;
        break;
    }

    if (start_rec) {
        if (!dvr_is_recording && !sdcard_is_full()) {
            // A stop that was left to finish has to be finished now: this is
            // the only thing between here and there that needed the old file
            // closed. Polled rather than slept whatever the stop switch says,
            // since deferring the stop is what asked for this.
            if (dvr_stop_outstanding) {
                dvr_poll_status(false, "stop collected");
                dvr_stop_outstanding = false;
            }

            // Read before the command goes out, so the wait can tell a fresh
            // "recording" from one left behind by an earlier run. Not read at
            // all with the poll off, so that path is the original one exactly.
            bool was_running = g_setting.speed.dvr_start_wait &&
                               dvr_read_status() == DVR_STATUS_RECORDING;

            dvr_update_record_conf();
            dvr_is_recording = true;
            usleep(100 * 1000);
            system_script(REC_START);
            dvr_recording_start = time(NULL);
            // Held under dvr_mutex, which dvr_update_vi_conf() on the switch
            // path also wants, so this blocks the menu switch as well as the
            // thread it runs on.
            dvr_wait_started(was_running);
        }
    } else {
        if (dvr_is_recording) {
            dvr_is_recording = false;
            system_script(REC_STOP);

            if (g_setting.speed.dvr_defer_stop) {
                // Measured on the goggles: the record process will not
                // finalise a recording until about three seconds after it
                // started, so stopping sooner waits out the remainder. That is
                // the two seconds on a channel change, and on going to the
                // menu straight after the picture arrives.
                //
                // Nothing between here and the next recording needs the old
                // file closed -- the channel change, the display timing and
                // the vi conf write are all its own -- so the wait moves to
                // the start, which does need it.
                dvr_stop_outstanding = true;
                LOGI("dvr: record stop left to finish in the background");
            } else {
                // On the menu switch this runs with lvgl_mutex held, so
                // whatever it costs is time the menu is not drawn.
                dvr_wait_stopped();
            }
        }
    }

    pthread_mutex_unlock(&dvr_mutex);
}