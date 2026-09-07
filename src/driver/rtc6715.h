#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void RTC6715_Open(int power_on, int audio_on);
void RTC6715_SetCH(int ch);
int RTC6715_GetRssi();
void *thread_rtc6715_rssi(void *ptr);

extern int rtc6715_rssi;

#ifdef __cplusplus
}
#endif
