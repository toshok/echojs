TOP=..

include $(TOP)/build/config.mk

LIBRARY=libecho.a
C_SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-boolean.c \
	ejs-builtin-modules.c \
	ejs-closureenv.c \
	ejs-console.c \
	ejs-date.c \
	ejs-error.c \
	ejs-exception.c \
	ejs-function.c \
	ejs-gc.c \
	ejs-init.c \
	ejs-json.c \
	ejs-map.c \
	ejs-math.c \
	ejs-number.c \
	ejs-object.c \
	ejs-ops.c \
	ejs-process.c \
	ejs-promise.c \
	ejs-proxy.c \
	ejs-recording.c \
	ejs-reflect.c \
	ejs-regexp.c \
	ejs-require.c \
	ejs-set.c \
	ejs-string.c \
	ejs-symbol.c \
	ejs-typedarrays.c \
	ejs-types.c \
	ejs-uri.c \
	parson.c

VPATH=.:../parson

CC=clang


ejs-atoms-gen.c: ejs-atoms.h gen-atoms.js
	@echo [GEN] $@ && ./gen-atoms.js $< > .tmp-$@ && mv .tmp-$@ $@

ifeq ($(HOST_OS),linux)
ALL_LIBRARIES=$(LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)

LINUX_OBJECTS=$(C_SOURCES:%.c=%.o.linux) $(OBJC_SOURCES:%.m=%.o.linux) ejs-log.o.linux main.o.linux

ALL_OBJECTS=$(LINUX_OBJECTS)

CFLAGS += -I../external-deps/pcre-linux

ejs-init.o.linux: ejs-atoms-gen.c

$(LIBRARY): $(LINUX_OBJECTS)
	@echo [ar linux] $@ && /usr/bin/ar rc $@ $(LINUX_OBJECTS)

OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch

%.o.linux: %.c
	@mkdir -p .deps
	@$(CC) -MM $(LINUX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.linux,,`/$@/ > .deps/$@-deps
	@echo [$(CC) linux] $< && $(CC) -ObjC $(LINUX_CFLAGS)  -c -o $@ $<

%.o.linux: %.m
	@mkdir -p .deps
	@$(CC) -MM $(LINUX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.linux,,`/$@/ > .deps/$@-deps
	@echo [$(CC) linux] $< && $(CC) $(LINUX_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

-include $(patsubst %.o.linux,.deps/%.o.linux-deps,$(LINUX_OBJECTS))
endif

ifeq ($(HOST_OS),darwin)

OBJC_SOURCES= \
	ejs-jsobjc.m \
	ejs-log.m \
	ejs-objc.m \
	ejs-webgl.m \
	ejs-xhr.m

OSX_OBJECTS=$(C_SOURCES:%.c=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx) main.o.osx
SIM_OBJECTS=$(C_SOURCES:%.c=%.o.sim) $(OBJC_SOURCES:%.m=%.o.sim) main.o.sim
DEV_OBJECTS=$(C_SOURCES:%.c=%.o.armv7) $(OBJC_SOURCES:%.m=%.o.armv7) main.o.armv7
DEVS_OBJECTS=$(C_SOURCES:%.c=%.o.armv7s) $(OBJC_SOURCES:%.m=%.o.armv7s) main.o.armv7s

analyze_plists_c = $(C_SOURCES:%.c=%.plist) main.plist
analyze_plists_objc = $(OBJC_SOURCES:%.m=%.plist) 

OSX_LIBRARY=$(LIBRARY)
SIM_LIBRARY=$(LIBRARY).sim
DEV_LIBRARY=$(LIBRARY).armv7
DEVS_LIBRARY=$(LIBRARY).armv7s
LIPOD_IOS_LIBRARY=$(LIBRARY).ios

ifneq ($(TRAVIS_BUILD_NUMBER),)
ALL_LIBRARIES=$(OSX_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)
else
ALL_LIBRARIES=$(OSX_LIBRARY) $(SIM_LIBRARY) $(DEV_LIBRARY) $(DEVS_LIBRARY) $(LIPOD_IOS_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES) $(analyze_plists_c) $(analyze_plists_objc)
endif

ALL_OBJECTS=$(SIM_OBJECTS) $(DEV_OBJECTS) $(DEVS_OBJECTS) $(OSX_OBJECTS)

CFLAGS += -I../external-deps/pcre-osx
IOSSIM_CFLAGS += -I../external-deps/pcre-iossim
IOSDEV_CFLAGS += -I../external-deps/pcre-iosdev


$(OSX_LIBRARY): $(OSX_OBJECTS)
	@echo [ar osx] $@ && /usr/bin/ar rc $@ $(OSX_OBJECTS)

$(SIM_LIBRARY): $(SIM_OBJECTS)
	@echo [ar sim] $@ && /usr/bin/ar rc $@ $(SIM_OBJECTS)

$(DEV_LIBRARY): $(DEV_OBJECTS)
	@echo [ar armv7] $@ && $(IOSDEV_BIN)/ar rc $@ $(DEV_OBJECTS)

$(DEVS_LIBRARY): $(DEVS_OBJECTS)
	@echo [ar armv7s] $@ && $(IOSDEVS_BIN)/ar rc $@ $(DEVS_OBJECTS)

$(LIPOD_IOS_LIBRARY): $(SIM_LIBRARY) $(DEV_LIBRARY) $(DEVS_LIBRARY)
	@echo [lipo] $@ && lipo -create $(SIM_ARCH) $(SIM_LIBRARY) $(DEV_ARCH) $(DEV_LIBRARY) $(DEVS_ARCH) $(DEVS_LIBRARY) -output $@

ejs-init.o.osx ejs-init.o.sim ejs-init.o.armv7 ejs-init.o.armv7s: ejs-atoms-gen.c

ejs-webgl-constants-sorted.h: ejs-webgl-constants.h
	@echo [GEN] $@ && (grep WEBGL_CONSTANT $< | sort > $@)

ejs-webgl.o.osx ejs-webgl.o.sim ejs-webgl.o.armv7 ejs-webgl.o.armv7s: ejs-webgl-constants-sorted.h


OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch

%.o.osx: %.c
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) -ObjC $(OSX_CFLAGS)  -c -o $@ $<

%.o.osx: %.m
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) $(OSX_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.sim: %.c
	@mkdir -p .deps
	@$(CC) -MM $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) -ObjC $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.sim: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) $(IOSSIM_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) -ObjC $(IOSDEV_CFLAGS) -c -o $@ $<

%.o.armv7: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) $(IOSDEV_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7s: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7s] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) -ObjC $(IOSDEVS_CFLAGS) -c -o $@ $<

%.o.armv7s: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) $(IOSDEVS_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

$(analyze_plists_c): %.plist: %.c
	@echo [$(CC) analyze] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) $(OSX_CFLAGS) --analyze $< -o $@

$(analyze_plists_objc): %.plist: %.m
	@echo [$(CC) analyze] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) $(OSX_CFLAGS) --analyze $< -o $@


class-test: $(C_SOURCES:%.c=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx) class-test.o.osx
	@echo [$(CC) osx] $< 
	$(CC) -ObjC $(OSX_CFLAGS) -o $@ $(C_SOURCES:%.c=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx) class-test.o.osx ../external-deps/pcre-osx/.libs/libpcre16.a -framework Foundation -framework AppKit -lstdc++


-include $(patsubst %.o.osx,.deps/%.o.osx-deps,$(OSX_OBJECTS))
-include $(patsubst %.o.sim,.deps/%.o.sim-deps,$(SIM_OBJECTS))
-include $(patsubst %.o.armv7,.deps/%.o.armv7-deps,$(DEV_OBJECTS))
-include $(patsubst %.o.armv7s,.deps/%.o.armv7s-deps,$(DEVS_OBJECTS))
endif

all-local:: $(ALL_TARGETS)

clean-local::
	rm -f test $(ALL_OBJECTS) $(ALL_LIBRARIES) ejs-atoms-gen.c $(analyze_plists_c) $(analyze_plists_objc)

include $(TOP)/build/build.mk
