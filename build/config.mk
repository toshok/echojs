PRODUCT_NAME=EchoJS
PRODUCT_VERSION=0.1

PRODUCT_RELEASE_NOTES_URL=http://toshokelectric.com/echojs/release_notes
PRODUCT_GITHUB_URL=https://github.com/toshok/echo-js
PRODUCT_EMAIL=toshok@toshokelectric.com
ORGANIZATION=com.toshokelectric


PRODUCT_name:=$(shell echo $(PRODUCT_NAME) | tr [:upper:] [:lower:])

PRODUCT_UTI=$(ORGANIZATION).$(PRODUCT_NAME)

# the place where we stuff everything
PRODUCT_INSTALL_ROOT=/Library/Frameworks/$(PRODUCT_NAME).framework

# in the future this should really be the value for DIST_ROOT, so we can build packages without trashing what's in PRODUCT_INSTALL_ROOT
#
#  DIST_ROOT=$(TOP)/dist
#
# but until then...
DIST_ROOT=$(PRODUCT_INSTALL_ROOT)

MKDIR=mkdir -p
INSTALL=install
CP=cp

CFLAGS=-g -O0 -Wall -I. -Wno-unused-function

MIN_IOS_VERSION=7.0

DEVELOPER_ROOT?=/Applications/Xcode.app/Contents/Developer
IOS_SDK_VERSION?=7.0

OSX_ARCH=-arch x86_64
OSX_CFLAGS=$(CFLAGS) -DOSX=1

IOSSIM_ARCH=-arch i386
IOSSIM_TRIPLE=i386-apple-darwin
IOSSIM_ARCH_FLAGS=
IOSSIM_ROOT=$(DEVELOPER_ROOT)/Platforms/iPhoneSimulator.platform/Developer
IOSSIM_BIN=$(IOSSIM_ROOT)/usr/bin
IOSSIM_SYSROOT=$(IOSSIM_ROOT)/SDKs/iPhoneSimulator$(IOS_SDK_VERSION).sdk

IOSDEV_ARCH=-arch armv7
IOSDEV_TRIPLE=armv7-apple-darwin
IOSDEV_ARCH_FLAGS=-mthumb -std=gnu99
IOSDEV_ROOT=$(DEVELOPER_ROOT)/Platforms/iPhoneOS.platform/Developer
IOSDEV_BIN=$(IOSDEV_ROOT)/usr/bin
IOSDEV_SYSROOT=$(IOSDEV_ROOT)/SDKs/iPhoneOS$(IOS_SDK_VERSION).sdk 

IOSSIM_CFLAGS=$(IOSSIM_ARCH) $(IOSSIM_ARCH_FLAGS) $(CFLAGS) -DIOS=1 -isysroot $(IOSSIM_SYSROOT) -miphoneos-version-min=$(MIN_IOS_VERSION)
IOSDEV_CFLAGS=$(IOSDEV_ARCH) $(IOSDEV_ARCH_FLAGS) $(CFLAGS) -DIOS=1 -isysroot $(IOSDEV_SYSROOT) -miphoneos-version-min=$(MIN_IOS_VERSION)

-include $(TOP)/build/config-local.mk
