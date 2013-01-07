SOURCES=type.cpp \
	functiontype.cpp \
	structtype.cpp \
	ejs-llvm.cpp

CXX=clang++
CXXFLAGS=-I../runtime -I/usr/local/include -g -D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fno-exceptions -fno-rtti -fno-common -Woverloaded-virtual -Wcast-qual -fPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I..


OBJECTS=$(SOURCES:%.cpp=%.o)

libejsllvm-module.a: $(OBJECTS)
	ar cru $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) libejsllvm-module.a
