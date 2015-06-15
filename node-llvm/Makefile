TOP=..

-include $(TOP)/build/config.mk

LLVM_CONFIG=llvm-config$(LLVM_SUFFIX)

LLVM_CXXFLAGS   := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_CPPFLAGS   := $(shell $(LLVM_CONFIG) --cppflags)
LLVM_INCLUDEDIR := $(shell $(LLVM_CONFIG) --includedir)

LLVM_CXXFLAGS   := $(subst $(LLVM_CPPFLAGS),,$(LLVM_CXXFLAGS))
LLVM_DEFINES    := $(subst -I$(LLVM_INCLUDEDIR),,$(LLVM_CPPFLAGS))
LLVM_DEFINES    := $(subst -D,,$(LLVM_DEFINES))

LLVM_LINKFLAGS  := $(shell $(LLVM_CONFIG) --ldflags --libs)

MIN_OSX_VERSION?=10.9

all-local:: build

build: configure
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_INCLUDEDIR="$(LLVM_INCLUDEDIR)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" LLVM_DEFINES="$(LLVM_DEFINES)" MIN_OSX_VERSION="$(MIN_OSX_VERSION)" node-gyp build

configure:
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_INCLUDEDIR="$(LLVM_INCLUDEDIR)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" LLVM_DEFINES="$(LLVM_DEFINES)" MIN_OSX_VERSION="$(MIN_OSX_VERSION)" node-gyp configure

clean-local::
	node-gyp clean

-include $(TOP)/build/build.mk
