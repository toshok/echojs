SOURCES= \
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
	module.cpp \
	structtype.cpp \
	switch.cpp \
	type.cpp \
	value.cpp


CXX=clang++
CXXFLAGS=-I../runtime -I/usr/local/include -g -D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fno-exceptions -fno-rtti -fno-common -Woverloaded-virtual -Wcast-qual -fPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I..


OBJECTS=$(SOURCES:%.cpp=%.o)

libejsllvm-module.a: $(OBJECTS)
	ar cru $@ $(OBJECTS)

$(OBJECTS): %.o: %.cpp
	@mkdir -p .deps
	@$(CXX) -MM $(CXXFLAGS) $< > .deps/$@-deps
	@echo [$(CXX)] $< && $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) libejsllvm-module.a

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))
