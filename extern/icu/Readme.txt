icu.exe is a self-extract pre-built (by us) IBM ICU 63.1 library.

The sources was downloaded from http://site.icu-project.org/download.

The simple fix for bug ICU-20302 (Windows 7: timezone detection on Windows 
is broken) is applyed, see:

https://unicode-org.atlassian.net/browse/ICU-20302
https://github.com/unicode-org/icu/pull/315

The build was done using VS 2017 (15.9).
