#pragma once

#define DIAL_SENSITIVITY           1    // number of clicks before dial event is triggered
#define DIAL_SENSITIVTY_TIMEOUT_MS 1000 // ms
#define CHANNEL_SHOWTIME           30   // must <= 127

#define GPIO_BEEP 131

#define GPIO_ESP32_BOOT0 128
#define GPIO_ESP32_EN    129
#define GPIO_ESP32_BOOT  130

#define GPIO_TP2825_RSTB 132

#define GPIO_HDZ_RX_RESET 224
#define GPIO_HDZ_TX_RESET 228
#define GPIO_FPGA_RESET   258

#define DEV_SPI_VTX_VRX_R "/dev/mtd8"
#define DEV_SPI_VRX_L     "/dev/mtd9"
#define DEV_SPI_VA        "/dev/mtd10"

#define SELF_TEST_FILE    "/mnt/extsd/self_test.txt"
#define NO_DIAL_FILE      "/mnt/extsd/no_dial.txt"
#define APP_LOG_FILE      "/mnt/extsd/HDZGOGGLE.log"
#define APP_LOG_FILE_PREV "/mnt/extsd/HDZGOGGLE.prev.log"
// Older boots, newest first: HDZGOGGLE.1.log is the boot before this one.
#define APP_LOG_FILE_OLD  "/mnt/extsd/HDZGOGGLE.%d.log"
// How many of those to keep. Two -- this boot and the one before -- meant a
// log was one power-on away from being gone, and the interesting boot is
// always identified after the fact. Ten of them is about a megabyte. The
// recorder writes 2.86MB a second (94.6MB for 33.1s, measured), so the whole
// set costs a third of a second of video on a card that holds six hours.
#define APP_LOG_KEEP      10
#define APP_BIN_FILE      "/mnt/extsd/HDZGOGGLE"
#define DEVELOP_SCRIPT    "/mnt/extsd/develop.sh"
