TOP=..

include $(TOP)/build/config.mk

LIBRARY=libejsnodecompat-module.a
C_SOURCES= \
	ejs-node-compat.c

CFLAGS += -I../runtime

ejs-atoms-gen.c: ejs-atoms.h gen-atoms.js
	@echo [GEN] $@ && ./gen-atoms.js $< > .tmp-$@ && mv .tmp-$@ $@

ifeq ($(HOST_OS),linux)
ALL_LIBRARIES=$(LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)

LINUX_OBJECTS=$(C_SOURCES:%.c=%.o.linux) $(OBJC_SOURCES:%.m=%.o.linux)

ALL_OBJECTS=$(LINUX_OBJECTS)

#ejs-init.o.linux: ejs-atoms-gen.c

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

OBJC_SOURCES=

OSX_OBJECTS=$(C_SOURCES:%.c=%.o.osx) $(OBJC_SOURCES:%.m=%.o.osx)
SIM_OBJECTS=$(C_SOURCES:%.c=%.o.sim) $(OBJC_SOURCES:%.m=%.o.sim)
DEV_OBJECTS=$(C_SOURCES:%.c=%.o.armv7) $(OBJC_SOURCES:%.m=%.o.armv7)
DEVS_OBJECTS=$(C_SOURCES:%.c=%.o.armv7s) $(OBJC_SOURCES:%.m=%.o.armv7s)

analyze_plists_c = $(C_SOURCES:%.c=%.plist)
analyze_plists_objc = $(OBJC_SOURCES:%.m=%.plist) 

OSX_LIBRARY=$(LIBRARY)
SIM_LIBRARY=$(LIBRARY).sim
DEV_LIBRARY=$(LIBRARY).armv7
DEVS_LIBRARY=$(LIBRARY).armv7s

ifneq ($(TRAVIS_BUILD_NUMBER),)
ALL_LIBRARIES=$(OSX_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES)
else
ALL_LIBRARIES=$(OSX_LIBRARY) $(SIM_LIBRARY) $(DEV_LIBRARY) $(DEVS_LIBRARY)
ALL_TARGETS=$(ALL_LIBRARIES) $(analyze_plists_c) $(analyze_plists_objc)
endif

ALL_OBJECTS=$(SIM_OBJECTS) $(DEV_OBJECTS) $(DEVS_OBJECTS) $(OSX_OBJECTS)

$(OSX_LIBRARY): $(OSX_OBJECTS)
	@echo [ar osx] $@ && /usr/bin/ar rc $@ $(OSX_OBJECTS)

$(SIM_LIBRARY): $(SIM_OBJECTS)
	@echo [ar sim] $@ && /usr/bin/ar rc $@ $(SIM_OBJECTS)

$(DEV_LIBRARY): $(DEV_OBJECTS)
	@echo [ar armv7] $@ && /usr/bin/ar rc $@ $(DEV_OBJECTS)

$(DEVS_LIBRARY): $(DEVS_OBJECTS)
	@echo [ar armv7s] $@ && /usr/bin/ar rc $@ $(DEVS_OBJECTS)

#ejs-init.o.osx ejs-init.o.sim ejs-init.o.armv7 ejs-init.o.armv7s: ejs-atoms-gen.c

OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch -fno-objc-arc

%.o.osx: %.c
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) -ObjC $(OSX_CFLAGS)  -c -o $@ $<

%.o.osx: %.m
	@mkdir -p .deps
	@$(CC) -MM $(OSX_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.osx,,`/$@/ > .deps/$@-deps
	@echo [$(CC) osx] $< && $(CC) $(OSX_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.osx: %.ll
	@echo [llc osx] $< && llc$(LLVM_SUFFIX) -filetype=obj -o=$@ -O2 $<

%.o.sim: %.c
	@mkdir -p .deps
	@$(CC) -MM $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) -ObjC $(IOSSIM_CFLAGS) -c -o $@ $<

%.o.sim: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSSIM_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.sim,,`/$@/ > .deps/$@-deps
	@echo [$(CC) sim] $< && PATH=$(IOSSIM_BIN):$$PATH $(CC) $(IOSSIM_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.sim: %.ll
	@echo [llc sim] $< && llc$(LLVM_SUFFIX) -march=x86 -filetype=obj -o=$@ -O2 $<

%.o.armv7: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) -ObjC $(IOSDEV_CFLAGS) -c -o $@ $<

%.o.armv7: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEV_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEV_BIN):$$PATH $(CC) $(IOSDEV_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7: %.ll
	@echo [llc armv7] $< && llc$(LLVM_SUFFIX) -march=arm -filetype=obj -o=$@ -O2 $<

%.o.armv7s: %.c
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7s] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) -ObjC $(IOSDEVS_CFLAGS) -c -o $@ $<

%.o.armv7s: %.m
	@mkdir -p .deps
	@$(CC) -MM -ObjC $(IOSDEVS_CFLAGS) $< | sed -e s/`echo $@ | sed -e s,.armv7s,,`/$@/ > .deps/$@-deps
	@echo [$(CC) armv7] $< && PATH=$(IOSDEVS_BIN):$$PATH $(CC) $(IOSDEVS_CFLAGS) $(OBJC_FLAGS) -c -o $@ $<

%.o.armv7s: %.ll
	@echo [llc armv7s] $< && llc$(LLVM_SUFFIX) -march=aarch64 -filetype=obj -o=$@ -O2 $<

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

#XXX(toshok) this doesn't work for osx where we want to install multiple libraries to different archlibdirs...
install-local::
	@$(MKDIR) $(libdir)
	@$(MKDIR) $(archlibdir)
	$(INSTALL) -c node-compat.ejs $(libdir)
	$(INSTALL) -c $(LIBRARY) $(archlibdir)

include $(TOP)/build/build.mk
