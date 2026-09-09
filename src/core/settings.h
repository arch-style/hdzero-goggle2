#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t top_speed;
    bool auto_mode;
    uint8_t left_speed;
    uint8_t right_speed;
} setting_fan_t;

typedef struct {
    int channel;
} setting_scan_t;

#define FAVORITES_MAX 8

typedef struct {
    bool enable;
    // How many of the slots below are in use, 1..FAVORITES_MAX.
    uint8_t count;
    // 0 marks an empty slot, otherwise a 1-based channel index. What the index
    // means depends on which list this is: a channel within the currently
    // selected band for HDZero, one of the 48 analog channels for analog.
    uint8_t channel[FAVORITES_MAX];
} setting_favorites_list_t;

#define FAVORITES_INI_HDZERO "favorites"
#define FAVORITES_INI_ANALOG "favorites_analog"

typedef struct {
    // Kept apart because the same index means different channels on each
    // source: 4 is R4 on HDZero and A4 on analog.
    setting_favorites_list_t hdzero;
    setting_favorites_list_t analog;
} setting_favorites_t;

typedef enum {
    SETTING_AUTOSCAN_STATUS_ON = 0,
    SETTING_AUTOSCAN_STATUS_LAST = 1,
    SETTING_AUTOSCAN_STATUS_OFF = 2
} setting_autoscan_status_t;

typedef enum {
    SETTING_AUTOSCAN_SOURCE_LAST = 0,
    SETTING_AUTOSCAN_SOURCE_HDZERO = 1,
    SETTING_AUTOSCAN_SOURCE_ANALOG = 2,
    SETTING_AUTOSCAN_SOURCE_AV_IN = 3,
    SETTING_AUTOSCAN_SOURCE_HDMI_IN = 4
} setting_autoscan_source_t;

typedef struct {
    setting_autoscan_status_t status;
    setting_autoscan_source_t last_source;
    setting_autoscan_source_t source;
} setting_autoscan_t;

typedef enum {
    SETTING_POWER_CELL_COUNT_MODE_AUTO = 0,
    SETTING_POWER_CELL_COUNT_MODE_MANUAL = 1
} setting_power_cell_count_mode_t;

typedef enum {
    SETTING_POWER_OSD_DISPLAY_MODE_TOTAL = 0,
    SETTING_POWER_OSD_DISPLAY_MODE_CELL = 1
} setting_power_osd_display_mode_t;

typedef enum {
    SETTING_POWER_WARNING_TYPE_BEEP = 0,
    SETTING_POWER_WARNING_TYPE_VISUAL = 1,
    SETTING_POWER_WARNING_TYPE_BOTH = 2
} setting_power_warning_type_t;

typedef enum {
    SETTING_HT_ALARM_STATE_OFF = 0,
    SETTING_HT_ALARM_STATE_VIDEO = 1,
    SETTING_HT_ALARM_STATE_ARM = 2,
} setting_ht_alarm_state_t;

typedef enum {
    SETTING_HT_ALARM_PATTERN_1SHORT = 0,
    SETTING_HT_ALARM_PATTERN_2SHORT = 1,
    SETTING_HT_ALARM_PATTERN_1LONG = 2
} setting_ht_alarm_pattern_t;

typedef struct {
    int voltage;
    bool display_voltage;
    setting_power_warning_type_t warning_type;
    setting_power_cell_count_mode_t cell_count_mode;
    int cell_count;
    setting_power_osd_display_mode_t osd_display_mode;
    bool power_ana;
    int calibration_offset;
} setting_power_t;

typedef enum {
    SETTING_RECORD_AUDIO_SOURCE_MIC = 0,
    SETTING_RECORD_AUDIO_SOURCE_LINE_IN = 1,
    SETTING_RECORD_AUDIO_SOURCE_AV_IN = 2
} setting_record_audio_source_t;

typedef enum {
    SETTING_NAMING_CONTIGUOUS,
    SETTING_NAMING_DATE
} setting_record_naming_t;

typedef struct {
    bool mode_manual;
    bool format_ts;
    bool osd;
    bool audio;
    setting_record_audio_source_t audio_source;
    setting_record_naming_t naming;
} setting_record_t;

typedef struct {
    uint8_t oled;
    uint8_t brightness;
    uint8_t saturation;
    uint8_t contrast;
    uint8_t auto_off; // 0=1min,1=3min,2=4min,3=5min,4=never,
} setting_image_t;

typedef struct {
    bool enable;
    int max_angle;
    int32_t acc_x;
    int32_t acc_y;
    int32_t acc_z;
    int32_t gyr_x;
    int32_t gyr_y;
    int32_t gyr_z;
    setting_ht_alarm_state_t alarm_state;
    int alarm_angle;
    uint16_t alarm_delay;
    setting_ht_alarm_pattern_t alarm_pattern;
    bool alarm_on_arm;
    bool alarm_on_video;
} setting_head_tracker_t;

typedef struct {
    bool enable;
} setting_elrs_t;

typedef enum {
    EMBEDDED_4x3,
    EMBEDDED_16x9
} setting_osd_embedded_mode_t;

typedef enum {
    SETTING_OSD_SHOW_AT_STARTUP_SHOW,
    SETTING_OSD_SHOW_AT_STARTUP_HIDE,
    SETTING_OSD_SHOW_AT_STARTUP_LAST
} setting_osd_show_at_startup_t;

typedef struct {
    int x;
    int y;
} setting_osd_goggle_element_position_t;

typedef struct {
    setting_osd_goggle_element_position_t mode_4_3;
    setting_osd_goggle_element_position_t mode_16_9;
} setting_osd_goggle_element_positions_t;

typedef struct {
    bool show;
    setting_osd_goggle_element_positions_t position;
} setting_osd_goggle_element_t;

typedef enum {
    OSD_GOGGLE_TOPFAN_SPEED = 0,
    OSD_GOGGLE_LATENCY_LOCK,
    OSD_GOGGLE_VTX_TEMP,
    OSD_GOGGLE_VRX_TEMP,
    OSD_GOGGLE_BATTERY_LOW,
    OSD_GOGGLE_BATTERY_VOLTAGE,
    OSD_GOGGLE_CLOCK_DATE,
    OSD_GOGGLE_CLOCK_TIME,
    OSD_GOGGLE_CHANNEL,
    OSD_GOGGLE_SD_REC,
    OSD_GOGGLE_VLQ,
    OSD_GOGGLE_ANT0,
    OSD_GOGGLE_ANT1,
    OSD_GOGGLE_ANT2,
    OSD_GOGGLE_ANT3,
    OSD_GOGGLE_TEMP_TOP,
    OSD_GOGGLE_TEMP_LEFT,
    OSD_GOGGLE_TEMP_RIGHT,

    OSD_GOGGLE_NUM,
} osd_goggle_element_e;

typedef struct {
    int orbit;
    setting_osd_embedded_mode_t embedded_mode;
    setting_osd_show_at_startup_t startup_visibility;
    bool is_visible;
    setting_osd_goggle_element_t element[OSD_GOGGLE_NUM];
} setting_osd_t;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int format;
} setting_clock_t;

#define WIFI_RF_CHANNELS  14 // World Channels
#define WIFI_NETWORK_MAX  16 // Includes NULL-Terminator
#define WIFI_SSID_MAX     33 // Includes NULL-Terminator
#define WIFI_PASSWD_MAX   64 // Includes NULL-Terminator
#define WIFI_PASSWD_MIN   8  // Minimum characters allowed
#define WIFI_CLIENTID_MAX 15 // Includes NULL-Terminator

enum {
    WIFI_MODE_AP = 0,
    WIFI_MODE_STA,
    WIFI_MODE_COUNT
};

typedef struct {
    bool enable;
    uint8_t mode; // 0 == WIFI_MODE_AP, 1 == WIFI_MODE_STA
    char clientid[WIFI_CLIENTID_MAX];
    char ssid[WIFI_MODE_COUNT][WIFI_SSID_MAX];
    char passwd[WIFI_MODE_COUNT][WIFI_PASSWD_MAX];
    bool dhcp;
    char ip_addr[WIFI_NETWORK_MAX];
    char netmask[WIFI_NETWORK_MAX];
    char gateway[WIFI_NETWORK_MAX];
    char dns[WIFI_NETWORK_MAX];
    uint8_t rf_channel;
    char root_pw[WIFI_SSID_MAX];
    bool ssh;
} wifi_t;

typedef struct {
    uint8_t no_dial; // 1=disable turning channels under video mode
    // 1=keep the HDZero tuner configured while the menu is open instead of
    // resetting it, so returning to video skips DM6302_init(). Costs power:
    // the tuner stays alive for as long as the menu is up.
} ease_use_t;

typedef struct {
    // Keep the HDZero tuner configured while the menu is open instead of
    // resetting it, so returning to video skips DM6302_init(), measured at
    // 2.3s. The tuner stays powered for as long as the menu is up.
    bool fast_menu;
    // Draw the menu over the running video instead of handing the display to
    // the UI layer, which is what forces the 1.1s dispw call in both
    // directions. The menu is laid out for 1080p, so it is cropped whenever
    // the video is not.
    bool keep_display;
    // Skip re-running audio_sel.sh when the mixer already holds the state
    // being asked for. It forks amixer once per control, twelve times for an
    // input change, and the switch path asks for the same state every time.
    bool skip_audio;
    // Skip configuring the display for the menu at boot. The app sets 1080p50
    // for the UI and then immediately sets the video timing, paying dispw
    // twice; the boot UI then shows at whatever mode the kernel left.
    bool boot_display;
    // Do not show the menu while starting up. It is created visible and
    // nothing hides it: what covers it once video starts is the OSD screen,
    // created later and drawn on top. Until then it is simply on screen.
    bool skip_boot_menu;
    // Bring the motion sensor up on a worker thread. It is 686ms of I2C in
    // device_init(), on the way to a picture, and nothing before the head
    // tracker actually runs needs it.
    bool async_imu;
    // Start the display timing change before the tuner init rather than after
    // it. dispw drives the SoC's display output and the tuner init is I2C to
    // the FPGA, so the two have no reason to be serialised, but between them
    // they are most of the time to a picture.
    bool async_display;
    // Read the OSD font bitmaps on a worker thread started before the display
    // and tuner are brought up, so the file I/O overlaps with them.
    bool boot_fonts;
    // Run the status bar and source status refresh at 20Hz instead of once per
    // main loop pass. They rewrite their labels every time, and a rewrite
    // invalidates them whether the text changed or not.
    bool ui_throttle;
    // Only write a label or image when its content actually changed.
    // lv_label_set_text() invalidates unconditionally, and the status bar
    // rewrites every field on every pass, so an idle bar was redrawing
    // constantly for nothing.
    bool label_diff;
    // Decide a long press by elapsed time rather than by counting the key
    // repeat events the kernel happens to send, which ties the feel of the
    // button to the autorepeat rate.
    bool timed_long_press;
    // Take and release lvgl_mutex around each stage of the main loop instead
    // of once around all of them, so the input handlers get several chances
    // to run per pass instead of waiting for the whole batch.
    bool split_lock;
    // Turn LVGL's antialiasing off while the scaled menu is up, and put it
    // back on the way out. Antialiasing is what makes the scaled menu heavy:
    // a transformed object is resampled bilinearly with it and by nearest
    // neighbour without. The flag itself is global, so it is scoped to the
    // menu here rather than left off over video and the OSD.
    bool menu_antialias_off;
    // Bring the HDZero tuner up on a worker started right after the devices,
    // when start-up is heading straight to HDZero video. DM6302_init() is
    // 1.8s of I2C to the FPGA and used to wait behind the whole UI build.
    // While it runs the main I2C bus is at the 1MHz the init asks for, so the
    // OLED and display set-up of the boot phase run at that speed too.
    bool async_tuner;
    // Send the seven FPGA register writes of one tuner SPI write as a single
    // I2C transaction instead of seven. Falls back to seven if the ioctl is
    // refused. Speeds up every DM6302_init() and channel change.
    bool spi_burst;
    // Do not run wlan_stop.sh at start-up when the WiFi driver is not loaded.
    // The script sleeps a second and then kills things that are not running,
    // and it blocks the main loop for 1.08s right after the picture appears.
    bool skip_wifi_stop;
    // Start the video timing change at start-up rather than at the switch.
    // dispw is 1.1s and it is the last thing the switch waits for, so run it
    // during the UI build instead. Needs Skip Display Setup (else the menu's
    // own timing runs a second dispw) and Skip Boot Menu (the panel stays
    // dark until the video arrives instead of lighting mid-reconfiguration).
    bool boot_display_early;
    // Build the menu pages after the video instead of before it. Nothing
    // needs them until someone opens the menu. Worth less than the 976ms it
    // moves, because what it exposes is the wait for the OSD fonts and then
    // the tuner and dispw underneath; the font preload is started earlier to
    // keep the first of those small.
    bool defer_menu;
} setting_speed_t;

typedef struct {
    // DM6302_init() gives up after ten tries and says so, but HDZero_open()
    // marked the tuner open regardless, so the app believed unconfigured
    // receivers were fine and nothing ever tried again. On means a failed
    // init leaves the tuner closed, and the next switch initialises it.
    bool retry_tuner_init;
} setting_bugfix_t;

typedef struct {
    // Beep on every recognised button press: the dial button short and long,
    // and the right button. A long press gets a longer beep so the two are
    // distinguishable by ear.
    bool button_beep;
    // Beep on every dial step. Short, because the dial turns quickly.
    bool dial_beep;
    // How long a press has to be held to count as long, when Timed Long Press
    // is on. One of LONG_PRESS_CHOICES.
    uint16_t long_press_ms;
} setting_input_t;

// Offered on the settings page as a slider, longest first.
#define LONG_PRESS_CHOICE_NUM 5
extern const uint16_t long_press_choices[LONG_PRESS_CHOICE_NUM];
// Position of ms in that list, or -1 if it is not one of them.
int long_press_choice_index(uint16_t ms);

typedef enum {
    SETTING_SOURCES_ANALOG_MODULE_INTERNAL = 0,
    SETTING_SOURCES_ANALOG_MODULE_EXTERNAL = 1
} setting_sources_analog_module_t;
typedef enum {
    SETTING_SOURCES_ANALOG_FORMAT_NTSC = 0,
    SETTING_SOURCES_ANALOG_FORMAT_PAL = 1
} setting_sources_analog_format_t;

typedef enum {
    SETTING_SOURCES_ANALOG_RATIO_4_3 = 0,
    SETTING_SOURCES_ANALOG_RATIO_16_9 = 1
} setting_sources_analog_ratio_t;
typedef enum {
    SETTING_SOURCES_HDZERO_BAND_RACEBAND = 0,
    SETTING_SOURCES_HDZERO_BAND_LOWBAND = 1
} setting_sources_hdzero_band_t;
typedef enum {
    SETTING_SOURCES_HDZERO_BW_WIDE = 0,
    SETTING_SOURCES_HDZERO_BW_NARROW = 1
} setting_sources_hdzero_bw_t;

typedef struct {
    setting_sources_analog_module_t analog_module;
    setting_sources_analog_format_t analog_format; // 0=NTSC, 1= PAL
    uint8_t analog_channel;
    setting_sources_analog_ratio_t analog_ratio; // 0=4:3, 1=16:9
    setting_sources_hdzero_band_t hdzero_band;
    setting_sources_hdzero_bw_t hdzero_bw;
} setting_sources_t;

typedef struct {
    uint16_t roller;
    uint16_t left_click;
    uint16_t left_press;
    uint16_t right_click;
    uint16_t right_press;
    uint16_t right_double_click;
} setting_inputs_t;

typedef struct {
    bool logging;
    bool selftest;
} setting_storage_t;

typedef struct {
    uint16_t lang;
} language_t;

typedef struct {
    uint16_t calib_min;
    uint16_t calib_max;
} setting_analog_rssi_t;

typedef struct {
    setting_scan_t scan;
    setting_favorites_t favorites;
    setting_speed_t speed;
    setting_input_t input;
    setting_bugfix_t bugfix;
    setting_fan_t fans;
    setting_autoscan_t autoscan;
    setting_power_t power;
    setting_sources_t source;
    setting_record_t record;
    setting_image_t image;
    setting_head_tracker_t ht;
    setting_elrs_t elrs;
    wifi_t wifi;
    setting_osd_t osd;
    setting_clock_t clock;
    setting_inputs_t inputs;
    ease_use_t ease;
    setting_storage_t storage;
    language_t language;
    setting_analog_rssi_t analog_rssi;
} setting_t;

extern setting_t g_setting;
extern const setting_t g_setting_defaults;

void settings_reset(void);
void settings_init(void);
void settings_load(void);
bool settings_get_bool(char *section, char *key, bool default_val);
int settings_put_bool(char *section, char *key, bool value);

int settings_put_osd_element(const setting_osd_goggle_element_t *element, char *config_name);
int settings_put_osd_element_pos_y(const setting_osd_goggle_element_positions_t *pos, char *config_name);
int settings_put_osd_element_pos_x(const setting_osd_goggle_element_positions_t *pos, char *config_name);
int settings_put_osd_element_shown(bool show, char *config_name);

#ifdef __cplusplus
}
#endif
