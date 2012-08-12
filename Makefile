TOPDIR=$(shell pwd)

LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"
LLVM_CONFIGURE_ARGS=--disable-jit --enable-static

CFLAGS=-Iruntime

all: build-llvm build-node-llvm build-lib test

configure-llvm:
	cd llvm && ./configure $(LLVM_CONFIGURE_ARGS)

build-llvm: configure-llvm
	$(MAKE) -C llvm

install-llvm:
	$(MAKE) -C llvm install

build-node-llvm:
	$(MAKE) -C node-llvm

build-lib:
	$(MAKE) -C lib

test:
	$(MAKE) -C test check

# because we have a test directory..
.PHONY: test
