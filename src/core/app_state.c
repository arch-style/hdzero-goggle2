#include "app_state.h"

#include <log/log.h>
#include <minIni.h>
#include <stdlib.h>
#include <unistd.h>

#include "core/dvr.h"
#include "core/input_device.h"
#include "core/msp_displayport.h"
#include "core/osd.h"
#include "core/settings.h"
#include "driver/dm5680.h"
#include "driver/dm6302.h"
#include "driver/hardware.h"
#include "driver/it66121.h"
#include "driver/rtc6715.h"
#include "ui/page_common.h"
#include "ui/page_imagesettings.h"
#include "ui/ui_image_setting.h"
#include "ui/ui_main_menu.h"
#include "ui/ui_porting.h"
#include "util/system.h"

app_state_t g_app_state = APP_STATE_MAINMENU;

extern int valid_channel_tb[10];
extern int user_select_index;

void app_state_push(app_state_t state) {
    g_app_state = state;
}

void app_switch_to_menu() {
    if (g_app_state == APP_STATE_IMS) {
        ims_save();
        set_slider_value();
    }

    app_state_push(APP_STATE_MAINMENU);
    LOGI("switch mark: to_menu start");

    // Stop recording if switching to menu mode from video mode regardless
    dvr_cmd(DVR_STOP);
    dvr_update_vi_conf(VR_1080P30);
    LOGI("switch mark: dvr stopped");

    // Switching the display source to UI on its own is what forces the 1080p50
    // rebuild, and leaving the panel on the video timing while doing it tore
    // the picture into stripes. So with keep_display do not switch the source
    // at all: leave the pipeline composing video with the UI layer over it,
    // exactly as it does for the OSD, and let the menu draw into that layer.
    // The menu is laid out for 1080p, so at 720p it is cropped.
    bool overlay = g_setting.speed.keep_display && vdpo_timing_applied();

    if (!overlay) {
        Display_UI();
        lvgl_switch_to_1080p();
    }
    LOGI("switch mark: display to UI");
    exit_tune_channel();
    osd_show(false);
    g_bShowIMS = false;
    main_menu_show(true);
    LOGI("switch mark: menu shown");
    // Overlaying the menu only means anything if the picture underneath keeps
    // running, so leave the tuner alone entirely in that case. Otherwise:
    // resetting the tuner here is what makes coming back cost a full
    // DM6302_init(), measured at 2.3s. Standby skips that at the price of
    // leaving it powered.
    if (overlay)
        ; // video keeps playing under the menu
    else if (g_setting.speed.fast_menu)
        HDZero_Standby();
    else
        HDZero_Close();
    LOGI("switch mark: rf off");
    g_sdcard_det_req = 1;
    if (g_source_info.source == SOURCE_HDMI_IN) // HDMI
        IT66121_init();

    if (!overlay) // the picture underneath has to keep its source powered
        Analog_Module_Power(0, 0);

    dvr_enable_line_out(false);
    LOGI("switch mark: sources off");

    system_script(REC_STOP_LIVE);
    LOGI("switch mark: to_menu done");
}

void app_exit_menu() {
    switch (g_source_info.source) {
    case SOURCE_HDZERO:
        progress_bar.start = 1;
        app_switch_to_hdzero(true);
        break;
    case SOURCE_HDMI_IN:
        app_switch_to_hdmi_in();
        break;
    case SOURCE_AV_IN:
        app_switch_to_av_in();
        break;
    case SOURCE_ANALOG:
        app_switch_to_analog();
        break;
    }
}

void app_switch_to_analog() {
    system_exec("aww 0x0300b084 0x00001555"); // Set vdpo clock driver strength to level 2. Refer datasheet 12.7.5.11
    Analog_Module_Power(0, 1);

    if (GOGGLE_VER_2) {
        if (g_setting.source.analog_module == SETTING_SOURCES_ANALOG_MODULE_INTERNAL) {
            RTC6715_Open(1, g_setting.record.audio_source == SETTING_RECORD_AUDIO_SOURCE_AV_IN);
            RTC6715_SetCH(g_setting.source.analog_channel - 1);
        } else {
            RTC6715_Open(0, 0);
        }
    }

    Source_AV(1);

    dvr_update_vi_conf(VR_720P50);
    osd_fhd(0);
    osd_show(true);
    lvgl_switch_to_720p();
    osd_clear();
    lv_timer_handler();
    Display_Osd(g_setting.record.osd);

    g_setting.autoscan.last_source = SETTING_AUTOSCAN_SOURCE_ANALOG;
    ini_putl("autoscan", "last_source", g_setting.autoscan.last_source, SETTING_INI);

    // audio in&out
    dvr_select_audio_source(g_setting.record.audio_source);
    dvr_enable_line_out(true);

    // usleep(300*1000);
    sleep(1);
    system_script(REC_STOP_LIVE);
}
void app_switch_to_av_in() {
    system_exec("aww 0x0300b084 0x00001555"); // Set vdpo clock driver strength to level 2. Refer datasheet 12.7.5.11
    Analog_Module_Power(0, 0);

    Source_AV(0);

    dvr_update_vi_conf(VR_720P50);
    osd_fhd(0);
    osd_show(true);
    lvgl_switch_to_720p();
    osd_clear();
    lv_timer_handler();
    Display_Osd(g_setting.record.osd);

    g_setting.autoscan.last_source = SETTING_AUTOSCAN_SOURCE_AV_IN;
    ini_putl("autoscan", "last_source", g_setting.autoscan.last_source, SETTING_INI);

    dvr_enable_line_out(true);
    // usleep(300*1000);
    sleep(1);
    system_script(REC_STOP_LIVE);
}

void app_switch_to_hdmi_in() {
    system_exec("aww 0x0300b084 0x00001555"); // Set vdpo clock driver strength to level 2. Refer datasheet 12.7.5.11
    Analog_Module_Power(0, 0);

    Source_HDMI_in();
    IT66121_close();
    sleep(2);

    if (g_hw_stat.hdmiin_vtmg == HDMIIN_VTMG_1080P60 ||
        g_hw_stat.hdmiin_vtmg == HDMIIN_VTMG_1080P50 ||
        g_hw_stat.hdmiin_vtmg == HDMIIN_VTMG_1080Pother)
        lvgl_switch_to_1080p();
    else
        lvgl_switch_to_720p();

    osd_show(true);
    osd_clear();
    lv_timer_handler();

    // update vi conf at HDMI_in_detect()
    // dvr_update_vi_conf((g_hw_stat.hdmiin_vtmg == 1) ? VR_1080P30 : VR_720P60);

    app_state_push(APP_STATE_VIDEO);
    g_source_info.source = SOURCE_HDMI_IN;
    g_setting.autoscan.last_source = SETTING_AUTOSCAN_SOURCE_HDMI_IN;
    ini_putl("autoscan", "last_source", g_setting.autoscan.last_source, SETTING_INI);

    // audio out
    dvr_enable_line_out(false);

    system_script(REC_STOP_LIVE);
}

//////////////////////////////////////////////////////////////////////
// is_default:
//    true = load from g_settings
//    false = user selected from auto scan page
// Which display timing the camera mode below will ask for. Mirrors the switch
// further down, so the two have to stay in step; getting it wrong only costs
// the head start, since vdpo_set_timing() runs the right one either way.
void start_display_timing_early(void) {
    switch (CAM_MODE) {
    case VR_720P50:
    case VR_720P60:
    case VR_960x720P60:
    case VR_540P60:
        vdpo_start_timing_async(VDPO_TMG_720P60, "720p60");
        break;

    case VR_540P90:
    case VR_540P90_CROP:
        vdpo_start_timing_async(VDPO_TMG_720P90, "720p90");
        break;

    case VR_1080P30:
    case VR_1080P24:
        vdpo_start_timing_async(VDPO_TMG_1080P60, "1080p60");
        break;

    default:
        break;
    }
}

// The part of the HDZero switch that is about recording and audio rather than
// the picture: independent of the display timing, so it can run either side
// of the wait for it.
static void hdzero_recording_side(void) {
    g_setting.autoscan.last_source = SETTING_AUTOSCAN_SOURCE_HDZERO;
    ini_putl("autoscan", "last_source", g_setting.autoscan.last_source, SETTING_INI);

    dvr_select_audio_source(g_setting.record.audio_source);
    dvr_enable_line_out(false);

    dvr_update_vi_conf(CAM_MODE);
    LOGI("switch mark: dvr configured");
    system_script(REC_STOP_LIVE);
}

void app_switch_to_hdzero(bool is_default) {
    int ch;
    LOGI("switch mark: to_hdzero start");

    system_exec("aww 0x0300b084 0x00001555"); // Set vdpo clock driver strength to level 2. Refer datasheet 12.7.5.11

    // After the clock drive strength, which used to run before dispw and now
    // has no reason not to, and before the tuner, so the two run together
    // instead of one after the other. Display_720P60_50() and friends collect
    // it where they would otherwise have started it.
    if (g_setting.speed.async_display)
        start_display_timing_early();
    Analog_Module_Power(0, 0);
    LOGI("switch mark: aww + analog power");

    if (is_default) {
        ch = g_setting.scan.channel - 1;
    } else {
        ch = valid_channel_tb[user_select_index];
        g_setting.scan.channel = ch + 1;
        ini_putl("scan", "channel", g_setting.scan.channel, SETTING_INI);
    }

    HDZero_open(g_setting.source.hdzero_bw);
    LOGI("switch mark: rf open");
    ch &= 0x7f;

    LOGI("switch to bw:%d, band:%d, ch:%d, CAM_MODE=%d 4:3=%d", g_setting.source.hdzero_bw, g_setting.source.hdzero_band, g_setting.scan.channel, CAM_MODE, cam_4_3);
    DM6302_SetChannel(g_setting.source.hdzero_band, ch);
    DM5680_clear_vldflg();
    DM5680_req_vldflg();
    progress_bar.start = 0;
    LOGI("switch mark: channel tuned");

    // The audio and recorder set-up below is half a second of forked
    // scripts that has nothing to do with the display, so it is free as long
    // as dispw is still running and the switch would be waiting anyway. The
    // test has to be liveness, not vdpo_timing_pending(): that stays true
    // until the join, and doing this after dispw had already exited put the
    // whole half second in front of the picture instead of beside it.
    bool side_done = false;

    if (g_setting.speed.async_display && vdpo_timing_running()) {
        LOGI("switch mark: audio and dvr while the display changes");
        hdzero_recording_side();
        side_done = true;
    }

    switch (CAM_MODE) {
    case VR_720P50:
    case VR_720P60:
    case VR_960x720P60:
    case VR_540P60:
        Display_720P60_50(CAM_MODE, cam_4_3);
        break;

    case VR_540P90:
    case VR_540P90_CROP:
        Display_720P90(CAM_MODE);
        break;

    case VR_1080P30:
        Display_1080P30(CAM_MODE);
        break;

    case VR_1080P24:
        Display_1080P24(CAM_MODE);
        break;

    default:
        perror("switch_to_video CaM_MODE error");
        break;
    }

    // Every case above collects the background timing change, except the one
    // that recognises no camera mode at all. Nothing else would, and until
    // something does the panel stays dark, so close that off here. A no-op
    // on every normal path.
    vdpo_timing_collect();

    channel_osd_mode = CHANNEL_SHOWTIME;
    LOGI("switch mark: display mode set");

    if (CAM_MODE == VR_1080P30 || CAM_MODE == VR_1080P24)
        lvgl_switch_to_1080p();
    else
        lvgl_switch_to_720p();
    osd_fhd(CAM_MODE == VR_1080P30 || CAM_MODE == VR_1080P24);
    osd_clear();
    osd_show(true);
    lv_timer_handler();
    Display_Osd(g_setting.record.osd);
    LOGI("switch mark: lvgl + osd");

    if (!side_done)
        hdzero_recording_side();
    LOGI("switch mark: to_hdzero done");
}