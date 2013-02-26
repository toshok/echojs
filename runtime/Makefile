TOP=..

include $(TOP)/build/build.mk

LIBRARY=libecho.a
SOURCES= \
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

OSX_OBJECTS=$(SOURCES:%.c=%.o.osx) main.o.osx
SIM_OBJECTS=$(SOURCES:%.c=%.o.sim) ios-main.o.sim
DEV_OBJECTS=$(SOURCES:%.c=%.o.armv7) ios-main.o.armv7

OSX_LIBRARY=$(LIBRARY)
SIM_LIBRARY=$(LIBRARY).sim
DEV_LIBRARY=$(LIBRARY).armv7
LIPOD_IOS_LIBRARY=$(LIBRARY).ios

ALL_LIBRARIES=$(OSX_LIBRARY) $(SIM_LIBRARY) $(DEV_LIBRARY) $(LIPOD_IOS_LIBRARY)

TARGET_LIBRARIES=$(LIPOD_IOS_LIBRARY) $(OSX_LIBRARY)

ALL_OBJECTS=$(SIM_OBJECTS) $(DEV_OBJECTS) $(OSX_OBJECTS)

all-local:: $(ALL_LIBRARIES)

clean:
	rm -f test $(ALL_OBJECTS) $(ALL_LIBRARIES) ejs-atoms-gen.c

VPATH=.:../parson

CFLAGS=-I. -Wall -g -O0 -Wno-unused-function
SHARED_CFLAGS=-fPIC $(CFLAGS)

DEV_CFLAGS=	-arch armv6 \
		-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk \
		-miphoneos-version-min=4.3

SIM_CFLAGS=	-arch i386 \
		-isysroot /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator5.0.sdk \
		-miphoneos-version-min=4.3

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

%.o.osx: %.c
	@echo [$(CC) osx] $< && $(CC) $(CFLAGS) -c -o $@ $<

%.o.osx: %.m
	@echo [$(CC) osx] $< && $(CC) -x objective-c++ $(CFLAGS) -c -o $@ $<

%.o.sim: %.c
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(IOSSIM_BIN)/clang $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.sim: %.m
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(IOSSIM_BIN)/clang $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.armv7: %.c
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(IOSDEV_BIN)/clang $(IOSDEV_CFLAGS) -c -o $@ $<

%.o.armv7: %.m
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(IOSDEV_BIN)/clang $(IOSDEV_CFLAGS) -c -o $@ $<

#CCLD=clang++
#CC=clang
#
#libecho.a: $(libecho_OBJECTS)
#	@echo [$(AR)] $@ && $(AR) cru $@ $(libecho_OBJECTS)
#
#$(libecho_OBJECTS): %.o: %.c
#	@mkdir -p .deps
#	@$(CC) -MM $(CFLAGS) $< > .deps/$@-deps
#	@echo [$(CC)] $< && $(CC) $(CFLAGS) -c $< -o $@

-include $(patsubst %.o,.deps/%.o-deps,$(libecho_OBJECTS))
