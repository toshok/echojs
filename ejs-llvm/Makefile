TOP=..

include $(TOP)/build/config.mk

SOURCES= \
	allocainst.cpp \
	arraytype.cpp \
	basicblock.cpp \
	callinvoke.cpp \
	constant.cpp \
	constantarray.cpp \
	constantfp.cpp \
	ejs-llvm.cpp \
	function.cpp \
	functiontype.cpp \
	globalvariable.cpp \
	irbuilder.cpp \
	landingpad.cpp \
	loadinst.cpp \
	module.cpp \
	structtype.cpp \
	switch.cpp \
	type.cpp \
	value.cpp


CXX=clang++

LLVM_CONFIG=llvm-config$(LLVM_SUFFIX)

LLVM_CXXFLAGS   := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_INCLUDEDIR := $(shell $(LLVM_CONFIG) --includedir)

LLVM_CXXFLAGS   := $(subst $(LLVM_CPPFLAGS),,$(LLVM_CXXFLAGS))
LLVM_DEFINES    := $(subst -I$(LLVM_INCLUDEDIR),,$(LLVM_CPPFLAGS))

CXXFLAGS=-I../runtime -I$(LLVM_INCLUDEDIR) $(LLVM_CXXFLAGS) $(OSX_CFLAGS)

OBJECTS=$(SOURCES:%.cpp=%.o)

MODULE=libejsllvm-module.a

all-local:: $(MODULE)

$(MODULE): $(OBJECTS)
	ar cru $@ $(OBJECTS)

$(OBJECTS): %.o: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(CXXFLAGS) $< > .deps/$@-deps
	@echo [$(CXX)] $< && $(CXX) $(CXXFLAGS) -c $< -o $@

clean-local::
	rm -f $(OBJECTS) $(MODULE) ejs-llvm-atoms-gen.c

ejs-llvm-atoms-gen.c: ejs-llvm-atoms.h $(TOP)/runtime/gen-atoms.js
	@echo [GEN] $@ && $(TOP)/runtime/gen-atoms.js $< > .tmp-$@ && mv .tmp-$@ $@

ejs-llvm.o: ejs-llvm-atoms-gen.c

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))

include $(TOP)/build/build.mk
