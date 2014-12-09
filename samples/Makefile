TOP=$(shell cd ../ && pwd)

EJS_DRIVER?=$(TOP)/ejs

trackmixcode: trackmixcode/trackmixcode.js
	@mkdir -p trackmixcode/TrackMixCode.app/Contents/MacOS/
	$(EJS_DRIVER) -o trackmixcode/TrackMixCode.app/Contents/MacOS/trackmixcode.js.exe -I pirouette=$(TOP)/pirouette/bindings trackmixcode/trackmixcode.js

all: trackmixcode

