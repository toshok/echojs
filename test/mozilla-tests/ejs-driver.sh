#/bin/sh

rm -f $2.exe

TOPDIR=../..

NODE_PATH="$TOPDIR/node-llvm/build/Release:$TOPDIR/lib/generated:$TOPDIR/lib:$TOPDIR/esprima:$TOPDIR/escodegen:$TOPDIR/estraverse" $TOPDIR/ejs $@

exec $2.exe
