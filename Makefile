TOPDIR=$(shell pwd)

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static

PCRE_CONFIGURE_ARGS=--enable-pcre16 --enable-utf

CFLAGS=-Iruntime

all: build-llvm build-pcre build-node-llvm build-lib test

configure-llvm:
	cd llvm && ./configure $(LLVM_CONFIGURE_ARGS)

configure-pcre:
	cd pcre && ./configure $(PCRE_CONFIGURE_ARGS)

build-llvm: configure-llvm
	$(MAKE) -C llvm

build-pcre: configure-pcre
	$(MAKE) -C pcre

install-llvm:
	$(MAKE) -C llvm install

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
