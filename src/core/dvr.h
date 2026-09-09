#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "msp_displayport.h"

typedef enum {
    DVR_TOGGLE,
    DVR_STOP,
    DVR_START,
} osd_dvr_cmd_t;

extern bool dvr_is_recording;

void dvr_update_status();
void dvr_select_audio_source(uint8_t audio_source);
void dvr_enable_line_out(bool enable);
void dvr_cmd(osd_dvr_cmd_t cmd);
// Finish a stop that was left running in the background. Call before taking
// the video away from under the record process.
void dvr_collect_stop(void);
void dvr_update_vi_conf(video_resolution_t fmt);
void dvr_toggle();
void dvr_star();

#ifdef __cplusplus
}
#endif
