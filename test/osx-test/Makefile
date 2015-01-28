TOP=$(shell cd ../.. && pwd)

EJS_DRIVER?=$(TOP)/ejs.exe

all:
	@mkdir -p HelloOSX.app/Contents/MacOS/
	time $(EJS_DRIVER) --srcdir --leave-temp -o HelloOSX.app/Contents/MacOS/helloosx.js.exe -I pirouette=$(TOP)/pirouette/bindings helloosx.js

clean:
	rm -rf HelloOSX.app/Contents/MacOS/helloosx.js.exe
