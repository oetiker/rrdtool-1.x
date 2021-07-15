#!/bin/bash
echo "MSYSTEM:     $MSYSTEM"
echo "MINGW_CHOST: $MINGW_CHOST"
./bootstrap
./configure --host="$MINGW_CHOST" --disable-static --disable-mmap --disable-tcl --disable-perl --disable-ruby --disable-python --disable-lua --disable-rrdcached --without-libintl-prefix --without-libiconv-prefix
make CFLAGS='-D__USE_MINGW_ANSI_STDIO=1'

# The export of TZ=Europe/Zurich in tests/functions does not work under Windows. The timezone needs to be set
# for Windows, e.g. using tzutil.exe
# The timezone TZ=Europe/Zurich is required for several tests to pass:
# modify1,modify2,modify3,modify4,modify5,tune1,tune2
TZ_BAK=$(tzutil.exe //g)
echo Current timezone="$TZ_BAK"
tzutil.exe //s "W. Europe Standard Time"
echo New timezone="$(tzutil.exe //g)"

# Set first day of week to Sunday. This is required for test "rpn2"
# 0 ... Monday, 6 ... Sunday
iFirstDayOfWeek_key=$(reg query "HKEY_CURRENT_USER\Control Panel\International" //v iFirstDayOfWeek)
iFirstDayOfWeek_BAK="${iFirstDayOfWeek_key//[!0-9]/}"
echo Current iFirstDayOfWeek="$iFirstDayOfWeek_BAK"
reg add "HKEY_CURRENT_USER\Control Panel\International" //t REG_SZ //v iFirstDayOfWeek //d 6 //f

make check || :

# Restore first day of week
reg add "HKEY_CURRENT_USER\Control Panel\International" //t REG_SZ //v iFirstDayOfWeek //d "$iFirstDayOfWeek_BAK" //f

# Restore the timezone
tzutil.exe //s "$TZ_BAK"
echo Restored timezone="$(tzutil.exe //g)"
