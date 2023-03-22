#!/usr/bin/env python3

from libreCAL import libreCAL
import datetime
import time

def get_datetime_with_UTC_offset():
    dt = datetime.datetime.now()
    # get current time in UTC
    now_utc = datetime.datetime.utcnow()

    # get UTC offset
    utc_offset = dt - now_utc

    # calculate offset in hours and minutes
    offset_seconds = int(utc_offset.total_seconds())
    offset_hours = abs(offset_seconds // 3600)
    offset_minutes = abs((offset_seconds // 60) % 60)

    if offset_seconds < 0:
        sign = "-"
    else:
        sign = "+"

    offset_str = f"{sign}{offset_hours:02d}:{offset_minutes:02d}"

    # format datetime as string
    dt_str = dt.strftime("%Y/%m/%d %H:%M:%S")

    # add UTC offset to string
    dt_str_with_offset = f"{dt_str} UTC{offset_str}"

    return dt_str_with_offset


# Connect to the first detected LibreCAL
cal = libreCAL()
print("Connected to LibreCAL with serial "+cal.getSerial())
pc_local_date_time_utc = get_datetime_with_UTC_offset()
librecal_date_time_utc = cal.getDateTimeUTC()
print("PC local Date+Time+UTC "+pc_local_date_time_utc)
print("LibreCAL Date+Time+UTC "+librecal_date_time_utc)
time.sleep(1)
pc_local_date_time_utc = get_datetime_with_UTC_offset()
librecal_date_time_utc = cal.getDateTimeUTC()
print("PC local Date+Time+UTC "+pc_local_date_time_utc)
print("LibreCAL Date+Time+UTC "+librecal_date_time_utc)
