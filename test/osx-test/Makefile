TOP=$(shell cd ../.. && pwd)

EJS_DRIVER?=$(TOP)/ejs.exe

MODULE_DIRS=--moduledir $(TOP)/modules/objc_internal
all:
	@mkdir -p HelloOSX.app/Contents/MacOS/
	time $(EJS_DRIVER) --srcdir --leave-temp $(MODULE_DIRS) -o HelloOSX.app/Contents/MacOS/helloosx.js.exe -I pirouette=$(TOP)/pirouette/bindings helloosx.js

with-pirouette:
	pirouette build

clean:
	rm -rf HelloOSX.app/Contents/MacOS/helloosx.js.exe
