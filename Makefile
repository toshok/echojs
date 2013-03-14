TOP=$(shell pwd)

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static

PCRE_CONFIGURE_ARGS=--enable-pcre16 --enable-utf

CFLAGS=-Iruntime

all: build-llvm build-pcre build-node-llvm build-lib test

include $(TOP)/build/build.mk

configure-llvm:
	cd llvm && ./configure $(LLVM_CONFIGURE_ARGS)

configure-pcre: configure-pcre-iossim configure-pcre-iosdev configure-pcre-osx

configure-pcre-osx:
	@mkdir -p pcre-osx
	(cd pcre-osx && \
	../pcre/configure $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-osx

configure-pcre-iossim:
	@mkdir -p pcre-iossim
	(cd pcre-iossim && \
	PATH=$(IOSSIM_ROOT)/usr/bin:$$PATH \
	CC="$(IOSSIM_ROOT)/usr/bin/clang $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	CXX="$(IOSSIM_ROOT)/usr/bin/clang++ $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	LD="$(IOSSIM_ROOT)/usr/bin/clang" \
	AR="$(IOSSIM_ROOT)/usr/bin/ar" \
	AS="$(IOSSIM_ROOT)/usr/bin/as" \
	../pcre/configure --host=$(IOSSIM_TRIPLE) $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-iossim

configure-pcre-iosdev:
	@mkdir -p pcre-iosdev
	(cd pcre-iosdev && \
	PATH=$(IOSDEV_ROOT)/usr/bin:$$PATH \
	CC="$(IOSDEV_ROOT)/usr/bin/clang $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	CXX="$(IOSDEV_ROOT)/usr/bin/clang++ $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	LD="$(IOSDEV_ROOT)/usr/bin/clang" \
	AR="$(IOSDEV_ROOT)/usr/bin/ar" \
	AS="$(IOSDEV_ROOT)/usr/bin/as" \
	../pcre/configure --host=$(IOSDEV_TRIPLE) $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-iosdev


build-llvm: configure-llvm
	$(MAKE) -C llvm

build-pcre: build-pcre-osx build-pcre-iossim build-pcre-iosdev


build-pcre-osx: configure-pcre-osx
	$(MAKE) -C pcre-osx pcre_chartables.c libpcre16.la

build-pcre-iossim: configure-pcre-iossim
	$(MAKE) -C pcre-iossim pcre_chartables.c libpcre16.la

build-pcre-iosdev: configure-pcre-iosdev
	$(MAKE) -C pcre-iosdev pcre_chartables.c libpcre16.la

install-llvm:

install-pcre:
	$(MAKE) -C pcre install

build-node-llvm:
	$(MAKE) -C node-llvm

build-lib:
	$(MAKE) -C lib

test:
	$(MAKE) -C test check

bootstrap:
	$(MAKE) -C lib/coffee bootstrap

# because we have a test directory..
.PHONY: test

