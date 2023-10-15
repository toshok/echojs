TOP=..

include $(TOP)/mk/config.mk

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
	main.c \
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
C_SOURCES += ejs-runloop-noop.c
else
RUNLOOP_DEF=-DHAVE_LIBUV=1
C_SOURCES += =ejs-runloop-libuv.c
endif

LINUX_OBJECTS=$(C_SOURCES:%.c=%.o.linux) $(CPP_SOURCES:%.cpp=%.o.linux) $(OBJC_SOURCES:%.m=%.o.linux) ejs-log.o.linux ejs-invoke-closure-catch.o.linux

ALL_OBJECTS=$(LINUX_OBJECTS)

LINUX_CFLAGS += -I/usr/include/libunwind -I../external-deps/pcre-linux -I../external-deps/double-conversion

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

MACOS_CFLAGS  += -I../external-deps/pcre-macos  -I../external-deps/double-conversion
IOSSIM_CFLAGS += -I../external-deps/pcre-iossim -I../external-deps/double-conversion
IOSDEV_CFLAGS += -I../external-deps/pcre-iosdev -I../external-deps/double-conversion


ejs-webgl-constants-sorted.h: ejs-webgl-constants.h
	@echo [GEN] $@ && (grep WEBGL_CONSTANT $< | sort > $@)

DARWIN_OBJC_FLAGS= -ObjC -DOBJC=1 -fobjc-abi-version=2 -fobjc-legacy-dispatch -fno-objc-arc
MACOS_OBJC_FLAGS=  $(DARWIN_OBJC_FLAGS)
IOSSIM_OBJC_FLAGS= $(DARWIN_OBJC_FLAGS)
IOSDEV_OBJC_FLAGS= $(DARWIN_OBJC_FLAGS)

PREFIXES=MACOS IOSSIM IOSDEV

INCLUDE=-include

ALL_OBJECTS=

define PREFIX_rules
$(1)_lowered=$(shell echo $(1) | tr '[:upper:]' '[:lower:]')
$(1)_LIBRARY=out/$($(1)_SHORT_TRIPLE)/$(LIBRARY)
$(1)_OBJECTS=$(C_SOURCES:%.c=out/$($(1)_SHORT_TRIPLE)/%.o) $(CPP_SOURCES:%.cpp=out/$($(1)_SHORT_TRIPLE)/%.o) $(OBJC_SOURCES:%.m=out/$($(1)_SHORT_TRIPLE)/%.o) out/$($(1)_SHORT_TRIPLE)/ejs-invoke-closure-catch.o

ALL_OBJECTS += $$($(1)_OBJECTS)

$$($(1)_LIBRARY): $$($(1)_OBJECTS)
	@echo [ar $(shell echo $(1) | tr '[:upper:]' '[:lower:]')] `basename $$@` && /usr/bin/ar rc $$@ $$($(1)_OBJECTS)

out/$($(1)_SHORT_TRIPLE)/%.o: %.ll
	@mkdir -p .deps/out/$$($(1)_SHORT_TRIPLE)
	@mkdir -p out/$$($(1)_SHORT_TRIPLE)
	@echo [llc $$($(1)_lowered)] $$< && llc$$(LLVM_SUFFIX) -march=$($(1)_MARCH) -mtriple=$($(1)_MTRIPLE) -filetype=obj -o=$$@ -O2 $$<

out/$($(1)_SHORT_TRIPLE)/%.o: %.c
	@mkdir -p .deps/out/$($(1)_SHORT_TRIPLE)
	@mkdir -p out/$($(1)_SHORT_TRIPLE)
	@$(CC) -MM $($(1)_CFLAGS) $$< | sed -e s,`basename $$@`,$$@, > .deps/$$@-deps
	@echo [$(CC) $$($(1)_lowered)] $$< && $(CC) -ObjC $($(1)_CFLAGS)  -c -o $$@ $$<

out/$($(1)_SHORT_TRIPLE)/%.o: %.cpp
	@mkdir -p .deps/out/$($(1)_SHORT_TRIPLE)
	@mkdir -p out/$($(1)_SHORT_TRIPLE)
	@$(CXX) -MM $($(1)_CFLAGS) $$< | sed -e s,`basename $$@`,$$@, > .deps/$$@-deps
	@echo [$(CXX) $$($(1)_lowered)] $$< && $(CXX) -std=c++11 $($(1)_CFLAGS)  -c -o $$@ $$<

out/$($(1)_SHORT_TRIPLE)/%.o: %.m
	@mkdir -p .deps/out/$($(1)_SHORT_TRIPLE)
	@mkdir -p out/$($(1)_SHORT_TRIPLE)
	$(CC) -MM $($(1)_OBJC_FLAGS) $($(1)_CFLAGS) $$< | sed -e s,`basename $$@`,$$@, > .deps/$$@-deps
	@echo [$(CC) $$($(1)_lowered)] $$< && $(CC) $($(1)_OBJC_FLAGS) $($(1)_CFLAGS) -c -o $$@ $$<

out/$($(1)_SHORT_TRIPLE)/ejs-webgl.o: ejs-webgl-constants-sorted.h

out/$($(1)_SHORT_TRIPLE)/ejs-init.o: ejs-atoms-gen.c
endef

$(foreach prefix,$(PREFIXES),$(eval $(call PREFIX_rules,$(prefix))))

-include $(patsubst out/$(MACOS_SHORT_TRIPLE)/%.o,.deps/out/$(MACOS_SHORT_TRIPLE)/%.o-deps,$(MACOS_OBJECTS))
-include $(patsubst out/$(IOSSIM_SHORT_TRIPLE)/%.o,.deps/out/$(IOSSIM_SHORT_TRIPLE)/%.o-deps,$(IOSSIM_OBJECTS))
-include $(patsubst out/$(IOSDEV_SHORT_TRIPLE)/%.o,.deps/out/$(IOSDEV_SHORT_TRIPLE)/%.o-deps,$(IOSDEV_OBJECTS))

ifneq ($(CIRCLE_BUILD_NUM),)
# on circleci we only build the macos library
ALL_LIBRARIES=$(MACOS_LIBRARY)
else
ALL_LIBRARIES=$(MACOS_LIBRARY) $(IOSSIM_LIBRARY) $(IOSDEV_LIBRARY)
endif

ALL_TARGETS=$(ALL_LIBRARIES)

analyze_plists = $(C_SOURCES:%.c=out/analysis/%.plist) $(CPP_SOURCES:%.cpp=out/analysis/%.plist) $(OBJC_SOURCES:%.m=out/analysis/%.plist)
analyze:: $(analyze_plists)

out/analysis/%.plist: %.c
	@mkdir -p out/analysis
	@echo [$(CC) analyze] $< && $(CC) $(MACOS_CFLAGS) --analyze $< -o $@

out/analysis/%.plist: %.cpp
	@mkdir -p out/analysis
	@echo [$(CXX) analyze] $< && $(CXX) $(MACOS_CFLAGS) --analyze $< -o $@

out/analysis/%.plist: %.m
	@mkdir -p out/analysis
	@echo [$(CC) analyze] $< && $(CC) $(MACOS_CFLAGS) --analyze $< -o $@
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
	rm -f test $(ALL_OBJECTS) $(ALL_LIBRARIES) ejs-atoms-gen.c $(analyze_plists)

include $(TOP)/mk/build.mk
