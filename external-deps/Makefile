TOP=..

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static

PCRE_CONFIGURE_ARGS=--enable-pcre16 --enable-utf

CFLAGS=-I$(TOP)/runtime

all-local:: build-llvm build-pcre

clean-local:: clean-llvm clean-pcre

configure-pcre: configure-pcre-iossim configure-pcre-iosdev configure-pcre-osx

configure-pcre-osx:
	@mkdir -p pcre-osx
	(cd pcre-osx && \
	../$(TOP)/pcre/configure $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-osx

configure-pcre-iossim:
	@mkdir -p pcre-iossim
	(cd pcre-iossim && \
	PATH=$(IOSSIM_ROOT)/usr/bin:$$PATH \
	CC="$(IOSSIM_ROOT)/usr/bin/gcc $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	CXX="$(IOSSIM_ROOT)/usr/bin/g++ $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	LD="$(IOSSIM_ROOT)/usr/bin/gcc" \
	AR="$(IOSSIM_ROOT)/usr/bin/ar" \
	AS="$(IOSSIM_ROOT)/usr/bin/as" \
	../$(TOP)/pcre/configure --host=$(IOSSIM_TRIPLE) $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-iossim

configure-pcre-iosdev:
	@mkdir -p pcre-iosdev
	(cd pcre-iosdev && \
	PATH=$(IOSDEV_ROOT)/usr/bin:$$PATH \
	CC="$(IOSDEV_ROOT)/usr/bin/gcc $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	CXX="$(IOSDEV_ROOT)/usr/bin/g++ $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	LD="$(IOSDEV_ROOT)/usr/bin/gcc" \
	AR="$(IOSDEV_ROOT)/usr/bin/ar" \
	AS="$(IOSDEV_ROOT)/usr/bin/as" \
	../$(TOP)/pcre/configure --host=$(IOSDEV_TRIPLE) $(PCRE_CONFIGURE_ARGS)) || rm -rf pcre-iosdev

build-pcre-osx: configure-pcre-osx
	$(MAKE) -C pcre-osx pcre_chartables.c libpcre16.la

build-pcre-iossim: configure-pcre-iossim
	$(MAKE) -C pcre-iossim pcre_chartables.c libpcre16.la

build-pcre-iosdev: configure-pcre-iosdev
	$(MAKE) -C pcre-iosdev pcre_chartables.c libpcre16.la

build-pcre: build-pcre-osx build-pcre-iossim build-pcre-iosdev

install-pcre:
	$(MAKE) -C pcre install

clean-pcre:
	@test -d pcre-osx && $(MAKE) -C pcre-osx clean
	@test -d pcre-iossim && $(MAKE) -C pcre-iossim clean
	@test -d pcre-iosdev && $(MAKE) -C pcre-iosdev clean


configure-llvm:
	@mkdir -p llvm-osx
	(cd llvm-osx && \
	../$(TOP)/llvm/configure  $(LLVM_CONFIGURE_ARGS)) || rm -rf llvm-osx

build-llvm: configure-llvm
	$(MAKE) -C llvm-osx

install-llvm:

clean-llvm:
	@test -d llvm-osx && $(MAKE) -C llvm-osx clean

include $(TOP)/build/build.mk
