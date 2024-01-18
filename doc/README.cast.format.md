## 1. DATETIME TO STRING

The following patterns are currently implemented for datetime to string conversion:
| Format Pattern | Description |
| -------------- | ----------- |
| YEAR | Year (1 - 9999) |
| YYYY | Last 4 digits of Year (0001 - 9999) |
| YYY | Last 3 digits of Year (000 - 999) |
| YY | Last 2 digits of Year (00 - 99) |
| Y | Last 1 digits of Year (0 - 9) |
| Q | Quarter of the Year (1 - 4) |
| MM | Month (01 - 12) |
| MON | Short Month name (Apr) |
| MONTH | Full Month name (APRIL) |
| RM | Roman representation of the Month (I - XII) |
| WW | Week of the Year (01 - 53) |
| W | Week of the Month (1 - 5) |
| D | Day of the Week (1 - 7) |
| DAY | Full name of the Day (MONDAY) |
| DD | Day of the Month (01 - 31) |
| DDD | Day of the Year (001 - 366) |
| DY | Short name of the Day (Mon) |
| J | Julian Day (number of days since January 1, 4712 BC) |
| HH / HH12 | Hour of the Day (01 - 12) without Period (for Period use A.M or P.M.)  |
| HH24 | Hour of the Day (00 - 23) |
| MI | Minutes (00 - 59) |
| SS | Seconds (00 - 59) |
| SSSSS | Seconds after midnight (0 - 86399) |
| FF1 - FF9 | Fractional seconds with the specified accuracy |
| A.M. / P.M. | Period for 12 hours time (it doesn't matter which one is used, period will be inserted based on time) |
| TZH | Time zone in Hours  (-14 - 14) |
| TZM | Time zone in Minutes (00 - 59) |
| TZR | Time zone Name |

The dividers are:
| Dividers |
| ------------- |
| . |
| / |
| , |
| ; |
| : |
| 'space' |
| - |

Patterns can be used without any dividers:
```
SELECT CAST(CURRENT_TIMESTAMP AS VARCHAR(50) FORMAT 'YEARMMDD HH24MISS') FROM RDB$DATABASE;
=========================
20230719 161757
```
However, be careful with patterns like `DDDDD`, it will be interpreted as `DDD` + `DD`.

It is possible to insert raw text into a format string with `""`: `... FORMAT '"Today is" DAY'` - Today is MONDAY. To add `"` in output raw string use `\"` (to print `\` use `\\`).
Also the format is case-insensitive, so `YYYY-MM` == `yyyy-mm`.
Example:
```
SELECT CAST(CURRENT_TIMESTAMP AS VARCHAR(45) FORMAT 'DD.MM.YEAR HH24:MI:SS "is" J "Julian day"') FROM RDB$DATABASE;
=========================
14.6.2023 15:41:29 is 2460110 Julian day
```

## 2. STRING TO DATETIME

The following patterns are currently implemented for string to datetime conversion:
| Format Pattern | Description |
| ------------- | ------------- |
| YEAR | Year |
| YYYY | Last 4 digits of Year |
| YYY | Last 3 digits of Year |
| YY | Last 2 digits of Year |
| Y | Last 1 digits of Year |
| RR / RRRR | Round Year (further information below) | 
| MM | Month (1 - 12) |
| MON | Short Month name (Apr) |
| MONTH | Full Month name (APRIL) |
| RM | Roman representation of the Month (I - XII) |
| DD | Day of the Month (1 - 31) |
| J | Julian Day (number of days since January 1, 4712 BC) |
| HH / HH12 | Hour of the Day (1 - 12) without Period (to specify Period use A.M or P.M.) |
| HH24 | Hour of the Day (0 - 23) |
| MI | Minutes (0 - 59) |
| SS | Seconds (0 - 59) |
| SSSSS | Seconds after midnight (0 - 86399) |
| FF1 - FF4 | Fractional seconds with the specified accuracy |
| A.M. / P.M. | Period for 12 hours time (it doesn't matter which one is used, period will be taken from input string) |
| TZH | Time zone in Hours  (-14 - 14) |
| TZM | Time zone in Minutes (0 - 59) |
| TZR | Time zone Name |

Dividers are the same as for datetime to string conversion and can also be omitted.

Year, month and day will be taken from current date if these components are not used in pattern (this applies only to data types that contain a date component).

Behavior of `RR`:
- If the specified two-digit year is 00 to 49, then
  - If the last two digits of the current year are 00 to 49, then the returned year has the same first two digits as the current year.
  - If the last two digits of the current year are 50 to 99, then the first 2 digits of the returned year are 1 greater than the first 2 digits of the current year.
- If the specified two-digit year is 50 to 99, then
  - If the last two digits of the current year are 00 to 49, then the first 2 digits of the returned year are 1 less than the first 2 digits of the current year.
  - If the last two digits of the current year are 50 to 99, then the returned year has the same first two digits as the current year.

Behavior of `RRRR`: Accepts either 4-digit or 2-digit input. If 2-digit, provides the same return as `RR`. If you do not want this functionality, then enter the 4-digit year.


Example:
```
SELECT CAST('2000.12.08 12:35:30.5000' AS TIMESTAMP FORMAT 'YEAR.MM.DD HH24:MI:SS.FF4') FROM RDB$DATABASE;
=====================
2000-12-08 12:35:30.5000
```
