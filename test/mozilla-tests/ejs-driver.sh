#/bin/sh

rm -f $2.exe

TOPDIR=../..

NODE_PATH="$TOPDIR/node-llvm/build/Release:$TOPDIR/lib/coffee:$TOPDIR/lib:$TOPDIR/esprima:$TOPDIR/escodegen" $TOPDIR/ejs $@

exec $2.exe
