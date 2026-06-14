#pragma once
#include <stdint.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} rtc_time_t;

void rtc_now(rtc_time_t* out);
