#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <log/log.h>
#include <lvgl/lvgl.h>
#include <minIni.h>

#include "lang/language.h"

#ifdef EMULATOR_BUILD
#include "SDLaccess.h"
SDL_mutex *global_sdl_mutex;
#endif

#include "bmi270/accel_gyro.h"
#include "core/app_state.h"
#include "core/common.hh"
#include "core/elrs.h"
#include "core/ht.h"
#include "core/input_device.h"
#include "core/osd.h"
#include "core/self_test.h"
#include "core/settings.h"
#include "core/sleep_mode.h"
#include "core/thread.h"
#include "driver/TP2825.h"
#include "driver/beep.h"
#include "driver/dm5680.h"
#include "driver/esp32.h"
#include "driver/fans.h"
#include "driver/gpadc.h"
#include "driver/gpio.h"
#include "driver/hardware.h"
#include "driver/i2c.h"
#include "driver/it66021.h"
#include "driver/it66121.h"
#include "driver/mcp3021.h"
#include "driver/oled.h"
#include "driver/rtc.h"
#include "driver/rtc6715.h"
#include "ui/page_power.h"
#include "ui/page_scannow.h"
#include "ui/page_source.h"
#include "ui/page_storage.h"
#include "ui/ui_image_setting.h"
#include "ui/ui_main_menu.h"
#include "ui/ui_osd_element_pos.h"
#include "ui/ui_porting.h"
#include "ui/ui_statusbar.h"
#include "util/time.h"

int gif_cnt = 0;

static void *thread_autoscan(void *ptr) {
    for (;;) {
        pthread_mutex_lock(&lvgl_mutex);
        main_menu_show(true);
        app_state_push(APP_STATE_SUBMENU);
        submenu_enter();
        pthread_mutex_unlock(&lvgl_mutex);

        if (g_autoscan_exit)
            goto a_exit;

        sleep(5);

        if (g_autoscan_exit)
            goto a_exit;
    }

a_exit:
    pthread_exit(NULL);
    return NULL;
}

static int boot_source(void) {
    if (g_setting.autoscan.source == SETTING_AUTOSCAN_SOURCE_LAST)
        return g_setting.autoscan.last_source;

    return g_setting.autoscan.source;
}

// True when start_running() will go straight to HDZero video, which is the
// one path that needs the tuner up as early as it can be.
static bool boot_goes_to_hdzero_video(void) {
    return boot_source() == SETTING_AUTOSCAN_SOURCE_HDZERO &&
           g_setting.autoscan.status == SETTING_AUTOSCAN_STATUS_LAST;
}

void start_running(void) {
    int source = boot_source();

    if (source == SETTING_AUTOSCAN_SOURCE_HDZERO) { // HDZero
        g_source_info.source = SOURCE_HDZERO;
        // go autoscan only if no dial up/down during initialization
        if ((g_setting.autoscan.status == SETTING_AUTOSCAN_STATUS_ON) && (g_init_done == 0)) {
            pthread_t pid;
            g_autoscan_exit = false;
            pthread_create(&pid, NULL, thread_autoscan, NULL);
        } else if (g_setting.autoscan.status == SETTING_AUTOSCAN_STATUS_LAST) {
            app_state_push(APP_STATE_VIDEO);
            app_switch_to_hdzero(true);
        } else { // auto scan disabled, go to go directly to last saved channel
            app_state_push(APP_STATE_MAINMENU);
            main_menu_show(true); // the menu is the destination here
        }
    } else {
        app_state_push(APP_STATE_VIDEO);
        if (source == SETTING_AUTOSCAN_SOURCE_ANALOG) { // analog
            g_hw_stat.av_pal[1] = g_setting.source.analog_format;
            app_switch_to_analog();
            g_source_info.source = SOURCE_ANALOG;
        } else if (source == SETTING_AUTOSCAN_SOURCE_AV_IN) { // AV in
            g_hw_stat.av_pal[0] = g_setting.source.analog_format;
            app_switch_to_av_in();
            g_source_info.source = SOURCE_AV_IN;
        } else { // HDMI in
            sleep(2);
            // g_source_info.hdmi_in_status = IT66021_Sig_det();
            // if (g_source_info.hdmi_in_status) {
            app_switch_to_hdmi_in();
            g_source_info.source = SOURCE_HDMI_IN;
            //} else {
            //    g_source_info.source = SOURCE_HDZERO;
            //    app_state_push(APP_STATE_MAINMENU);
            //}
        }
    }

    if (g_setting.elrs.enable)
        enable_esp32();
}

// The motion sensor takes 686ms of I2C to come up and nothing between here
// and a picture needs it. The motion timer in ht.c would start reading it a
// second after ht_init() whether it was up or not, so that timer stays idle
// until ht_set_imu_ready() is called after the join below.
static void *imu_init_worker(void *arg) {
    (void)arg;

    enable_bmi270();

    return NULL;
}

static pthread_t imu_init_thread;
static bool imu_init_running = false;

static void device_init(void) {
    self_test();

    if (g_setting.speed.async_imu) {
        if (pthread_create(&imu_init_thread, NULL, imu_init_worker, NULL) == 0)
            imu_init_running = true;
        else
            enable_bmi270();
    } else {
        enable_bmi270();
    }
    IT66021_init();
    IT66121_init();
    TP2825_Config(0, 0);
    DM5680_req_ver();
    fans_top_setspeed(g_setting.fans.top_speed);
}

void lvgl_init() {
    lv_init();
    style_init();
    lvgl_init_porting();
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_color_make(0xff, 0xff, 0xff), lv_palette_main(LV_PALETTE_RED),
                                              false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(64, 64, 64), 0);
}

// lv_timer_handler() is where LVGL actually draws. Menu navigation is much
// heavier than video, and with the menu scaled to fit 720p every change
// inside it is rendered into a layer and then resampled, so log the passes
// that run long rather than guessing which part is slow.
static void ui_draw_timed(void) {
    uint32_t t0 = time_ms();

    lv_timer_handler();

    uint32_t dt = time_ms() - t0;
    if (dt >= 20)
        LOGI("ui: draw %ums", dt);
}

int main(int argc, char *argv[]) {
    // Anchored here so the total covers everything, device_init() included.
    // Started after that, it missed the very work Async Motion Sensor moves
    // and reported a 169ms change for what was really 641ms.
    uint32_t boot_start_ms = time_ms();

    pthread_mutex_init(&lvgl_mutex, NULL);

#ifdef EMULATOR_BUILD
    global_sdl_mutex = SDL_CreateMutex();
    if (global_sdl_mutex == NULL) {
        LOGE("Failed to create an SDL mutex!");
    }
#endif

    // 1. Recall configuration
    settings_init();
    settings_load();
    language_init();
    vclk_phase_init();
    pclk_phase_init();

    // 2. Initialize communications.
    rtc_init();
    iic_init();
    gpio_init();
    uart_init();

    // 3. Initialize core devices.
    mcp3021_init();
    input_device_init();
    hw_stat_init();
    device_init();

    LOGI("hw: GOGGLE_VER_2=%d revision=%d", GOGGLE_VER_2, (int)getHwRevision());

    // The tuner init is 1.1-1.8s and the UI phase below is about 1.1s. They
    // share the main I2C bus, which is why the per-port lock is a turnstile
    // and why the init's 1MHz applies to the display bring-up as well. After
    // device_init() so TP2825_Config() is done before that clock goes up, and
    // only when start-up is heading straight to HDZero video, since that is
    // the one path that needs the tuner before the threads run.
    if (g_setting.speed.async_tuner && boot_goes_to_hdzero_video())
        HDZero_open_async_start(g_setting.source.hdzero_bw);

    // dispw is 1.1s and, with the tuner out of the way, it is the only thing
    // the switch still waits for. Started here it runs out during the UI
    // build instead of after it. Skip Display Setup is required or
    // Display_UI_init() below runs a second dispw for the menu's own timing
    // and the switch a third; Skip Boot Menu is required because the panel
    // then stays dark until the video arrives rather than being lit halfway
    // through the reconfiguration.
    if (g_setting.speed.boot_display_early && boot_goes_to_hdzero_video()) {
        if (g_setting.speed.async_display && g_setting.speed.boot_display &&
            g_setting.speed.skip_boot_menu)
            start_display_timing_early();
        else
            LOGW("vdpo: Early Video Timing needs Async Display Setup, "
                 "Skip Display Setup and Skip Boot Menu; not starting early");
    }

    esp32_init();
    elrs_init();
    ht_init();
    beep_init();
    gpadc_init();

    // Reading the OSD fonts is file I/O and nothing below needs them, so let
    // it run alongside the display and tuner bring-up rather than after it.
    osd_font_prefetch_start();

    // 4. Initilize UI
    uint32_t phase_ms = time_ms();
    uint32_t step_ms = phase_ms;
    lvgl_init();
    LOGI("boot phase: lvgl %ums", time_ms() - step_ms);

    step_ms = time_ms();
    main_menu_init();
    LOGI("boot phase: menu pages %ums", time_ms() - step_ms);

    step_ms = time_ms();
    statusbar_init();
    lv_timer_handler();
    LOGI("boot phase: statusbar and first draw %ums", time_ms() - step_ms);
    LOGI("boot phase: ui %ums", time_ms() - phase_ms);

    // 5. Prepare Display
    phase_ms = time_ms();
    OLED_Startup();
    Display_UI_init();
    OLED_Pattern(0, 0, 0);
    osd_init();
    ims_init();
    ui_osd_element_pos_init();
    LOGI("boot phase: display and osd %ums", time_ms() - phase_ms);

    // 6. Enable functionality
    phase_ms = time_ms();
    if (g_setting.ht.enable) {
        ht_enable();
    } else {
        ht_disable();
    }
    LOGI("boot phase: head tracker %ums", time_ms() - phase_ms);

    // 7 set initial analog module power state
    Analog_Module_Power(1, 0); // must before start_running()

    // The motion timer armed in ht_init() has been firing since a second after
    // that call, but it does nothing until told the sensor is up. Wait for the
    // async init if there was one, then open the gate. The gate is opened on
    // every path, async or not: without it the head tracker never reads.
    if (imu_init_running) {
        step_ms = time_ms();
        pthread_join(imu_init_thread, NULL);
        imu_init_running = false;
        LOGI("boot phase: waited %ums for the motion sensor", time_ms() - step_ms);
    }
    ht_set_imu_ready();

    // 8. Start threads
    start_running();

    // Every path to a picture collects the background timing change on the
    // way, but the panel stays dark until something does, so make sure. Costs
    // nothing when there is nothing outstanding.
    vdpo_timing_collect();

    // One line to compare boots by, since the phases alone have proved
    // misleading: they said deferring the menu was faster while the run as a
    // whole was slower, the difference sitting in things that vary by
    // hundreds of milliseconds on their own.
    LOGI("boot total: app start to video %ums", time_ms() - boot_start_ms);

    create_threads();

    // 9. Synthetic counter for gif refresh
    gif_cnt = 0;

    // Head alarm
    head_alarm_init();

    // 10. Execute main loop
    g_init_done = 1;
    for (;;) {
        // The status bar and the source status page rewrite their labels on
        // every pass, and a rewrite invalidates them whether the text changed
        // or not, so at one pass per 5ms they were being redrawn around 200
        // times a second. Nothing on them changes that fast, and the redraws
        // compete for lvgl_mutex with the dial and button handlers.
        bool slow_tick = true;

        if (g_setting.speed.ui_throttle) {
            static uint32_t last_slow_ms = 0;
            uint32_t now = time_ms();

            slow_tick = (uint32_t)(now - last_slow_ms) >= 50;
            if (slow_tick)
                last_slow_ms = now;
        }

        // The dial and button handlers take lvgl_mutex too, so held across the
        // whole batch it makes them wait for a full pass. Released between the
        // stages they get several chances per pass instead. Each stage still
        // runs under the lock, so LVGL is never touched unprotected; what
        // changes is that input can land between two stages rather than only
        // between passes.
        bool split = g_setting.speed.split_lock;

#define UI_STAGE(call)                         \
    do {                                       \
        if (split)                             \
            pthread_mutex_lock(&lvgl_mutex);   \
        call;                                  \
        if (split)                             \
            pthread_mutex_unlock(&lvgl_mutex); \
    } while (0)

        if (!split)
            pthread_mutex_lock(&lvgl_mutex);

        UI_STAGE(main_menu_update());
        UI_STAGE(sleep_reminder());
        if (slow_tick)
            UI_STAGE(statubar_update());
        UI_STAGE(osd_hdzero_update());
        UI_STAGE(draw_hdmi_in_dvr_osd());
        UI_STAGE(ims_update());
        UI_STAGE(ui_osd_element_pos_update());
        UI_STAGE(ht_detect_motion());
        UI_STAGE(ui_draw_timed());
        if (slow_tick)
            UI_STAGE(source_status_timer());

        if (!split)
            pthread_mutex_unlock(&lvgl_mutex);

#undef UI_STAGE

        usleep(5000);
        gif_cnt++;
    }
    return 0;
}
