TOP=..

include $(TOP)/build/config.mk

LIBRARY=libecho.a
C_SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-boolean.c \
	ejs-closureenv.c \
	ejs-console.c \
	ejs-date.c \
	ejs-error.c \
	ejs-exception.c \
	ejs-function.c \
	ejs-gc.c \
	ejs-generator.c \
	ejs-init.c \
	ejs-json.c \
	ejs-map.c \
	ejs-math.c \
	ejs-module.c \
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
	ejs-stream.c \
	ejs-string.c \
	ejs-symbol.c \
	ejs-timers.c \
	ejs-typedarrays.c \
	ejs-types.c \
	ejs-uri.c \
	ejs-weakmap.c \
	ejs-weakset.c \
	parson.c

CPP_SOURCES= \
	ejs-dtoa.cpp

VPATH=.:../external-deps/parson

ejs-atoms-gen.c: ejs-atoms.h gen-atoms.js
	@echo [GEN] $@ && ./gen-atoms.js $< > .tmp-$@ && mv .tmp-$@ $@

ifeq ($(HOST_OS),linux)
ALL_LIBRARIES=$(LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)

ifeq ($(EJS_RUNLOOP_IMPL),noop)
RUNLOOP_DEF=-DNOOP_RUNLOOP=1
RUNLOOP_C_SOURCE=ejs-runloop-noop.c
else
RUNLOOP_DEF=-DHAVE_LIBUV=1
RUNLOOP_C_SOURCE=ejs-runloop-libuv.c
endif

LINUX_OBJECTS=$(C_SOURCES:%.c=%.o.linux) $(CPP_SOURCES:%.cpp=%.o.linux) $(OBJC_SOURCES:%.m=%.o.linux) ejs-log.o.linux main.o.linux ejs-invoke-closure-catch.o.linux $(RUNLOOP_C_SOURCE:%.c=%.o.linux)

ALL_OBJECTS=$(LINUX_OBJECTS)

CFLAGS += -I/usr/include/libunwind -I../external-deps/pcre-linux -I../external-deps/double-conversion

ejs-init.o.linux: ejs-atoms-gen.c

$(LIBRARY): $(LINUX_OBJECTS)
	@echo [ar linux] $@ && /usr/bin/ar rc $@ $(LINUX_OBJECTS)

OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch

%.o.linux: %.c
	@mkdir -p .deps
	@$(CC) -MM $(LINUX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.linux,,`/$@/ > .deps/$@-deps
	@echo [$(CC) linux] $< && $(CC) $(LINUX_CFLAGS)  -c -o $@ $<

%.o.linux: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(LINUX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.linux,,`/$@/ > .deps/$@-deps
	@echo [$(CXX) linux] $< && $(CXX) -std=c++11 $(LINUX_CFLAGS)  -c -o $@ $<

%.o.linux: %.m
	@mkdir -p .deps
	@$(CC) -MM $(LINUX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.linux,,`/$@/ > .deps/$@-deps
	@echo [$(CC) linux] $< && $(CC) $(LINUX_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.linux: %.ll
	@echo [llc linux] $< && llc$(LLVM_SUFFIX) --relocation-model=pic -filetype=obj -o=$@ -O2 $<

-include $(patsubst %.o.linux,.deps/%.o.linux-deps,$(LINUX_OBJECTS))
endif

ifeq ($(HOST_OS),darwin)

OBJC_SOURCES= \
	ejs-jsobjc.m \
	ejs-log.m \
	ejs-objc.m \
	ejs-webgl.m \
	ejs-xhr.m \
	ejs-runloop-darwin.m

OSX_OBJECTS=$(C_SOURCES:%.c=%.o.osx) $(CPP_SOURCES:%.cpp=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx) ejs-invoke-closure-catch.o.osx main.o.osx
SIM_OBJECTS=$(C_SOURCES:%.c=%.o.sim) $(CPP_SOURCES:%.cpp=%.o.sim) $(OBJC_SOURCES:%.m=%.o.sim) ejs-invoke-closure-catch.o.sim main.o.sim
DEV_OBJECTS=$(C_SOURCES:%.c=%.o.armv7) $(CPP_SOURCES:%.cpp=%.o.armv7) $(OBJC_SOURCES:%.m=%.o.armv7) ejs-invoke-closure-catch-sret.o.armv7 main.o.armv7
DEVS_OBJECTS=$(C_SOURCES:%.c=%.o.armv7s) $(CPP_SOURCES:%.cpp=%.o.armv7s) $(OBJC_SOURCES:%.m=%.o.armv7s) ejs-invoke-closure-catch.o.armv7s main.o.armv7s

analyze_plists_c = $(C_SOURCES:%.c=%.plist) main.plist
analyze_plists_objc = $(OBJC_SOURCES:%.m=%.plist) 

OSX_LIBRARY=$(LIBRARY)
SIM_LIBRARY=$(LIBRARY).sim
DEV_LIBRARY=$(LIBRARY).armv7
DEVS_LIBRARY=$(LIBRARY).armv7s

ifneq ($(CIRCLE_BUILD_NUM),)
# on circleci we only build the osx library
ALL_LIBRARIES=$(OSX_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)
else
# on local builds we build all the libraries (XXX need to figure out how to accurately target those platforms first)
ALL_LIBRARIES=$(OSX_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES) $(analyze_plists_c) $(analyze_plists_objc)
endif

ALL_OBJECTS=$(SIM_OBJECTS) $(DEV_OBJECTS) $(DEVS_OBJECTS) $(OSX_OBJECTS)

CFLAGS += -I../external-deps/pcre-osx -I../external-deps/double-conversion
IOSSIM_CFLAGS += -I../external-deps/pcre-iossim -I../external-deps/double-conversion
IOSDEV_CFLAGS += -I../external-deps/pcre-iosdev -I../external-deps/double-conversion


$(OSX_LIBRARY): $(OSX_OBJECTS)
	@echo [ar osx] $@ && /usr/bin/ar rc $@ $(OSX_OBJECTS)

$(SIM_LIBRARY): $(SIM_OBJECTS)
	@echo [ar sim] $@ && /usr/bin/ar rc $@ $(SIM_OBJECTS)

$(DEV_LIBRARY): $(DEV_OBJECTS)
	@echo [ar armv7] $@ && /usr/bin/ar rc $@ $(DEV_OBJECTS)

$(DEVS_LIBRARY): $(DEVS_OBJECTS)
	@echo [ar armv7s] $@ && /usr/bin/ar rc $@ $(DEVS_OBJECTS)

ejs-init.o.osx ejs-init.o.sim ejs-init.o.armv7 ejs-init.o.armv7s: ejs-atoms-gen.c

ejs-webgl-constants-sorted.h: ejs-webgl-constants.h
	@echo [GEN] $@ && (grep WEBGL_CONSTANT $< | sort > $@)

ejs-webgl.o.osx ejs-webgl.o.sim ejs-webgl.o.armv7 ejs-webgl.o.armv7s: ejs-webgl-constants-sorted.h


OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch -fno-objc-arc

%.o.osx: %.c
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) -ObjC $(OSX_CFLAGS)  -c -o $@ $<

%.o.osx: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CXX) osx] $< && $(CXX) -std=c++11 $(OSX_CFLAGS)  -c -o $@ $<

%.o.osx: %.m
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) $(OSX_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.osx: %.ll
	@echo [llc osx] $< && llc$(LLVM_SUFFIX) -mtriple=$(OSX_MTRIPLE) -filetype=obj -o=$@ -O2 $<

%.o.sim: %.c
	@mkdir -p .deps
	@$(CC) -MM $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) -ObjC $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.sim: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CXX) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CXX) -std=c++11 $(IOSSIM_CFLAGS)  -c -o $@ $<

%.o.sim: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) $(IOSSIM_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.sim: %.ll
	@echo [llc sim] $< && llc$(LLVM_SUFFIX) -march=x86 -mtriple=$(IOSSIM_MTRIPLE) -filetype=obj -o=$@ -O2 $<

%.o.armv7: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) -ObjC $(IOSDEV_CFLAGS) -c -o $@ $<

%.o.armv7: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CXX) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CXX) -std=c++11 $(IOSDEV_CFLAGS)  -c -o $@ $<

%.o.armv7: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) $(IOSDEV_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7: %.ll
	@echo [llc armv7] $< && llc$(LLVM_SUFFIX) -march=arm -mtriple=$(IOSDEV_MTRIPLE) -filetype=obj -o=$@ -O2 $<

%.o.armv7s: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7s] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) -ObjC $(IOSDEVS_CFLAGS) -c -o $@ $<

%.o.armv7s: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CXX) armv7s] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CXX) -std=c++11 $(IOSDEVS_CFLAGS)  -c -o $@ $<

%.o.armv7s: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) $(IOSDEVS_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7s: %.ll
	@echo [llc armv7s] $< && llc$(LLVM_SUFFIX) -march=arm -mtriple=$(IOSDEVS_MTRIPLE) -filetype=obj -o=$@ -O2 $<

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

#XXX(toshok) same as node-compat's Makefile - we need to install all targets to their respective archlibdirs
install-local::
	@$(MKDIR) $(includedir)/runtime
	@$(MKDIR) $(archlibdir)
	$(INSTALL) -c $(ALL_LIBRARIES) $(archlibdir)
	@for i in *.h; do \
	  $(INSTALL) -c $$i $(includedir)/runtime; \
	done

clean-local::
	rm -f test $(ALL_OBJECTS) $(ALL_LIBRARIES) ejs-atoms-gen.c $(analyze_plists_c) $(analyze_plists_objc)

include $(TOP)/build/build.mk
