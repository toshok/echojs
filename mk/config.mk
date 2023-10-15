# we need this line or else default 'make' behavior will only generate host-config.mk
do-make-all: all

$(TOP)/build/host-config.mk: $(TOP)/build/config.guess
	@(host_triple=`$(TOP)/build/config.guess`; \
	  echo HOST_TRIPLE:=$$host_triple > $@; \
	  echo $$host_triple | awk '{split($$0,a,"-"); print "HOST_CPU:=" a[1] "\nHOST_VENDOR:=" a[2] "\nHOST_OS:=" a[3] "\n"}' >> $@)

-include $(TOP)/build/host-config.mk

LLVM_SUFFIX?=-16.0.6

# we don't care about the version here
HOST_OS:=$(patsubst darwin%,darwin,$(HOST_OS))

PRODUCT_NAME=EchoJS
PRODUCT_VERSION=0.1.0

PRODUCT_RELEASE_NOTES_URL=http://toshokelectric.com/echojs/release_notes
PRODUCT_GITHUB_URL=https://github.com/toshok/echo-js
PRODUCT_EMAIL=toshok@toshokelectric.com
ORGANIZATION=com.toshokelectric


PRODUCT_name:=$(shell echo $(PRODUCT_NAME) | tr [:upper:] [:lower:])

PRODUCT_UTI=$(ORGANIZATION).$(PRODUCT_NAME)

# the place where we stuff everything
PRODUCT_INSTALL_ROOT=/Library/Frameworks/$(PRODUCT_NAME).framework

MKDIR=mkdir -p
INSTALL=install
CP=cp
CC?=clang
CXX?=clang++

CFLAGS=-g -O0 -Wall -I. -Wno-unused-function -Wno-unused-variable

MIN_IOS_VERSION=17.0
MIN_OSX_VERSION=10.10

DEVELOPER_ROOT?=/Applications/Xcode.app/Contents/Developer
IOS_SDK_VERSION?=8.3

ifeq ($(HOST_OS),linux)
EJS_RUNLOOP_IMPL?=libuv
else
EJS_RUNLOOP_IMPL=darwin
endif

LINUX_ARCH=-arch x86_64
LINUX_CLANG_TRIPLE=x86_64-unknown-linux
LINUX_GNU_TRIPLE=x86_64-unknown-linux
LINUX_SHORT_TRIPLE=x86_64-linux
LINUX_CFLAGS=$(CFLAGS) -DTARGET_CPU_AMD64=1 -DEJS_BITS_PER_WORD=64 -DIS_LITTLE_ENDIAN=1 -D_GNU_SOURCE

MACOS_MARCH=arm64
MACOS_ARCH=-arch $(MACOS_MARCH)
MACOS_CLANG_TRIPLE=arm64-apple-macos
MACOS_GNU_TRIPLE=arm64-apple-darwin
MACOS_SHORT_TRIPLE=arm64-macos
MACOS_MTRIPLE="arm64-apple-macosx$(MIN_OSX_VERSION).0"
MACOS_CFLAGS=$(CFLAGS) -DOSX=1 -DTARGET_CPU_ARM64=1 -DEJS_BITS_PER_WORD=64 -DIS_LITTLE_ENDIAN=1 -D_XOPEN_SOURCE -Wno-deprecated-declarations

IOSSIM_MARCH=arm64
IOSSIM_ARCH=-arch $(IOSSIM_MARCH)
IOSSIM_CLANG_TRIPLE=arm64-apple-ios-simulator
IOSSIM_GNU_TRIPLE=aarch64-apple-iossimulator
IOSSIM_SHORT_TRIPLE=arm64-iossim
IOSSIM_MTRIPLE="x86_64-apple-ios$(MIN_IOS_VERSION).0"
IOSSIM_ARCH_FLAGS=-DTARGET_CPU_ARM64=1 -DEJS_BITS_PER_WORD=64 -DIS_LITTLE_ENDIAN=1
IOSSIM_ROOT=$(DEVELOPER_ROOT)/Platforms/iPhoneSimulator.platform/Developer
IOSSIM_BIN=$(IOSSIM_ROOT)/usr/bin
IOSSIM_SYSROOT=$(IOSSIM_ROOT)/SDKs/iPhoneSimulator$(IOS_SDK_VERSION).sdk

IOSDEV_MARCH=arm64
IOSDEV_ARCH=-arch $(IOSDEV_MARCH)
IOSDEV_CLANG_TRIPLE=arm64-apple-ios
IOSDEV_GNU_TRIPLE=aarch64-apple-ios
IOSDEV_SHORT_TRIPLE=arm64-ios
IOSDEV_MTRIPLE="arm64-apple-ios$(MIN_IOS_VERSION).0"
IOSDEV_ARCH_FLAGS=-DTARGET_CPU_ARM64=1 -DEJS_BITS_PER_WORD=64 -DIS_LITTLE_ENDIAN=1
IOSDEV_ROOT=$(DEVELOPER_ROOT)/Platforms/iPhoneOS.platform/Developer
IOSDEV_BIN=$(IOSDEV_ROOT)/usr/bin
IOSDEV_SYSROOT=$(IOSDEV_ROOT)/SDKs/iPhoneOS$(IOS_SDK_VERSION).sdk 

IOSSIM_CFLAGS=$(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) $(CFLAGS) -DIOS=1 -isysroot $(IOSSIM_SYSROOT) -miphoneos-version-min=$(MIN_IOS_VERSION) -D_XOPEN_SOURCE -Wno-deprecated-declarations
IOSDEV_CFLAGS=$(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) $(CFLAGS) -DIOS=1 -isysroot $(IOSDEV_SYSROOT) -miphoneos-version-min=$(MIN_IOS_VERSION) -D_XOPEN_SOURCE -Wno-deprecated-declarations

# directories used during make install
prefix?=/usr/local

bindir:=$(DESTDIR)$(prefix)/bin
includedir:=$(DESTDIR)$(prefix)/include
libdir:=$(DESTDIR)$(prefix)/lib
archlibdir:=$(libdir)/$(HOST_CPU)-$(HOST_OS)

-include $(TOP)/build/config-local.mk
