TOP=$(shell cd ../ && pwd)

EJS_DRIVER?=$(TOP)/ejs

trackmixcode:
	@mkdir -p TrackMixCode.app/Contents/MacOS/
	$(EJS_DRIVER) -o TrackMixCode.app/Contents/MacOS/trackmixcode.js.exe -I pirouette=$(TOP)/pirouette/bindings trackmixcode.js

all: trackmixcode

