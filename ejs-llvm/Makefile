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
	dibuilder.cpp \
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

LLVM_LINK_FLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs)

ifeq ($(HOST_OS),darwin)
LLVM_LINK_FLAGS := $(LLVM_LINK_FLAGS) -lcurses
endif

CXXFLAGS=-I../runtime -I$(LLVM_INCLUDEDIR) $(LLVM_CXXFLAGS) $(OSX_CFLAGS)

CFLAGS += -Wno-c99-extensions -Wno-gnu-statement-expression

OBJECTS=$(SOURCES:%.cpp=%.o)

MODULE=libejsllvm-module.a

all-local:: ejs-llvm.ejs $(MODULE)

$(MODULE): $(OBJECTS)
	ar cru $@ $(OBJECTS)

$(OBJECTS): %.o: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(CXXFLAGS) $< > .deps/$@-deps
	@echo [$(CXX)] $< && $(CXX) $(CXXFLAGS) -c $< -o $@

ejs-llvm.ejs: ejs-llvm.ejs.in
	@echo [gen] $@ && sed -e "s%@EJS_VERSION@%$(PRODUCT_VERSION)%" -e "s%@LLVM_LINK_FLAGS@%$(LLVM_LINK_FLAGS)%" $< > $@

install-local::
	@$(MKDIR) $(libdir)
	@$(MKDIR) $(archlibdir)
	$(INSTALL) -c ejs-llvm.ejs $(libdir)
	$(INSTALL) -c $(MODULE) $(archlibdir)

clean-local::
	rm -f $(OBJECTS) $(MODULE) ejs-llvm-atoms-gen.c

ejs-llvm-atoms-gen.c: ejs-llvm-atoms.h $(TOP)/runtime/gen-atoms.js
	@echo [GEN] $@ && $(TOP)/runtime/gen-atoms.js $< > .tmp-$@ && mv .tmp-$@ $@

ejs-llvm.o: ejs-llvm-atoms-gen.c

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))

include $(TOP)/build/build.mk
