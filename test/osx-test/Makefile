TOP=../..

EJS_DRIVER?=../../ejs

#	make -C $(TOP)/pirouette/bindings
#	cp $(TOP)/pirouette/bindings/generated/*.js .

all: copy-pirouette
	$(EJS_DRIVER) --leave-temp helloosx.js
	mkdir -p HelloOSX.app/Contents/MacOS/
	cp helloosx.js.exe HelloOSX.app/Contents/MacOS/

copy-pirouette:
	@cp $(TOP)/pirouette/bindings/objc.js .
	@cp $(TOP)/pirouette/bindings/foundation.js .
	@cp $(TOP)/pirouette/bindings/appkit.js .
	@mkdir -p AppKit
	@mkdir -p Foundation
	@cp -r $(TOP)/pirouette/bindings/AppKit/ AppKit
	@cp -r $(TOP)/pirouette/bindings/Foundation/ Foundation


clean:
	rm -rf helloosx.js.exe objc.js foundation.js appkit.js HelloOSX.app/Contents/MacOS/helloosx.js.exe AppKit Foundation
