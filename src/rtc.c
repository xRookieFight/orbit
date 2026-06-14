#include "rtc.h"
#include "io.h"

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

void rtc_now(rtc_time_t* out)
{
    while (cmos_read(0x0A) & 0x80)
        ;
    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t reg_b = cmos_read(0x0B);

    if (!(reg_b & 0x04)) {
        sec = from_bcd(sec);
        min = from_bcd(min);
        hour = from_bcd(hour);
        day = from_bcd(day);
        month = from_bcd(month);
        year = from_bcd(year);
    }

    out->year = 2000 + year;
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = min;
    out->second = sec;
}
