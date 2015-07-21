TOP=..
include $(TOP)/build/config.mk

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static --enable-optimized --disable-assertions

PCRE_CONFIGURE_ARGS=--enable-pcre16 --enable-utf --disable-cpp

CFLAGS=-I$(TOP)/runtime

all-local:: build-pcre

clean-local:: clean-pcre

install-local:: install-pcre

ifeq ($(HOST_OS),linux)
PCRE_TARGETS=linux
else
ifneq ($(TRAVIS_BUILD_NUMBER),)
PCRE_TARGETS=osx
else
PCRE_TARGETS=iossim iosdev osx
endif
endif


build-pcre: $(PCRE_TARGETS:%=build-pcre-%)

clean-pcre: $(PCRE_TARGETS:%=clean-pcre-%)

.stamp-configure-pcre-linux: $(TOP)/pcre/configure
	@$(MKDIR) pcre-linux
	(cd pcre-linux && \
	../$(TOP)/pcre/configure $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-osx: $(TOP)/pcre/configure
	@$(MKDIR) pcre-osx
	(cd pcre-osx && \
	../$(TOP)/pcre/configure $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-iossim: $(TOP)/pcre/configure
	@$(MKDIR) pcre-iossim
	(cd pcre-iossim && \
	PATH=$(IOSSIM_ROOT)/usr/bin:$$PATH \
	CC="clang $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	CXX="clang++ $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	LD="clang" \
	AS="$(IOSSIM_ROOT)/usr/bin/as" \
	../$(TOP)/pcre/configure --host=$(IOSSIM_TRIPLE) $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-iosdev: $(TOP)/pcre/configure
	@$(MKDIR) pcre-iosdev
	(cd pcre-iosdev && \
	PATH=$(IOSDEV_ROOT)/usr/bin:$$PATH \
	CC="clang $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	CXX="clang++ $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	LD="clang" \
	AS="$(IOSDEV_ROOT)/usr/bin/as" \
	../$(TOP)/pcre/configure --host=$(IOSDEV_TRIPLE) $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-build-pcre-linux: .stamp-configure-pcre-linux
	$(MAKE) -C pcre-linux pcre_chartables.c libpcre16.la && touch $@

.stamp-build-pcre-osx: .stamp-configure-pcre-osx
	$(MAKE) -C pcre-osx pcre_chartables.c libpcre16.la && touch $@

.stamp-build-pcre-iossim: .stamp-configure-pcre-iossim
	$(MAKE) -C pcre-iossim pcre_chartables.c libpcre16.la && touch $@

.stamp-build-pcre-iosdev: .stamp-configure-pcre-iosdev
	$(MAKE) -C pcre-iosdev pcre_chartables.c libpcre16.la && touch $@



build-pcre-linux: .stamp-build-pcre-linux
build-pcre-osx: .stamp-build-pcre-osx
build-pcre-iossim: .stamp-build-pcre-iossim
build-pcre-iosdev: .stamp-build-pcre-iosdev


install-pcre-linux: build-pcre-linux
	@$(MKDIR) $(archlibdir)
	$(INSTALL) -c pcre-linux/.libs/libpcre16.a $(archlibdir)

clean-pcre-iossim:
	-@test -d pcre-iossim && $(MAKE) -C pcre-iossim clean
	@rm -f .stamp-build-pcre-iossim

clean-pcre-iosdev:
	-@test -d pcre-iosdev && $(MAKE) -C pcre-iosdev clean
	@rm -f .stamp-build-pcre-iosdev

clean-pcre-osx:
	-@test -d pcre-osx    && $(MAKE) -C pcre-osx clean
	@rm -f .stamp-build-pcre-osx

clean-pcre-linux:
	-@test -d pcre-linux  && $(MAKE) -C pcre-linux clean
	@rm -f .stamp-build-pcre-linux

include $(TOP)/build/build.mk
