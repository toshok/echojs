#/bin/sh

rm -f $2.exe

../../ejs $@

exec $2.exe
