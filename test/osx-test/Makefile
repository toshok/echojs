TOP=../..

all:
	make -C $(TOP)/coffeekit/coffee
	cp $(TOP)/coffeekit/coffee/generated/*.js .
	coffee -c helloosx.coffee
	../../ejs helloosx.js appkit.js coffeekit.js foundation.js
	mkdir -p HelloOSX.app/Contents/MacOS/
	cp helloosx.js.exe HelloOSX.app/Contents/MacOS/

clean:
	rm helloosx.js.exe *.js HelloOSX.app/Contents/MacOS/helloosx.js.exe
