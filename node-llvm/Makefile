
LLVM_CONFIG=llvm-config$(LLVM_SUFFIX)

LLVM_CXXFLAGS   := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_CPPFLAGS   := $(shell $(LLVM_CONFIG) --cppflags)
LLVM_INCLUDEDIR := $(shell $(LLVM_CONFIG) --includedir)

LLVM_CXXFLAGS   := $(subst $(LLVM_CPPFLAGS),,$(LLVM_CXXFLAGS))
LLVM_DEFINES    := $(subst -I$(LLVM_INCLUDEDIR),,$(LLVM_CPPFLAGS))
LLVM_DEFINES    := $(subst -D,,$(LLVM_DEFINES))

LLVM_LINKFLAGS  := $(shell $(LLVM_CONFIG) --ldflags --libs)

ifneq ($(TRAVIS_BUILD_NUMBER),)
# travis-ci osx machines are 10.8.5, so fix our min version there
MIN_OSX_VERSION=10.8
else
# otherwise use latest, but give the user the opportunity to override
MIN_OSX_VERSION?=10.9
endif

all: build

build: configure
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_INCLUDEDIR="$(LLVM_INCLUDEDIR)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" LLVM_DEFINES="$(LLVM_DEFINES)" MIN_OSX_VERSION="$(MIN_OSX_VERSION)" node-gyp build -v

configure:
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_INCLUDEDIR="$(LLVM_INCLUDEDIR)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" LLVM_DEFINES="$(LLVM_DEFINES)" MIN_OSX_VERSION="$(MIN_OSX_VERSION)" node-gyp configure -v

clean:
	node-gyp clean
