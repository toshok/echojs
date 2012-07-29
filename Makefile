TOPDIR=$(shell pwd)
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/esprima:$(TOPDIR)/lib

NJS_SHELL=/Users/toshok/src/coffeekit/coffeekit-internal/external-deps/spidermonkey-osx/js
LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"

CFLAGS=-Iruntime

all: build-llvm build-node-llvm build-lib

test:
	$(MAKE) -C test check

# because we have a test directory..
.PHONY: test

LLVM_CONFIGURE_ARGS=--disable-jit --enable-static
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