
all: build

LLVM_CXXFLAGS:=$(shell llvm-config$(LLVM_SUFFIX) --cxxflags)
LLVM_LINKFLAGS:=$(shell llvm-config$(LLVM_SUFFIX) --ldflags --libs)

build: configure
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" node-waf build

configure:
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" node-waf configure 

clean:
	@LLVM_CXXFLAGS="$(LLVM_CXXFLAGS)" LLVM_LINKFLAGS="$(LLVM_LINKFLAGS)" node-waf clean
