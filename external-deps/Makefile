TOP=..
include $(TOP)/build/config.mk

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static --enable-optimized --disable-assertions

PCRE_CONFIGURE_ARGS=--enable-pcre16 --enable-utf --disable-cpp

CFLAGS=-I$(TOP)/runtime

all-local:: build-pcre build-double-conversion

clean-local:: clean-pcre clean-double-conversion

install-local:: install-pcre install-double-conversion

ifeq ($(HOST_OS),linux)
_TARGETS=linux
else
ifneq ($(TRAVIS_BUILD_NUMBER),)
_TARGETS=osx
else
_TARGETS=iossim iosdev osx
endif
endif

build-double-conversion: $(_TARGETS:%=build-double-conversion-%)

clean-double-conversion: $(_TARGETS:%=clean-double-conversion-%)

.stamp-configure-double-conversion-linux: double-conversion/CMakeLists.txt
	@$(MKDIR) double-conversion-linux
	(cd double-conversion-linux && cmake ../double-conversion) && touch $@

.stamp-build-double-conversion-linux: .stamp-configure-double-conversion-linux
	$(MAKE) -C double-conversion-linux && touch $@

.stamp-configure-double-conversion-osx: double-conversion/CMakeLists.txt
	@$(MKDIR) double-conversion-osx
	(cd double-conversion-osx && cmake ../double-conversion) && touch $@

.stamp-configure-double-conversion-iossim: double-conversion/CMakeLists.txt
	@$(MKDIR) double-conversion-iossim
	(cd double-conversion-iossim && \
	cmake ../double-conversion -DCMAKE_TOOLCHAIN_FILE=../../build/iOS.cmake -DIOS_PLATFORM=SIMULATOR -DMIN_IOS_VERSION=$(MIN_IOS_VERSION) -DIOS_SYSROOT=$(IOSSIM_SYSROOT)) && touch $@

.stamp-configure-double-conversion-iosdev: double-conversion/CMakeLists.txt
	@$(MKDIR) double-conversion-iosdev
	(cd double-conversion-iosdev && \
	cmake ../double-conversion -DCMAKE_TOOLCHAIN_FILE=../../build/iOS.cmake -DIOS_PLATFORM=OS -DMIN_IOS_VERSION=$(MIN_IOS_VERSION) -DIOS_SYSROOT=$(IOSDEV_SYSROOT)) && touch $@

.stamp-build-double-conversion-osx: .stamp-configure-double-conversion-osx
	$(MAKE) -C double-conversion-osx && touch $@

.stamp-build-double-conversion-iossim: .stamp-configure-double-conversion-iossim
	$(MAKE) -C double-conversion-iossim && touch $@

.stamp-build-double-conversion-iosdev: .stamp-configure-double-conversion-iosdev
	$(MAKE) -C double-conversion-iosdev && touch $@


build-double-conversion-linux: .stamp-build-double-conversion-linux
build-double-conversion-osx: .stamp-build-double-conversion-osx
build-double-conversion-iossim: .stamp-build-double-conversion-iossim
build-double-conversion-iosdev: .stamp-build-double-conversion-iosdev


clean-double-conversion-iossim:
	-@test -d double-conversion-iossim && $(MAKE) -C double-conversion-iossim clean
	@rm -f .stamp-build-double-conversion-iossim

clean-double-conversion-iosdev:
	-@test -d double-conversion-iosdev && $(MAKE) -C double-conversion-iosdev clean
	@rm -f .stamp-build-double-conversion-iosdev

clean-double-conversion-osx:
	-@test -d double-conversion-osx    && $(MAKE) -C double-conversion-osx clean
	@rm -f .stamp-build-double-conversion-osx

clean-double-conversion-linux:
	-@test -d double-conversion-linux  && $(MAKE) -C double-conversion-linux clean
	@rm -f .stamp-build-double-conversion-linux

build-pcre: $(_TARGETS:%=build-pcre-%)

clean-pcre: $(_TARGETS:%=clean-pcre-%)

.stamp-configure-pcre-linux: pcre/configure
	@$(MKDIR) pcre-linux
	(cd pcre-linux && \
	../pcre/configure $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-osx: pcre/configure
	@$(MKDIR) pcre-osx
	(cd pcre-osx && \
	../pcre/configure $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-iossim: pcre/configure
	@$(MKDIR) pcre-iossim
	(cd pcre-iossim && \
	PATH=$(IOSSIM_ROOT)/usr/bin:$$PATH \
	CC="clang $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	CXX="clang++ $(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSSIM_SYSROOT)" \
	LD="clang" \
	AS="$(IOSSIM_ROOT)/usr/bin/as" \
	../pcre/configure --host=$(IOSSIM_TRIPLE) $(PCRE_CONFIGURE_ARGS)) && touch $@

.stamp-configure-pcre-iosdev: pcre/configure
	@$(MKDIR) pcre-iosdev
	(cd pcre-iosdev && \
	PATH=$(IOSDEV_ROOT)/usr/bin:$$PATH \
	CC="clang $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	CXX="clang++ $(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) -miphoneos-version-min=$(MIN_IOS_VERSION) -isysroot $(IOSDEV_SYSROOT)" \
	LD="clang" \
	AS="$(IOSDEV_ROOT)/usr/bin/as" \
	../pcre/configure --host=$(IOSDEV_TRIPLE) $(PCRE_CONFIGURE_ARGS)) && touch $@

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
