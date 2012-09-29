SOURCES=type.cpp \
	functiontype.cpp \
	structtype.cpp \
	ejs-llvm.cpp

CXX=clang++
CXXFLAGS=-I../runtime -I/usr/local/include -D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -g -fno-exceptions -fno-rtti -fno-common -Woverloaded-virtual -Wcast-qual

OBJECTS=$(SOURCES:%.cpp=%.o)

libejsllvm-module.a: $(OBJECTS)
	ar cru $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) libejsllvm-module.a
