TOP=$(shell cd ../ && pwd)

EJS_DRIVER?=$(TOP)/ejs

trackmix: trackmix/trackmix.js
	@mkdir -p trackmix/TrackMix.app/Contents/MacOS/
	ibtool --compile trackmix/TrackMix.app/Contents/Resources/Base.lproj/MainMenu.nib trackmix/TrackMix.app/Contents/Resources/Base.lproj/MainMenu.xib
	$(EJS_DRIVER) -o trackmix/TrackMix.app/Contents/MacOS/trackmix.js.exe -I pirouette=$(TOP)/pirouette/bindings trackmix/trackmix.js

trackmixcode: trackmixcode/trackmixcode.js
	@mkdir -p trackmixcode/TrackMixCode.app/Contents/MacOS/
	$(EJS_DRIVER) -o trackmixcode/TrackMixCode.app/Contents/MacOS/trackmixcode.js.exe -I pirouette=$(TOP)/pirouette/bindings trackmixcode/trackmixcode.js

all: trackmix trackmixcode

