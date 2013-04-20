TOP=../..

all:
	make -C $(TOP)/pirouette/bindings
	cp $(TOP)/pirouette/bindings/generated/*.js .
	../../ejs --leave-temp helloosx.js appkit.js objc.js foundation.js
	mkdir -p HelloOSX.app/Contents/MacOS/
	cp helloosx.js.exe HelloOSX.app/Contents/MacOS/

clean:
	rm -f helloosx.js.exe *.js HelloOSX.app/Contents/MacOS/helloosx.js.exe
