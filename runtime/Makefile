TOP=..

include $(TOP)/build/build.mk

LIBRARY=libecho.a
C_SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-boolean.c \
	ejs-builtin-modules.c \
	ejs-console.c \
	ejs-closureenv.c \
	ejs-date.c \
	ejs-error.c \
	ejs-exception.c \
	ejs-function.c \
	ejs-gc.c \
	ejs-init.c \
	ejs-json.c \
	ejs-math.c \
	ejs-number.c \
	ejs-object.c \
	ejs-ops.c \
	ejs-process.c \
	ejs-regexp.c \
	ejs-require.c \
	ejs-string.c \
	ejs-typedarrays.c \
	ejs-types.c \
	ejs-uri.c \
	parson.c

OBJC_SOURCES= \
	ejs-jsobjc.m \
	ejs-objc.m \
	ejs-webgl.m \
	ejs-xhr.m

OSX_OBJECTS=$(C_SOURCES:%.c=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx) main.o.osx
SIM_OBJECTS=$(C_SOURCES:%.c=%.o.sim) $(OBJC_SOURCES:%.m=%.o.sim) main.o.sim
DEV_OBJECTS=$(C_SOURCES:%.c=%.o.armv7) $(OBJC_SOURCES:%.m=%.o.armv7) main.o.armv7

OSX_LIBRARY=$(LIBRARY)
SIM_LIBRARY=$(LIBRARY).sim
DEV_LIBRARY=$(LIBRARY).armv7
LIPOD_IOS_LIBRARY=$(LIBRARY).ios

ALL_LIBRARIES=$(OSX_LIBRARY) $(SIM_LIBRARY) $(DEV_LIBRARY) $(LIPOD_IOS_LIBRARY)

TARGET_LIBRARIES=$(LIPOD_IOS_LIBRARY) $(OSX_LIBRARY)

ALL_OBJECTS=$(SIM_OBJECTS) $(DEV_OBJECTS) $(OSX_OBJECTS)

all-local:: $(ALL_LIBRARIES)

clean-local::
	rm -f test $(ALL_OBJECTS) $(ALL_LIBRARIES) ejs-atoms-gen.c

VPATH=.:../parson

OSX_CC=clang
SIM_CC=clang
DEV_CC=clang

CFLAGS += -I../external-deps/pcre-osx
IOSSIM_CFLAGS += -I../external-deps/pcre-iossim
IOSDEV_CFLAGS += -I../external-deps/pcre-iosdev


$(OSX_LIBRARY): $(OSX_OBJECTS)
	@echo [ar osx] $@ && /usr/bin/ar rc $@ $(OSX_OBJECTS)

$(SIM_LIBRARY): $(SIM_OBJECTS)
	@echo [ar sim] $@ && $(IOSSIM_BIN)/ar rc $@ $(SIM_OBJECTS)

$(DEV_LIBRARY): $(DEV_OBJECTS)
	@echo [ar armv7] $@ && $(IOSDEV_BIN)/ar rc $@ $(DEV_OBJECTS)

$(LIPOD_IOS_LIBRARY): $(SIM_LIBRARY) $(DEV_LIBRARY)
	@echo [lipo] $< && lipo -create $(SIM_ARCH) $(SIM_LIBRARY) $(DEV_ARCH) $(DEV_LIBRARY) -output $@

ejs-atoms-gen.c: ejs-atoms.h gen-atoms.js
	@echo [GEN] $@ && ./gen-atoms.js > .tmp-$@ && mv .tmp-$@ $@

ejs-init.o.osx ejs-init.o.sim ejs-init.o.armv7: ejs-atoms-gen.c

ejs-webgl-constants-sorted.h: ejs-webgl-constants.h
	@echo [GEN] $@ && (grep WEBGL_CONSTANT $< | sort > $@)

ejs-webgl.o.osx ejs-webgl.o.sim ejs-webgl.o.armv7: ejs-webgl-constants-sorted.h


OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch

%.o.osx: %.c
	@mkdir -p .deps
	@$(OSX_CC) -MM $(CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(OSX_CC) osx] $< && $(OSX_CC) -ObjC $(CFLAGS)  -c -o $@ $<

%.o.osx: %.m
	@mkdir -p .deps
	@$(OSX_CC) -MM $(CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(OSX_CC) osx] $< && $(OSX_CC) $(CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.sim: %.c
	@mkdir -p .deps
	@$(SIM_CC) -MM $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	echo [$(SIM_CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(SIM_CC) -ObjC $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.sim: %.m
	@mkdir -p .deps
	@$(SIM_CC) -MM -ObjC $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(SIM_CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(SIM_CC) $(IOSSIM_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7: %.c
	@mkdir -p .deps
	@$(DEV_CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.dev,,`/$@/ > .deps/$@-deps
	@echo [$(DEV_CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(DEV_CC) -ObjC $(IOSDEV_CFLAGS) -c -o $@ $<

%.o.armv7: %.m
	@mkdir -p .deps
	@$(DEV_CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.dev,,`/$@/ > .deps/$@-deps
	@echo [$(DEV_CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(DEV_CC) $(IOSDEV_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

-include $(patsubst %.o.osx,.deps/%.o.osx-deps,$(OSX_OBJECTS))
-include $(patsubst %.o.sim,.deps/%.o.sim-deps,$(SIM_OBJECTS))
-include $(patsubst %.o.armv7,.deps/%.o.armv7-deps,$(DEV_OBJECTS))
